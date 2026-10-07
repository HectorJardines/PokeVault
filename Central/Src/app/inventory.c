#include "../../Inc/app/inventory.h"
#include "../../Inc/drivers/py25q_flash.h"
// #include "../../Inc/common/printf-stdarg.h"
#include <stdio.h>
#include "../../Inc/app/client.h"
#include "../../../Core/Inc/common/ring_buffer.h"
#include "../../Inc/common/defines.h"
#include "../../Inc/common/log.h"
#include "../../Inc/app/rfid_tag.h"
#include "../../Inc/app/display.h"
#include "../../Inc/drivers/spi.h"
#include "../../Inc/drivers/io.h"

#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"


/********************
 * MACROS
 ***************/
#define FLASH_ADDR_FROM_NODE_ID(base,id)    ((base) + ((id) * sizeof(struct storage_node)))
#define FLASH_ADDR_FROM_ITEM_ID(base,id)    ((base) + ((id) * sizeof(struct flash_item_dict_entry)))
#define CONDITION_STR_TO_U8(str)            (!strcmp(str, "NM") ? 0 : !strcmp(str, "LP") ? 1 : !strcmp(str, "MP") ? 2 : !strcmp(str, "HP") ? 3 : 4)
#define U8_TO_CONDITION_STR(u8)             ((u8) == 0 ? "NM" : (u8) == 1 ? "LP" : (u8) == 2 ? "MP" : (u8) == 3 ? "HP" : "DMG")

#define MURMURHASH_SEED         (0xDEADBEEFU)
#define PG_SIZE                 (256U)
#define SECTOR_SIZE             (4096U)
#define SECTORS_PER_BLOCK       (16U)
#define BLOCK_SIZE              (SECTOR_SIZE * SECTORS_PER_BLOCK)
#define NUM_BLOCKS              (128U)
#define FLASH_SIZE              (BLOCK_SIZE * NUM_BLOCKS)
#define INVALID_FLASH_ADDR      (0xFFFFFFFFU)

#define FLASH_METADATA_AD_BASE      (0x0000U)
#define FLASH_METADA_SZ             (SECTOR_SIZE)

#define FLASH_JOURNAL_AD_BASE       (FLASH_METADATA_AD_BASE + FLASH_METADA_SZ)
#define FLASH_JOURNAL_SZ            (BLOCK_SIZE)

#define FLASH_INVENT_SNAP_0         (FLASH_JOURNAL_AD_BASE + FLASH_JOURNAL_SZ)
#define FLASH_INVENT_SNAP0_SZ       (BLOCK_SIZE)

#define FLASH_INVENT_SNAP_1         (FLASH_INVENT_SNAP_0 + FLASH_INVENT_SNAP0_SZ)
#define FLASH_INVENT_SNAP1_SZ       (BLOCK_SIZE)

#define FLASH_ITEM_ENT_AD_BASE      (FLASH_INVENT_SNAP_1 + FLASH_INVENT_SNAP1_SZ)
#define FLASH_ITEM_ENT_SZ           (BLOCK_SIZE)

enum flash_region {
    FLASH_REGION_JOURNAL,
    FLASH_REGION_SNAP0,
    FLASH_REGION_SNAP1,
    FLASH_REGION_ITEM_ENT,
    NUM_FLASH_REGIONS
};

#define MAX_INVENT_MSGS     (10U)
#define MAX_TRANSACTIONS    (20U)
#define FILE_NAME_LEN       (12U)

#define TRANS_PEND_TIMEOUT  (pdMS_TO_TICKS(45000)) // timeout to sync changes every 45s
#define TRANS_POST_TIMEOUT  (pdMS_TO_TICKS(25))

#define INVENTORY_TASK_STK_DEPTH    (512U)
#define INVENTORY_TASK_PRIO         (4U)

typedef struct {
    uint8_t state               : 1;    /* DIRTY OR CLEAN : DICTATES WHETHER WE FLUSH CSV UPDATES */
    uint8_t armed               : 2;    /* ARMED/DISARMED */
    uint8_t num_records         : 8;    /* MAX OF 256 ITEMS PER UNIT */
} unit_info_t;


struct flash_sec_handle {
    uint8_t erased;

    uint32_t base_addr;
    uint32_t offset;
    uint32_t size;
};

struct                                                                                                                                                             flash_metadata {
    uint32_t active_img;
    uint32_t inactive_img;
    uint32_t flash_snapshot_sn;     // last flash snapshot sequence number
    uint32_t sd_snapshot_sn;        // last sd snasphot sequence number 
    uint16_t num_valid_nodes;
    uint16_t num_valid_items;

    uint32_t node_journal_addrs[MAX_UNITS];
    uint32_t crcs[NUM_FLASH_REGIONS];

    uint32_t metadata_crc;
};


struct flash_handle {
    struct flash_sec_handle metadata_sec;
    struct flash_sec_handle journal_sec;
    struct flash_sec_handle invent_part0;
    struct flash_sec_handle invent_part1;
    struct flash_sec_handle item_dict_sec;

    spiflash_t flash_hspi;
    const spiflash_hal_t spiflash_funcs;
    const spiflash_config_t spiflash_conf;
    const spiflash_cmd_tbl_t spiflash_cmds;
};


struct flash_item_dict_entry {
    uint16_t token;
    uint32_t hash;

