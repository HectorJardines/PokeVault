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


#define MAX_TRANSACTIONS    (10U)
#define FILE_NAME_LEN       (12U)

#define TRANS_PEND_TIMEOUT  (pdMS_TO_TICKS(4500))
#define TRANS_POST_TIMEOUT  (pdMS_TO_TICKS(25))
#define INVENT_FLUSH_PERIOD (pdMS_TO_TICKS(5000)) // flush every 5 seconds

#define INVENTORY_TASK_STK_DEPTH    (512U)
#define INVENTORY_TASK_PRIO         (3U)



/*********************
 * STATIC DECLARATIONS
 **********************/
static uint8_t load_inventory(void);
static void task_inventory(void *arg);
static uint8_t inventory_remove_item(uint32_t item_idx, uint8_t node_id);
static uint8_t inventory_enroll_item(uint32_t item_id, char *item_name, uint8_t node_id);
static uint8_t process_transaction(msg *trans);
static uint8_t inventory_flush_transactions(void);

struct unit_csv_t{
    uint8_t state; /* DIRTY OR CLEAN : DICTATES WHETHER WE FLUSH CSV UPDATES */
    uint8_t armed;
    uint32_t num_records;
    CsvRecord unit_inventory[MAX_ITEMS_PER_UNIT];
};


static struct unit_csv_t node_csvs[NUM_UNITS];
static char trans_msg[MAX_LOG_BODY_LEN];

static QueueHandle_t trans_q;
static StaticQueue_t _trans_q;
static uint8_t trans_q_buf[MAX_TRANSACTIONS * sizeof(msg)];

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
    invent_tsk = xTaskCreateStatic(task_inventory, "Invent Task", INVENTORY_TASK_STK_DEPTH,
                                        NULL, INVENTORY_TASK_PRIO, invent_stk, &_invent_tsk);

    if (invent_tsk == NULL || trans_q == NULL) {
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
    return xQueueSendToBack(trans_q, transaction_msg, TRANS_POST_TIMEOUT);
}



/**
 * @brief Signal a rfid scan is requested
 * 
 * 
 * @param[in] name optional param, if NULL signals a tag scan
 * else signals a product scan
 * @return 0 on successful signal; else 1
 */
uint8_t inventory_signal_scan(char *name) {
    uint8_t stat = pdFALSE;
    msg scan_msg = msg_init_default;
    scan_msg.which_payload = msg_type_transaction_tag;

    if (name != NULL) {
        scan_msg.command = SCAN_PRODUCT_CMD;
        memcpy((void *)scan_msg.payload.type_transaction.item_name, (const void *)name, MAX_ITEM_NAME_LEN);
        stat = xQueueSendToBack(trans_q, &scan_msg, portMAX_DELAY);
    }
    else {
        scan_msg.command = SCAN_TAG_CMD;
        stat = xQueueSendToBack(trans_q, &scan_msg, portMAX_DELAY);
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
        if (node_csvs[node_id].num_records > ITEMS_PER_SCREEN * pg_idx) {
            uint8_t record_mod = pg_idx == 0 ? ITEMS_PER_SCREEN : ITEMS_PER_SCREEN * pg_idx;
            records_read = (node_csvs[node_id].num_records % record_mod) * sizeof(CsvRecord);
            memcpy((void *)records, 
                    (const void *)&node_csvs[node_id].unit_inventory[ITEMS_PER_SCREEN * pg_idx],
                    records_read);
        }
        xSemaphoreGive(csv_mutx);
    }
    return records_read / sizeof(CsvRecord);
}


/**
 * @brief
 * 
 * 
 */
