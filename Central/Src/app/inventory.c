#include "../../Inc/app/inventory.h"
// #include "../../Inc/common/printf-stdarg.h"
#include <stdio.h>
#include "../../Inc/app/client.h"
#include "../../../Core/Inc/common/ring_buffer.h"
#include "../../Inc/common/defines.h"
#include "../../Inc/common/log.h"
#include "../../Inc/app/rfid_tag.h"
#include "../../Inc/app/display.h"

#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"


/********************
 * MACROS
 ***************/


#define MAX_INVENT_MSGS     (10U)
#define MAX_TRANSACTIONS    (20U)
#define FILE_NAME_LEN       (12U)

#define TRANS_PEND_TIMEOUT  (pdMS_TO_TICKS(10000)) // timeout to sync changes every 10s
#define TRANS_POST_TIMEOUT  (pdMS_TO_TICKS(25))

#define INVENTORY_TASK_STK_DEPTH    (512U)
#define INVENTORY_TASK_PRIO         (4U)

typedef struct {
    uint8_t state               : 1;    /* DIRTY OR CLEAN : DICTATES WHETHER WE FLUSH CSV UPDATES */
    uint8_t armed               : 2;    /* ARMED/DISARMED */
    uint8_t num_records         : 8;    /* MAX OF 256 ITEMS PER UNIT */
} unit_info_t;

struct unit_csv_t{
    unit_info_t unit_data;                          /* BITFIELD OF UNIT METADATA, I.E. STATE, NUM ITEMS, ARMED STATUS*/
    item_info_t unit_inventory[MAX_UNIQUE_ITEMS];
};

// we trade time for space here, e.g. we have to retrieve the name/condition from FLASH but we save 20 bytes per item in RAM
struct trans_evt {
    uint8_t node_id;
    uint8_t item_token;             /* MAPS TO IDX IN NODE BUFFER IN FLASH */
    int8_t direction;               /* IN OR OUT */
};

struct node_item {
    uint8_t token;
    uint8_t qty;
};

struct cached_node {
    uint8_t node_id;
    uint8_t capacity;

    struct node_item items[MAX_UNIQUE_ITEMS];
};
/*********************
 * STATIC DECLARATIONS
 **********************/
static uint8_t load_inventory(void);
static void task_inventory(void *arg);
static uint8_t inventory_remove_item(uint32_t item_idx, uint8_t node_id);
static uint8_t inventory_enroll_item(char *item_name, char *itm_condition, uint8_t node_id, uint8_t match_idx);
static int8_t invent_item_is_dupe(char *name, char *condition, uint8_t node_id);
static uint8_t process_transaction(msg *trans);
static uint8_t inventory_flush_transactions(void);


static struct unit_csv_t node_csvs[NUM_UNITS]; // 3KB SRAM
static char trans_msg[MAX_LOG_BODY_LEN];

static struct cached_node nodes[NUM_UNITS]; // 294 bytes SRAM (same information)
static QueueHandle_t trans_q;
static StaticQueue_t _trans_q;
static uint8_t trans_q_buf[MAX_TRANSACTIONS * sizeof(struct trans_evt)];

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
    trans_q = xQueueCreateStatic(MAX_TRANSACTIONS, sizeof(msg), trans_q_buf, &_trans_q);
    invent_msgq = xQueueCreateStatic(MAX_INVENT_MSGS, sizeof(msg), invent_q_buf, &_invent_msgq);
    invent_tsk = xTaskCreateStatic(task_inventory, "Invent Task", INVENTORY_TASK_STK_DEPTH,
                                        NULL, INVENTORY_TASK_PRIO, invent_stk, &_invent_tsk);

    for (uint8_t i = 0; i < NUM_UNITS; ++i) // all units initially armed
        node_csvs[i].unit_data.armed = 1;

    if (invent_tsk == NULL || trans_q == NULL) {
        while (1) {}
    }
    sizeof(node_csvs);
    sizeof(nodes);
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
uint8_t inventory_get_contents(uint8_t node_id, CsvRecord *records, uint8_t pg_idx) {
    uint8_t records_read = 0;
    // called by display function when screen needs to update (nothing else to do in that thread)
    if (xSemaphoreTake(csv_mutx, portMAX_DELAY) == pdTRUE) {
        if (node_csvs[node_id].unit_data.num_records > ITEMS_PER_SCREEN * pg_idx) {
            if (pg_idx * ITEMS_PER_SCREEN == 0)
                records_read = node_csvs[node_id].unit_data.num_records <= ITEMS_PER_SCREEN ? node_csvs[node_id].unit_data.num_records : ITEMS_PER_SCREEN;
            else
                records_read = node_csvs[node_id].unit_data.num_records % (pg_idx * ITEMS_PER_SCREEN);
            memcpy((void *)records,
                    (const void *)&node_csvs[node_id].unit_inventory[ITEMS_PER_SCREEN * pg_idx],
                    (records_read * sizeof(CsvRecord)));
        }
        xSemaphoreGive(csv_mutx);
    }
    return records_read;
}