    char name[MAX_ITEM_NAME_LEN];
    uint8_t condition;

    uint32_t crc;
};


struct node_item {
    uint16_t token;
    uint8_t qty;
};


struct storage_node {
    uint8_t id;            /* OFFSET INTO MAIN SNAPSHOT WILL BE NODE_ID * FIXED SIZE + BASE */
    unit_info_t data;

    struct node_item items[MAX_UNIQUE_ITEMS];
    uint32_t crc;
};


struct storage_node_cache {
    uint8_t tail;
    uint8_t head;
    uint8_t size;

    struct storage_node nodes[NUM_UNITS];
};

/*********************
 * STATIC DECLARATIONS
 **********************/
static void task_inventory(void *arg);

static int32_t inventory_remove_item(uint16_t token, uint32_t item_idx, struct storage_node *node);
static int32_t inventory_enroll_item(uint16_t token, struct storage_node *node, uint8_t match_idx);
static uint8_t invent_item_is_dupe(struct storage_node *node, uint16_t item_token);
static uint8_t process_transaction(msg *trans);

static int32_t invent_restore_from_sd(void);
static int32_t invent_sync_sd(void);

static int32_t invent_get_item_desc(uint16_t token, char *name, uint8_t *condition);
static int32_t inventory_send_unit_contents(uint8_t node_id, uint8_t pg_idx);
static int32_t inventory_send_unit_stats(uint8_t pg_idx);

static void invent_retrieve_metadata(void);
static int32_t compute_flash_region_crc(uint32_t base_addr, uint32_t len, enum flash_region region);
static int32_t validate_flash_region(uint32_t base_addr, uint32_t len, uint32_t expected_crc);
static int32_t write_evt_to_journal(struct storage_node *node, uint8_t node_id);
static int32_t read_node_entry(struct storage_node *node, uint8_t node_id);
static int32_t flush_journal_evts(void);
static uint16_t invent_get_item_token(char *name, char *condition);
static int32_t invent_get_item_desc(uint16_t token, char *name, uint8_t *condition);
static uint32_t murmurhash3_32(const void* key, size_t len, uint32_t seed);
// Generates the 256-entry lookup table
void generate_crc32_table(void);
// Computes the CRC-32 checksum of a data buffer
uint32_t crc32_fast(const uint8_t *data, size_t length, uint32_t *crc, uint8_t new_val);


// static struct unit_csv_t node_csvs[NUM_UNITS]; // 3KB SRAM
static char trans_msg[MAX_LOG_BODY_LEN];

static int32_t spiflash_txrx(struct spiflash_s *spi, const uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len);
static void spiflash_cs(struct spiflash_s *spi, uint8_t cs_level);
static void spiflash_wait(struct spiflash_s *spi, uint32_t ms);
static int32_t spiflash_lock(struct spiflash_s *spi);
static int32_t spiflash_unlock(struct spiflash_s *spi);

static struct flash_handle invent_flash = {
    .flash_hspi = NULL,

    .metadata_sec = {.base_addr = FLASH_METADATA_AD_BASE, .offset = 0x00, .size = FLASH_METADA_SZ},
    .journal_sec  = {.base_addr = FLASH_JOURNAL_AD_BASE, .offset = 0x00, .size = FLASH_JOURNAL_SZ},
    .invent_part0 = {.base_addr = FLASH_INVENT_SNAP_0, .offset = 0x00, .size = FLASH_INVENT_SNAP0_SZ},
    .invent_part1 = {.base_addr = FLASH_INVENT_SNAP_1, .offset = 0x00, .size = FLASH_INVENT_SNAP1_SZ},
    .item_dict_sec = {.base_addr = FLASH_ITEM_ENT_AD_BASE, .offset = 0x00, .size = FLASH_ITEM_ENT_SZ},

    .spiflash_funcs = {
        ._spiflash_spi_txrx = spiflash_txrx,
        ._spiflash_spi_cs = spiflash_cs,
        ._spiflash_wait = spiflash_wait,
        ._spiflash_lock = spiflash_lock,
        ._spiflash_unlock = spiflash_unlock
    },

    .spiflash_conf = {
        .sz = FLASH_SIZE,
        .page_sz = PG_SIZE,
        .addr_sz = 3U,
        .addr_dummy_sz = 0U, // NORMAL SPI MODE
        .addr_endian = SPIFLASH_ENDIANNESS_BIG,
        .sr_write_ms = 12U, // 12 MS MAX
        .page_program_ms = 3U, // max 2.4 ms
        .block_erase_4_ms = 150U, // ms to erase sector (max)
        .block_erase_8_ms = 0, // NOT SUPPORTED
        .block_erase_16_ms = 0, // NOT SUPPORTED
        .block_erase_32_ms = 600U, // in ms (max)
        .block_erase_64_ms = 1000U, // in ms (max)
        .chip_erase_ms = 40000U, // in ms (max)
    },

    .spiflash_cmds = SPIFLASH_CMD_TBL_STANDARD
};


static struct flash_metadata metadata = {
    .active_img = FLASH_INVENT_SNAP_0,
    .inactive_img = FLASH_INVENT_SNAP_1,
    .flash_snapshot_sn = 0,
    .sd_snapshot_sn = 0,
    .node_journal_addrs = {0},
    .num_valid_items = 0,
    .num_valid_nodes = NUM_UNITS
};