uint8_t inventory_get_unit_stats(unit_record_t *records, uint8_t pg_idx) {
    uint8_t records_read = 0;
    if (xSemaphoreTake(csv_mutx, portMAX_DELAY) == pdTRUE) {
        if (NUM_UNITS > pg_idx * NODES_PER_SCREEN) {
            uint8_t record_mod = pg_idx == 0 ? NODES_PER_SCREEN : NODES_PER_SCREEN * pg_idx;
            records_read = (NUM_UNITS % record_mod);

            for (uint8_t i = 0; i < records_read; ++i) {
                snprintf(records[i].capacity, sizeof(records[i].capacity), "%02d/%02d", 
                        node_csvs[(NODES_PER_SCREEN * pg_idx) + i].num_records, MAX_ITEMS_PER_UNIT);
                snprintf(records[i].id, sizeof(records[i].id), "UNIT %d", (NODES_PER_SCREEN * pg_idx) + i);
                records[i].armed = node_csvs[(NODES_PER_SCREEN * pg_idx) + i].armed;
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
    msg trans_msg = msg_init_default;
    uint8_t stat = STATUS_OK, records_ready = 0;

    for (;;) {
        if (xQueueReceive(trans_q, (void *)&trans_msg, TRANS_PEND_TIMEOUT) == pdTRUE) {
            if (!records_ready) {
                if (trans_msg.command == RECORDS_READY_CMD) {
                    stat = load_inventory();
                    records_ready = 1;
                }
                else continue;
            }

            if(trans_msg.command == 0)
                process_transaction(&trans_msg);
            else if (trans_msg.command == SCAN_PRODUCT_CMD) { // prob change to a state based approach, dont want to block all other tasks here
                // scan for product tag
                display_load_scanning_screen();
                while (tag_register(TAG_PRODUCT, trans_msg.payload.type_transaction.item_name) != STATUS_OK);
                display_load_scanned_screen();
            }
            else if (trans_msg.command == SCAN_TAG_CMD) {
                // scan for key tag
                display_load_scanning_screen();
                while (tag_register(TAG_AUTH_CARD, NULL) != STATUS_OK);
                display_load_scanned_screen();
            }
        }

        curr_flush_tick = xTaskGetTickCount();
        if (curr_flush_tick - prev_flush_tick >= INVENT_FLUSH_PERIOD) {
            stat = inventory_flush_transactions(); // eventually log any errors
            prev_flush_tick = curr_flush_tick;
        }

        UBaseType_t high_stk_usage = uxTaskGetStackHighWaterMark(NULL);
        // printf("INVENT TASK: FREE RAM = %d - %d\r\n", INVENTORY_TASK_STK_DEPTH, high_stk_usage);
    }
}



static uint8_t load_inventory(void) {
    uint8_t status = STATUS_OK;

    // load inventory for each unit into RAM
    FIL fp;
    char node_csv_file[FILE_NAME_LEN];
    for (uint8_t i = 0; i < NUM_UNITS; ++i) {
        memset((void *)node_csv_file, 0, FILE_NAME_LEN);
        snprintf(node_csv_file, FILE_NAME_LEN, "inv%02d.csv", i);
        status |= sd_read_csv(node_csv_file, node_csvs[i].unit_inventory, MAX_ITEMS_PER_UNIT, &node_csvs[i].num_records);
        if (status == FR_NO_PATH || status == FR_NO_FILE) {
            f_open(&fp, node_csv_file, FA_OPEN_ALWAYS | FA_WRITE);
            f_close(&fp);
            status = STATUS_OK;
        }
        node_csvs[i].state = CSV_CLEAN;
    }

    if (status = STATUS_OK) 
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

    if (node_csvs[node_id].num_records > 0) {
        memset((void *)trans_msg, 0, MAX_LOG_BODY_LEN);
        snprintf(trans_msg, MAX_LOG_BODY_LEN, "Item Removed: %s, %d\r\n", 
            node_csvs[node_id].unit_inventory[item_idx].name, node_csvs[node_id].unit_inventory[item_idx].id);
        status = log_transaction(trans_msg);
        if (status == STATUS_OK)
            status = client_post_message(trans_msg, strlen(trans_msg));

        // swap item at index and last item when we "remove"
        CsvRecord temp = node_csvs[node_id].unit_inventory[node_csvs[node_id].num_records - 1];
        node_csvs[node_id].unit_inventory[item_idx] = temp;
        memset((void *)&node_csvs[node_id].unit_inventory[node_csvs[node_id].num_records - 1], 0, sizeof(CsvRecord));

        node_csvs[node_id].num_records--;
    }

    return status;
}



/**
 * @brief Add the item associated with the item_id to the storage unit
 * 
 * 
 * 
 */
static uint8_t inventory_enroll_item(uint32_t item_id, char *item_name, uint8_t node_id) {
    uint8_t status = STATUS_ERR;

    if (node_csvs[node_id].num_records < MAX_ITEMS_PER_UNIT) {
        memset((void *)trans_msg, 0, MAX_LOG_BODY_LEN);
        snprintf(trans_msg, MAX_LOG_BODY_LEN, "UNIT %02d - Item Added: %s, %d\r\n",
                node_id, item_name, item_id);
        status = log_transaction(trans_msg);
        if (status == STATUS_OK)
            status = client_post_message(trans_msg, strlen(trans_msg));

        // append only, saves us the overhead of shifting entire array
        uint8_t record_idx = node_csvs[node_id].num_records;
        CsvRecord *new_record = &node_csvs[node_id].unit_inventory[record_idx];
        memcpy((void *)new_record->name, (void *)item_name, strlen(item_name));
        new_record->id = item_id;

        // increment record count
        node_csvs[node_id].num_records++;
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
        if (node_csvs[i].state == CSV_CLEAN)
            continue;
        memset((void *)node_csv_file, 0, FILE_NAME_LEN);
        snprintf(node_csv_file, FILE_NAME_LEN, "inv%02d.csv", i);
        status = sd_write_csv(node_csv_file, node_csvs[i].unit_inventory, node_csvs[i].num_records);
        node_csvs[i].state = CSV_CLEAN;
    }

    return status;
}




static uint8_t process_transaction(msg *trans) {
    uint8_t status = STATUS_OK;
    uint8_t item_found = 0;

    struct unit_csv_t node = node_csvs[trans->node_id];

    for (uint8_t i = 0; i < node.num_records; ++i) {
        if (node.unit_inventory[i].id == trans->payload.type_transaction.item_id) {
            status = inventory_remove_item(i, trans->node_id);
            item_found = 1;
            break;
        }
    }

    if (!item_found) {
        status = inventory_enroll_item(trans->payload.type_transaction.item_id, 
                                        trans->payload.type_transaction.item_name, 
                                        trans->node_id);
    }

    if (!status && node_csvs[trans->node_id].state == CSV_CLEAN)
        node_csvs[trans->node_id].state = CSV_DIRTY;

    return status;
}