/**
 * @brief
 * 
 * 
 */
uint8_t inventory_get_unit_stats(unit_record_t *records, uint8_t pg_idx, uint8_t *new_values) {
    uint8_t records_read = 0;
    if (xSemaphoreTake(csv_mutx, portMAX_DELAY) == pdTRUE) {
        if (NUM_UNITS > pg_idx * NODES_PER_SCREEN) {
            if (pg_idx * NODES_PER_SCREEN == 0)
                records_read = NUM_UNITS <= NODES_PER_SCREEN ? NUM_UNITS : NODES_PER_SCREEN;
            else
                records_read = NUM_UNITS % (pg_idx * NODES_PER_SCREEN);

            for (uint8_t i = 0; i < records_read; ++i) {
                uint8_t id = (NODES_PER_SCREEN * pg_idx) + i;
                uint8_t cap =  node_csvs[id].unit_data.num_records;
                uint8_t armed_stat = node_csvs[id].unit_data.armed;

                if (records[i].data.cap_val == cap && records[i].id_val == id && records[i].data.armed == armed_stat)
                    continue;

                records[i].id_val = id;
                records[i].data.cap_val = cap;
                records[i].data.armed = armed_stat;
            }
        }
        xSemaphoreGive(csv_mutx);
    }
    return records_read;
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
    uint8_t stat = STATUS_OK, records_ready = 0;

    if (sd_wait_ready() == pdTRUE) {
        load_inventory();
        tag_init();
    }

    for (;;) {
        if (xQueueReceive(invent_msgq, (void *)&evt_msg, TRANS_PEND_TIMEOUT) == pdTRUE) {
            if(evt_msg.command == 0)
                process_transaction(&evt_msg);
            else if (evt_msg.command == SCAN_PRODUCT_CMD) { // prob change to a state based approach, dont want to block all other tasks here
                // scan for product tag
                display_load_scanning_screen();
                do {
                    stat = tag_register(TAG_PRODUCT, evt_msg.payload.type_transaction.item_name, evt_msg.payload.type_transaction.item_cond);
                    vTaskDelay(1); // allow other tasks to continue
                } while (stat != STATUS_OK);
                display_load_scanned_screen();
            } else if (evt_msg.command == SCAN_TAG_CMD) {
                // scan for key tag
                display_load_scanning_screen();
                do {
                    stat = tag_register(TAG_AUTH_CARD, NULL, NULL);
                    vTaskDelay(1); // allow other tasks to continue
                } while (stat != STATUS_OK);
                display_load_scanned_screen();
            } else if (evt_msg.command == CMD_UNIT_STAT_CH) {
                node_csvs[evt_msg.node_id].unit_data.armed = evt_msg.payload.type_alert.value;
                display_signal_unit_change(DISP_UNIT_CHANGE);
            }
        } else // idea is that if we are constantly getting transaction messages we don't want to keep flushing every time only flush after 10s of idle time
            stat = inventory_flush_transactions();
    }
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
static uint8_t load_inventory(void) {
    uint8_t status = STATUS_OK;

    // load inventory for each unit into RAM
    FIL fp;
    char node_csv_file[FILE_NAME_LEN];
    for (uint8_t i = 0; i < NUM_UNITS; ++i) {
        memset((void *)node_csv_file, 0, FILE_NAME_LEN);
        snprintf(node_csv_file, FILE_NAME_LEN, "inv%02d.csv", i);
        
        int32_t num_records = 0;
        status |= sd_read_csv(node_csv_file, node_csvs[i].unit_inventory, MAX_ITEMS, &num_records);
        if (status == FR_NO_PATH || status == FR_NO_FILE) {
            f_open(&fp, node_csv_file, FA_OPEN_ALWAYS | FA_WRITE);
            f_close(&fp);
            status = STATUS_OK;
        }
        node_csvs[i].unit_data.num_records = num_records;
        node_csvs[i].unit_data.state = CSV_CLEAN;
    }

    if (status == STATUS_OK) 
        display_first_load_ready();
    return status;
}



/**
 * @brief Remove the item associated with the item_id from the storage unit
 * 
 * 
 * 
 */
static uint8_t inventory_remove_item(uint32_t item_idx, uint8_t node_id) {
    uint8_t status = STATUS_ERR;

    if (node_csvs[node_id].unit_data.num_records > 0) {
        memset((void *)trans_msg, 0, MAX_LOG_BODY_LEN);
        snprintf(trans_msg, MAX_LOG_BODY_LEN, "Item Removed: %s\r\n",
            node_csvs[node_id].unit_inventory[item_idx].name);
        status = log_transaction(trans_msg);
        if (status == STATUS_OK)
            status = client_post_message(trans_msg, strlen(trans_msg));

        // swap item at index and last item when we "remove"
        if (--node_csvs[node_id].unit_inventory[item_idx].qty == 0) {
            CsvRecord temp = node_csvs[node_id].unit_inventory[node_csvs[node_id].unit_data.num_records - 1];
            node_csvs[node_id].unit_inventory[item_idx] = temp;
            memset((void *)&node_csvs[node_id].unit_inventory[node_csvs[node_id].unit_data.num_records - 1], 0, sizeof(CsvRecord));
            node_csvs[node_id].unit_data.num_records--;
        }
        display_signal_unit_change(DISP_INVENT_CHANGE);
        display_signal_unit_change(DISP_UNIT_CHANGE);
    }

    return status;
}



/**
 * @brief
 * 
 * 
 * 
 */
static int8_t invent_item_is_dupe(char *name, char *condition, uint8_t node_id) {
    int8_t match_idx = 0xFF;
    for (uint8_t i = 0; i < node_csvs[node_id].unit_data.num_records; ++i) {
        if (memcmp((const void *)condition, (const void *)node_csvs[node_id].unit_inventory[i].condition, MAX_ITEM_CND_LEN))
            continue; // condition doesn't match
        if (memcmp((const void *)name, (const void *)node_csvs[node_id].unit_inventory[i].name, MAX_ITEM_NAME_LEN))
            continue; // name doesn't match
        else {
            match_idx = i;
            break;
        }
    }
    return match_idx;
}

/**
 * @brief Add the item associated with the item_id to the storage unit
 * 
 * 
 * 
 */
static uint8_t inventory_enroll_item(char *item_name, char *itm_condition, uint8_t node_id, uint8_t match_idx) {
    uint8_t status = STATUS_ERR;

    if (node_csvs[node_id].unit_data.num_records < MAX_ITEMS) {
        memset((void *)trans_msg, 0, MAX_LOG_BODY_LEN);
        snprintf(trans_msg, MAX_LOG_BODY_LEN, "UNIT %02d Item Added: %s, %s\r\n",
                node_id, item_name, itm_condition);
        status = log_transaction(trans_msg);
        if (status == STATUS_OK)
            status = client_post_message(trans_msg, strlen(trans_msg));

        // IF DUPLICATE ITEM IN STORAGE SIMPLY INCREMENT QTY
        if (match_idx != 0xFF)
            node_csvs[node_id].unit_inventory[match_idx].qty++;
        else {
            // append only, saves us the overhead of shifting entire array
            uint8_t record_idx = node_csvs[node_id].unit_data.num_records;
            CsvRecord *new_record = &node_csvs[node_id].unit_inventory[record_idx];
            memset((void *)new_record->name, 0, MAX_ITEM_NAME_LEN);
            memcpy((void *)new_record->name, (void *)item_name, strlen(item_name));
            memset((void *)new_record->condition, 0, MAX_ITEM_CND_LEN);
            memcpy((void *)new_record->condition, (void *)itm_condition, strlen(itm_condition));
            new_record->qty = 1;
            // increment record count
            node_csvs[node_id].unit_data.num_records++;
        }

        display_signal_unit_change(DISP_INVENT_CHANGE);
        display_signal_unit_change(DISP_UNIT_CHANGE);
    }

    return status;
}



/**
 * @brief Flush transactions to transacton log file
 * 
 * This function should be periodically called when transactions
 * have been completed. 
 * 
 * @return 0 on success; else 1 
 */
static uint8_t inventory_flush_transactions(void) {
    // flushes the active up-to-date inventory to the csv files
    uint8_t status = STATUS_OK;
    char node_csv_file[FILE_NAME_LEN];

    for (uint8_t i = 0; i < NUM_UNITS; ++i) {
        if (node_csvs[i].unit_data.state == CSV_CLEAN)
            continue;
        memset((void *)node_csv_file, 0, FILE_NAME_LEN);
        snprintf(node_csv_file, FILE_NAME_LEN, "inv%02d.csv", i);
        status = sd_write_csv(node_csv_file, node_csvs[i].unit_inventory, node_csvs[i].unit_data.num_records);
        node_csvs[i].unit_data.state = CSV_CLEAN;
    }

    return status;
}




static uint8_t process_transaction(msg *trans) {
    sizeof(node_csvs);
    uint8_t status = STATUS_OK;
    uint8_t item_found = 0;

    struct unit_csv_t node = node_csvs[trans->node_id];

    uint8_t match_idx = invent_item_is_dupe(trans->payload.type_transaction.item_name, 
                            trans->payload.type_transaction.item_cond,
                            trans->node_id);
    if (trans->payload.type_transaction.direction == PRODUCT_OUT && (match_idx != 0xFF))
        status = inventory_remove_item(match_idx, trans->node_id);
    else if (trans->payload.type_transaction.direction == PRODUCT_IN)
        status = inventory_enroll_item(trans->payload.type_transaction.item_name,
                                        trans->payload.type_transaction.item_cond,
                                        trans->node_id, match_idx);

    if (!status && node_csvs[trans->node_id].unit_data.state == CSV_CLEAN)
        node_csvs[trans->node_id].unit_data.state = CSV_DIRTY;

    return status;
}