struct storage_node_cache cache = {.head = 0, .tail = 0, .size = 0}; // ~500 bytes SRAM (more information)

static QueueHandle_t invent_msgq;
static StaticQueue_t _invent_msgq;
static uint8_t invent_q_buf[MAX_INVENT_MSGS * sizeof(msg)];

static SemaphoreHandle_t csv_mutx;
static StaticSemaphore_t csv_mutx_buf;

static TaskHandle_t invent_tsk;
static StaticTask_t _invent_tsk;
static StackType_t invent_stk[INVENTORY_TASK_STK_DEPTH];
/******************
 * PUBLIC APIs
 *****************/



/**
 * @brief Load inventory from SD card 
 * 
 * This function will initialize the inventory 
 * subsystem for the central node. Each CSV file
 * associated with a peer node's inventory will be 
 * loaded into RAM to be updated and periodically
 * written to a clean file.
 * 
 */
void c_inventory_init(void) {
    uint8_t status = STATUS_OK;
    csv_mutx = xSemaphoreCreateMutexStatic(&csv_mutx_buf);
    invent_msgq = xQueueCreateStatic(MAX_INVENT_MSGS, sizeof(msg), invent_q_buf, &_invent_msgq);
    invent_tsk = xTaskCreateStatic(task_inventory, "Invent Task", INVENTORY_TASK_STK_DEPTH,
                                        NULL, INVENTORY_TASK_PRIO, invent_stk, &_invent_tsk);

    if (invent_tsk == NULL || invent_msgq == NULL) {
        while (1) {}
    }
}



/**
 * @brief Receive transaction message and process accordingly
 * 
 * 
 * 
 */
uint8_t inventory_post_event(msg *transaction_msg) {
    return xQueueSendToBack(invent_msgq, transaction_msg, TRANS_POST_TIMEOUT);
}



/**
 * @brief Signal a rfid scan is requested
 * 
 * 
 * @param[in] name optional param, if NULL signals a tag scan
 * else signals a product scan
 * @return 0 on successful signal; else 1
 */
uint8_t inventory_signal_scan(char *name, char *cond) {
    uint8_t stat = pdFALSE;
    msg scan_msg = msg_init_default;
    scan_msg.which_payload = msg_type_transaction_tag;

    if (name != NULL) {
        scan_msg.command = SCAN_PRODUCT_CMD;
        memcpy((void *)scan_msg.payload.type_transaction.item_name, (const void *)name, MAX_ITEM_NAME_LEN);
        memcpy((void *)scan_msg.payload.type_transaction.item_cond, (const void *)cond, MAX_ITEM_CND_LEN);
        stat = xQueueSendToBack(invent_msgq, &scan_msg, portMAX_DELAY);
    }
    else {
        scan_msg.command = SCAN_TAG_CMD;
        stat = xQueueSendToBack(invent_msgq, &scan_msg, portMAX_DELAY);
    }
    
    return stat;
}


/**
 * @brief Retrieves as many records as are available to fit on current screen
 * 
 * @return number of records read on success; else 0
 */
static int32_t inventory_send_unit_contents(uint8_t node_id, uint8_t pg_idx) {
    int32_t ret = 0;
    uint8_t records_to_read = 0;

    // called by display function when screen needs to update (nothing else to do in that thread)
    invent_screen_t item_screen;
    struct storage_node *node = NULL;
    ret = read_node_entry(node, node_id);
    if (node->data.num_records > (ITEMS_PER_SCREEN * pg_idx)) {
        if (pg_idx * ITEMS_PER_SCREEN == 0)
            records_to_read = node->data.num_records <= ITEMS_PER_SCREEN ? node->data.num_records : ITEMS_PER_SCREEN;
        else
            records_to_read = node->data.num_records % (pg_idx * ITEMS_PER_SCREEN);
        
        uint16_t token = 0;
        uint8_t start_idx = ITEMS_PER_SCREEN * pg_idx;
        for (uint8_t i = 0; i < records_to_read; ++i) {
            token = node->items[start_idx + i].token;
            ret = invent_get_item_desc(token, item_screen.records[i].name, item_screen.records[i].condition);
            item_screen.records[i].qty = node->items[start_idx + i].qty;
        }

        item_screen.pg_idx = pg_idx;
        item_screen.valid_records = records_to_read;
        display_signal_invent_change(DISP_ITEM_CHANGE, (void *)&item_screen);
    }
    return ret;
}


/**
 * @brief
 * 
 * 
 */
static int32_t inventory_send_unit_stats(uint8_t pg_idx) {
    int32_t ret = 0;
    uint8_t records_to_read = 0;
    if (NUM_UNITS > (pg_idx * NODES_PER_SCREEN)) {
        if (pg_idx * NODES_PER_SCREEN == 0)
            records_to_read = NUM_UNITS <= NODES_PER_SCREEN ? NUM_UNITS : NODES_PER_SCREEN;
        else
            records_to_read = NUM_UNITS % (pg_idx * NODES_PER_SCREEN);

        units_screen_t unit_screen;
        struct storage_node *node = NULL;
        uint8_t node_id = 0;
        for (uint8_t i = 0; i < records_to_read; ++i) {
            node_id = (NODES_PER_SCREEN * pg_idx) + i;
            ret = read_node_entry(node, node_id);
            if (ret) continue;
            unit_screen.units[i].id_val = node_id;
            unit_screen.units[i].data.cap_val = node->data.num_records;
            unit_screen.units[i].data.armed = node->data.armed;
        }

        unit_screen.valid_units = records_to_read;
        unit_screen.pg_idx = pg_idx;
        display_signal_invent_change(DISP_UNIT_CHANGE, (void *)&unit_screen);
    }
    return ret;
}


/****************
 * STATIC DEFS
 ****************/

/**
 * @brief This task handles inventory management
 * 
 * This task handles scanning of rfid tags and
 * receiving/processing inventory transactions and 
 * updates.
 * 
 */
static void task_inventory(void *arg) {
    TickType_t prev_flush_tick = 0;
    TickType_t curr_flush_tick = 0;
    msg evt_msg = msg_init_default;
    int32_t stat = STATUS_OK, records_ready = 0;

    SPIFLASH_init(&invent_flash.flash_hspi, &invent_flash.spiflash_conf, 
                    &invent_flash.spiflash_cmds, &invent_flash.spiflash_funcs, NULL, 0, NULL);
    tag_init();
    invent_retrieve_metadata();
    if (sd_wait_ready() == pdTRUE) {
        int32_t active_snap_corrupted = 0, item_dict_corrupted = 0;
        enum flash_region region = metadata.active_img == FLASH_INVENT_SNAP_0 ? FLASH_REGION_SNAP0 : FLASH_REGION_SNAP1;
        active_snap_corrupted = validate_flash_region(metadata.active_img, BLOCK_SIZE, metadata.crcs[region]);
        item_dict_corrupted = validate_flash_region(invent_flash.item_dict_sec.base_addr, invent_flash.item_dict_sec.size, metadata.crcs[FLASH_REGION_ITEM_ENT]);
        if (active_snap_corrupted || item_dict_corrupted)
            stat = invent_restore_from_sd();
    }
    display_first_load_ready();
    
    for (;;) {
        if (xQueueReceive(invent_msgq, (void *)&evt_msg, TRANS_PEND_TIMEOUT) == pdTRUE) {
            switch (evt_msg.command) {
            case 0:
                process_transaction(&evt_msg);
                break;
            case SCAN_PRODUCT_CMD:
                display_load_scanning_screen();
                do {
                    stat = tag_register(TAG_PRODUCT, evt_msg.payload.type_transaction.item_name, evt_msg.payload.type_transaction.item_cond);
                    vTaskDelay(1); // allow other tasks to continue
                } while (stat != STATUS_OK);
                display_load_scanned_screen();
                break;
            case SCAN_TAG_CMD:
                // scan for key tag
                display_load_scanning_screen();
                do {
                    stat = tag_register(TAG_AUTH_CARD, NULL, NULL);
                    vTaskDelay(1); // allow other tasks to continue
                } while (stat != STATUS_OK);
                display_load_scanned_screen();
                break;
            case CMD_UNIT_STAT_CH:
                struct storage_node *node = NULL;
                stat = read_node_entry(node, evt_msg.node_id);
                node->data.armed = evt_msg.payload.type_alert.value;
                inventory_send_unit_stats(evt_msg.node_id / NODES_PER_SCREEN);
                break;
            case CMD_GET_INVENT_STATS:
                inventory_send_unit_contents(evt_msg.node_id, evt_msg.which_payload); // which payload will hold the pg_idx
                break;
            case CMD_GET_NODE_STATS:
                inventory_send_unit_stats(evt_msg.which_payload);
                break;
            }
        } else // idea is that if we are constantly getting transaction messages we don't want to keep flushing every time only flush after 10s of idle time
            stat = invent_sync_sd();
    }
}


static void invent_retrieve_metadata(void) {
    struct flash_metadata metadata_cpy;
    SPIFLASH_read(&invent_flash.flash_hspi, invent_flash.metadata_sec.base_addr, sizeof(metadata), (uint8_t *)&metadata_cpy);
    uint32_t cpy_crc = 0;
    crc32_fast((const uint8_t *)&metadata_cpy, sizeof(metadata_cpy) - 4, &cpy_crc, true);
    if (cpy_crc == metadata_cpy.metadata_crc)
        memcpy((void *)&metadata, (const void *)&metadata_cpy, sizeof(metadata));
}

/**
 * @brief Load into RAM all unit inventorys
 * 
 * This function stores all of the unit's inventory
 * in CsvRecord arrays of 660 bytes each. 
 * 
 * @note This doesn't scale well and would probably
 * benefit from loading in multiple screen's worth of 
 * units to quickly swap between them
 */
static int32_t invent_restore_from_sd(void) {
    int32_t ret = STATUS_OK;

    // load inventory for each unit into RAM
    FIL fp;
    char *items_bkup = "items.bin";
    char *nodes_bkup = "nodes.bin";

    SPIFLASH_erase(&invent_flash.flash_hspi, invent_flash.item_dict_sec.base_addr, invent_flash.item_dict_sec.size);
    invent_flash.item_dict_sec.offset = 0;

    ret = f_open(&fp, items_bkup, FA_OPEN_ALWAYS | FA_READ);
    uint32_t br, write_addr = 0;
    struct flash_item_dict_entry item;
    do {
        f_read(&fp, (void *)&item, sizeof(struct flash_item_dict_entry), &br);
        if (br != sizeof(struct flash_item_dict_entry)) continue;
        write_addr = FLASH_ADDR_FROM_ITEM_ID(invent_flash.item_dict_sec.base_addr, item.token);
        SPIFLASH_write(&invent_flash.flash_hspi, write_addr, sizeof(struct flash_item_dict_entry), (uint8_t *)&item);
        invent_flash.item_dict_sec.offset += sizeof(struct flash_item_dict_entry);
    } while (br == sizeof(struct flash_item_dict_entry));
    f_close(&fp);

    ret = f_open(&fp, nodes_bkup, FA_OPEN_ALWAYS | FA_READ);
    struct storage_node node;
    do {
        f_read(&fp, (void *)&node, sizeof(node), &br);
        if (br != sizeof(struct storage_node)) continue;
        write_addr = FLASH_ADDR_FROM_NODE_ID(metadata.inactive_img, node.id);
        SPIFLASH_write(&invent_flash.flash_hspi, write_addr, sizeof(struct storage_node), (uint8_t *)&node);
    } while(br == sizeof(struct storage_node));

    if (br >= sizeof(struct storage_node)) {
        SPIFLASH_erase(&invent_flash.flash_hspi, invent_flash.journal_sec.base_addr, invent_flash.journal_sec.size);
        SPIFLASH_erase(&invent_flash.flash_hspi, metadata.active_img, FLASH_INVENT_SNAP0_SZ);
    }

    f_close(&fp);
    return ret;
}


/**
 * @brief Flush transactions to transacton log file
 * 
 * This function should be periodically called when transactions
 * have been completed. 
 * 
 * @return 0 on success; else 1 
 */
static int32_t invent_sync_sd(void) {
    // flushes the active up-to-date inventory to the csv files
    int32_t ret = 0;
    char *items_fn = "items.bin"; // flash item dict backup
    char *invent_nodes = "nodes.bin"; // flash nodes backup
    char node_csv_file[FILE_NAME_LEN];

    FIL items_bkup, nodes_bkup, invent_csv;
    
    uint32_t bw;
    uint32_t item_addr = 0;
    struct flash_item_dict_entry item_entries[10];
    ret = f_open(&items_bkup, items_fn, FA_OPEN_ALWAYS | FA_WRITE);
    if (ret) goto cleanup;
    for (uint16_t i = 0; i < metadata.num_valid_items;) {
        uint8_t items_to_read = (i + 10) < metadata.num_valid_items ? 10 : metadata.num_valid_items - i;
        item_addr = FLASH_ADDR_FROM_ITEM_ID(FLASH_ITEM_ENT_AD_BASE, i);
        ret = SPIFLASH_read(&invent_flash.flash_hspi, item_addr, sizeof(struct flash_item_dict_entry) * items_to_read, (uint8_t *)item_entries);
        ret = f_write(&items_bkup, (const void *)item_entries, sizeof(struct flash_item_dict_entry) * items_to_read, &bw);
        i += items_to_read;
    }
    f_close(&items_bkup);

    ret = f_open(&nodes_bkup, invent_nodes, FA_OPEN_ALWAYS | FA_WRITE);
    if (ret) goto cleanup;
    struct storage_node *node = NULL;
    char csv_line[64];
    for (uint16_t i = 0; i < metadata.num_valid_nodes; ++i) {
        ret = read_node_entry(node, i);
        if (ret || node == NULL) continue;
        ret = f_write(&nodes_bkup, (const void *)node, sizeof(struct storage_node), &bw);

        snprintf(node_csv_file, FILE_NAME_LEN, "invent%02d.csv", i);
        ret = f_open(&invent_csv, node_csv_file, FA_OPEN_ALWAYS | FA_WRITE);
        for (uint16_t i = 0; i < node->data.num_records; ++i) {
            char name[MAX_ITEM_NAME_LEN];
            uint8_t condition;
            invent_get_item_desc(node->items[i].token, name, &condition);
            snprintf(csv_line, sizeof(csv_line), "%s,%s,%d", name, U8_TO_CONDITION_STR(condition), node->items[i].qty);
            f_write(&invent_csv, csv_line, sizeof(csv_line), &bw);
            memset(csv_line, 0, sizeof(csv_line));
        }
        f_close(&invent_csv);
        memset(node_csv_file, 0, FILE_NAME_LEN);
    }
    f_close(&nodes_bkup);

cleanup:
    return ret;
}


static uint8_t process_transaction(msg *trans) {
    int32_t status = STATUS_ERR;
    uint8_t item_found = 0;

    struct storage_node *node = NULL;
    status = read_node_entry(node, trans->node_id);
    if (status) goto cleanup;
    uint16_t token = invent_get_item_token(trans->payload.type_transaction.item_name, trans->payload.type_transaction.item_cond);
    
    uint8_t match_idx = invent_item_is_dupe(node, token);
    if (trans->payload.type_transaction.direction == PRODUCT_OUT && (match_idx != 0xFFU))
        status = inventory_remove_item(token, match_idx, node);
    else if (trans->payload.type_transaction.direction == PRODUCT_IN)
        status = inventory_enroll_item(token, node, match_idx);

    if (!status && node->data.state == CSV_CLEAN)
        node->data.state = CSV_DIRTY;

cleanup:
    return status;
}


static uint8_t invent_item_is_dupe(struct storage_node *node, uint16_t item_token) {
    uint8_t match_idx = 0xFF;
    for (uint8_t i = 0; i < node->data.num_records; ++i) {
        if (item_token == node->items[i].token) {
            match_idx = i;
            break;
        }
    }
}


/**
 * @brief Add the item associated with the item_id to the storage unit
 * 
 * 
 * 
 */
static int32_t inventory_enroll_item(uint16_t token, struct storage_node *node, uint8_t match_idx) {
    int32_t status = STATUS_ERR;
    uint8_t item_idx = 0;

    if (node->data.num_records < MAX_ITEMS) {
        char item_name[MAX_ITEM_NAME_LEN];
        uint8_t condition = 0;
        status = invent_get_item_desc(token, item_name, &condition);
        if (status) goto cleanup;

        memset((void *)trans_msg, 0, MAX_LOG_BODY_LEN);
        snprintf(trans_msg, MAX_LOG_BODY_LEN, "UNIT %02d Item Added: %s, %s\r\n",
                node->id, item_name, U8_TO_CONDITION_STR(condition));
        status = log_transaction(trans_msg);
        if (status == STATUS_OK)
            status = client_post_message(trans_msg, strlen(trans_msg));

        // IF DUPLICATE ITEM IN STORAGE SIMPLY INCREMENT QTY
        if (match_idx != 0xFF) {
            node->items[match_idx].qty++;
            item_idx = match_idx;
        }
        else {
            // append only, saves us the overhead of shifting entire array
            uint8_t record_idx = node->data.num_records;
            item_idx = record_idx;
            struct node_item *new_record = &node->items[record_idx];
            new_record->token = token;
            new_record->qty = 1;
            // increment record count
            node->data.num_records++;
        }

        inventory_send_unit_contents(node->id, item_idx / ITEMS_PER_SCREEN);
        inventory_send_unit_stats(node->id / NODES_PER_SCREEN);
    }

cleanup:
    return status;
}



/**
 * @brief Remove the item associated with the item_id from the storage unit
 * 
 * 
 * 
 */
static int32_t inventory_remove_item(uint16_t token, uint32_t item_idx, struct storage_node *node) {
    int32_t status = STATUS_ERR;

    if (node->data.num_records > 0) {
        char item_name[MAX_ITEM_NAME_LEN];
        uint8_t condition = 0;
        status = invent_get_item_desc(token, item_name, &condition);

        memset((void *)trans_msg, 0, MAX_LOG_BODY_LEN);
        snprintf(trans_msg, MAX_LOG_BODY_LEN, "Item Removed: %s, %s\r\n",
            item_name, U8_TO_CONDITION_STR(condition));
        status = log_transaction(trans_msg);
        if (status == STATUS_OK)
            status = client_post_message(trans_msg, strlen(trans_msg));

        // swap item at index and last item when we "remove"
        if (--node->items[item_idx].qty == 0) {
            struct node_item temp = node->items[node->data.num_records - 1];
            node->items[item_idx] = temp;
            memset((void *)&node->items[node->data.num_records - 1], 0, sizeof(struct node_item));
            node->data.num_records--;
        }
        inventory_send_unit_contents(node->id, item_idx / ITEMS_PER_SCREEN);
        inventory_send_unit_stats(node->id / NODES_PER_SCREEN);
    }

    return status;
}

/**************************
 * FLASH MEMORY APIs
 ***************************/

static int32_t compute_flash_region_crc(uint32_t base_addr, uint32_t len, enum flash_region region) {
    int32_t ret = 0;
    uint16_t pages_to_read = len / PG_SIZE;
    uint16_t valid_bytes = len % PG_SIZE; // bytes to read after all pages read
    uint32_t *crc = &metadata.crcs[region];

    uint8_t buff[PG_SIZE], new_crc = 1;
    uint32_t addr = 0;
    uint16_t i = 0;
    for (i = 0; i < pages_to_read; ++i) {
        addr = base_addr + (PG_SIZE * i);
        ret = SPIFLASH_read(&invent_flash.flash_hspi, addr, PG_SIZE, buff);
        if (ret) goto cleanup;
        crc32_fast(buff, PG_SIZE, crc, i == 0 ? true : false);
    }

    if (valid_bytes) {
        addr = base_addr + (PG_SIZE * i);
        ret = SPIFLASH_read(&invent_flash.flash_hspi, addr, valid_bytes, buff);
        if (ret) goto cleanup;
        crc32_fast(buff, valid_bytes, crc, false);
    }
cleanup:
    return ret;
}

static int32_t validate_flash_region(uint32_t base_addr, uint32_t len, uint32_t expected_crc) {
    int32_t ret = 0;
    uint32_t crc = 0;
    ret = compute_flash_region_crc(base_addr, len, crc); // TODO: FIX LIKELY JUST USE THE ENUMS TO INDEX AN ARRAY OF REGIONS
    if (ret) return ret;
    if (crc != expected_crc) ret = 1;
    return ret;
}


/**
 * @brief Writes updated snapshot of node into journal
 * 
 * This function is called when a node in the active cache 
 * if evicted from the cache. It writes the last active state 
 * of the node into the journal. Journal logs are flushed
 * to new main image when the journal block is full
 * 
 */
static int32_t write_evt_to_journal(struct storage_node *node, uint8_t node_id) {
    uint32_t br = (invent_flash.journal_sec.size - invent_flash.journal_sec.offset);
    if (br < sizeof(struct storage_node))
        flush_journal_evts();
/*
    struct storage_node *node_to_update;
    int32_t ret = read_node_entry(node_to_update, e->node_id);
    if (ret || (node_to_update->node_id != e->node_id)) goto cleanup;
    node_to_update->capacity++;
    node_to_update->items[e->item_token].qty++;
*/
    uint32_t write_addr = invent_flash.journal_sec.base_addr + invent_flash.journal_sec.offset;
    metadata.node_journal_addrs[node_id] = write_addr;
    int32_t ret = SPIFLASH_write(&invent_flash.flash_hspi, write_addr, sizeof(struct storage_node), (const uint8_t *)node);
    if (ret) goto cleanup;
    invent_flash.journal_sec.offset += sizeof(struct storage_node);
    
    ret = compute_flash_region_crc(invent_flash.journal_sec.base_addr, invent_flash.journal_sec.size, FLASH_REGION_JOURNAL);
cleanup:
    return ret;
}



/**
 * @brief Retrieves nodes from flash into SRAM cache
 * 
 * This funciton should be called when the cached nodes 
 * need to be swapped out, e.g. when the user scrolls 
 * to other sections of main screen
 * 
 */
static int32_t read_node_entry(struct storage_node *node, uint8_t node_id) {
    int32_t ret = 0;
    int32_t node_found = 0;
    for (uint8_t i = 0; i < NUM_UNITS; ++i) {
        if (cache.nodes[i].id == node_id) {
            node = &cache.nodes[i];
            node_found = 1;
            break;
        }
    }
    if (node_found) goto cleanup;

    // if not in cache go to journal
    uint32_t read_addr;
    if ((read_addr = metadata.node_journal_addrs[node_id]) != INVALID_FLASH_ADDR) {
        if (cache.size == NUM_UNITS) {
            ret = write_evt_to_journal(&cache.nodes[cache.tail], cache.nodes[cache.tail].id);
            cache.tail = (cache.tail + 1) % NUM_UNITS;
            cache.size--;
        }

        ret = SPIFLASH_read(&invent_flash.flash_hspi, read_addr, sizeof(struct storage_node), (uint8_t *)&cache.nodes[cache.head]);
        if (ret) goto cleanup;
        cache.size++;
        node = &cache.nodes[cache.head];
        cache.head = (cache.head + 1) % NUM_UNITS;
        goto cleanup;
    }

    // if invalid addr in journal LUT, get from main snapshot
    read_addr = FLASH_ADDR_FROM_NODE_ID(metadata.active_img, node_id);
    if (cache.size == NUM_UNITS) {
        ret = write_evt_to_journal(&cache.nodes[cache.tail], cache.nodes[cache.tail].id);
        cache.tail = (cache.tail + 1) % NUM_UNITS;
        cache.size--;
    }

    ret = SPIFLASH_read(&invent_flash.flash_hspi, read_addr, sizeof(struct storage_node), (uint8_t *)&cache.nodes[cache.head]);
    if (ret) goto cleanup;
    cache.size++;
    node = &cache.nodes[cache.head];
    cache.head = (cache.head + 1) % NUM_UNITS;
    goto cleanup;
cleanup:
    return ret;
}


/**
 * @brief Flushes the latest node journal logs to the main snapshot
 * 
 * 
 * This function is called when the event journal is 
 * filled. The latest journal states are written to the 
 * new main snapshot.
 * 
 */
static int32_t flush_journal_evts(void) {
    int32_t ret = 0;
    struct storage_node *node;
    ret = SPIFLASH_erase(&invent_flash.flash_hspi, metadata.inactive_img, BLOCK_SIZE);
    if (ret) goto cleanup;
    enum flash_region snap_region = metadata.inactive_img == FLASH_INVENT_SNAP_0 ? FLASH_REGION_SNAP0 : FLASH_REGION_SNAP1;
    for (uint8_t i = 0; i < metadata.num_valid_nodes; ++i) {
        ret = read_node_entry(node, i);
        if (ret) continue;
        uint32_t write_addr = FLASH_ADDR_FROM_NODE_ID(metadata.inactive_img, i); 
        ret = SPIFLASH_write(&invent_flash.flash_hspi, metadata.inactive_img, sizeof(struct storage_node), (const uint8_t *)node);
    }
    ret = compute_flash_region_crc(metadata.inactive_img, BLOCK_SIZE, snap_region);
    ret = SPIFLASH_erase(&invent_flash.flash_hspi, FLASH_JOURNAL_AD_BASE, BLOCK_SIZE);
    memset((void *)&cache, 0, sizeof(cache));
    uint32_t temp = metadata.active_img;
    metadata.active_img = metadata.inactive_img;
    metadata.inactive_img = temp;

cleanup:
    return ret;
}



/**
 * @brief
 * 
 * 
 * 
 */
static uint16_t invent_get_item_token(char *name, char *condition) {
    int32_t ret = 0;
    uint16_t match_idx = 0xFFFF;
    struct flash_item_dict_entry item_entries[10];

    uint8_t key_len = strlen(name) + strlen(condition);
    uint8_t hash_array[MAX_ITEM_NAME_LEN + MAX_ITEM_CND_LEN - 2];
    memcpy((void *)hash_array, (const void *)name, strlen(name));
    memcpy((void *)(hash_array + strlen(name)), (const void *)condition, strlen(condition));
    uint32_t hash = murmurhash3_32((const void *)hash_array, key_len, MURMURHASH_SEED);
    
    for (uint16_t i = 0; i < metadata.num_valid_items; i += 10) {
        ret = SPIFLASH_read(&invent_flash.flash_hspi, FLASH_ADDR_FROM_ITEM_ID(FLASH_ITEM_ENT_AD_BASE, i), sizeof(item_entries), (uint8_t *)item_entries);
        for (uint8_t j = 0; j < 10; ++j) {
            if (item_entries[j].hash != hash) continue;
            if (!strcmp((const char *)name, (const char *)item_entries->name) && (CONDITION_STR_TO_U8(condition) == item_entries->condition)) {
                match_idx = item_entries[j].token;
                break;
            }
        }
        if (match_idx != 0xFFFFU) break;
    }

    if (match_idx == 0xFFFFU) {
        struct flash_item_dict_entry new_ent;
        memcpy(new_ent.name, name, MAX_ITEM_NAME_LEN);
        new_ent.condition = CONDITION_STR_TO_U8(condition);
        new_ent.hash = hash;
        new_ent.token = metadata.num_valid_items + 1;
        
        ret = SPIFLASH_write(&invent_flash.flash_hspi, FLASH_ADDR_FROM_ITEM_ID(FLASH_ITEM_ENT_AD_BASE, new_ent.token), sizeof(new_ent), (const uint8_t *)&new_ent);
        metadata.num_valid_items++;
        compute_flash_region_crc(invent_flash.item_dict_sec.base_addr, invent_flash.item_dict_sec.size, FLASH_REGION_ITEM_ENT);
    }

    return match_idx;
}



static int32_t invent_get_item_desc(uint16_t token, char *name, uint8_t *condition) {
    uint32_t item_addr = FLASH_ADDR_FROM_ITEM_ID(FLASH_ITEM_ENT_AD_BASE, token);

    struct flash_item_dict_entry item;
    int32_t ret = SPIFLASH_read(&invent_flash.flash_hspi, item_addr, sizeof(struct flash_item_dict_entry), (uint8_t *)&item);
    if (ret || (item.token != token)) goto cleanup;
    memcpy((void *)name, (const void *)item.name, strlen(item.name) + 1);
    *condition = item.condition;
cleanup:
    return ret;
}


static uint32_t murmurhash3_32(const void* key, size_t len, uint32_t seed) {
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len / 4;
    uint32_t h1 = seed;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    // ---------- body ----------
    const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);

    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17); // ROTL32(k1, 15)
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19); // ROTL32(h1, 13)
        h1 = h1 * 5 + 0xe6546b64;
    }

    // ---------- tail ----------
    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);
    uint32_t k1 = 0;

    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
                k1 *= c1; 
                k1 = (k1 << 15) | (k1 >> 17); 
                k1 *= c2; 
                h1 ^= k1;
    };

    // ---------- finalization ----------
    h1 ^= len;

    // fmix32 avalanche step
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}


static uint32_t crc32_table[256];
static int table_computed = 0;

// Generates the 256-entry lookup table
void generate_crc32_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    table_computed = 1;
}

// Computes the CRC-32 checksum of a data buffer
uint32_t crc32_fast(const uint8_t *data, size_t length, uint32_t *crc, uint8_t new_val) {
    if (!table_computed) {
        generate_crc32_table();
    }
    
    if (new_val) // Standard CRC-32 initializes the register to all 1s
        *crc = 0xFFFFFFFF; 
    
    for (size_t i = 0; i < length; i++) {
        uint8_t table_index = (*crc ^ data[i]) & 0xFF;
        *crc = (*crc >> 8) ^ crc32_table[table_index];
    }
    
    // Post-invert the result before returning
    return *crc ^ 0xFFFFFFFF; 
}


static int32_t spiflash_txrx(struct spiflash_s *spi, const uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len) {
    int32_t ret = 0;
    if (tx_buf && tx_len)
        ret = spi_transmit(DEV_FLASH, tx_buf, tx_len);
    if (ret) goto cleanup;
    if (rx_buf && rx_len)
        ret = spi_receive(DEV_FLASH, rx_buf, rx_len);
cleanup:
    return ret;
}

static void spiflash_cs(struct spiflash_s *spi, uint8_t cs_level) {
    if (cs_level > 0)
        io_set_out(IO_SPI_CS_FLASH, IO_OUT_LOW);
    else
        io_set_out(IO_SPI_CS_FLASH, IO_OUT_HIGH);
}

static void spiflash_wait(struct spiflash_s *spi, uint32_t ms) {
    vTaskDelay(ms);
}

static int32_t spiflash_lock(struct spiflash_s *spi) {
    return spi_lock(DEV_FLASH);
}

static int32_t spiflash_unlock(struct spiflash_s *spi) {
    return spi_unlock(DEV_FLASH);
}
