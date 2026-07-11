#include "../Inc/inventory.h"
#include "../Inc/alerts.h"
#include "../Inc/client.h"
#include "ring_buffer.h"
#include "sd_functions.h"
#include "../../Inc/common/defines.h"
#include "log.h"

#define MAX_TRANSACTIONS    (10U)
#define FILE_NAME_LEN       (11U)
/*********************
 * STATIC DECLARATIONS
 **********************/

struct unit_csv_t{
    uint8_t state; /* DIRTY OR CLEAN : DICTATES WHETHER WE FLUSH CSV UPDATES */
    uint8_t num_records;
    CsvRecord unit_inventory[MAX_ITEMS_PER_UNIT];
};

static struct unit_csv_t node_csvs[NUM_UNITS];
static char trans_msg[MAX_LOG_BODY_LEN];
// STATIC_RING_BUFFER(transaction_queue, MAX_TRANSACTIONS, transaction_t);
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
void inventory_init(void) {
    uint8_t status = STATUS_OK;
    // load inventory for each unit into RAM
    char node_csv_file[FILE_NAME_LEN];

    for (uint8_t i = 0; i < NUM_UNITS; ++i) {
        memset((void *)node_csv_file, 0, FILE_NAME_LEN);
        snprintf(node_csv_file, FILE_NAME_LEN, "invent%02d.csv", i);
        status = sd_read_csv(node_csv_file, node_csvs[i].unit_inventory, MAX_ITEMS_PER_UNIT, &node_csvs[i].num_records);
        node_csvs[i].state = CSV_CLEAN;
    }

    return status;
}



/**
 * @brief Receive transaction message and process accordingly
 * 
 * 
 * 
 */
uint8_t inventory_process_transaction(msg transaction_msg) {
    uint8_t status = STATUS_OK;
    
    uint8_t item_found = 0;
    struct unit_csv_t node = node_csvs[transaction_msg.node_id];
    transaction trans = transaction_msg.payload.type_transaction;
    for (uint8_t i = 0; i < node.num_records; ++i) {
        if (node.unit_inventory[i].id == trans.item_id) {
            status = inventory_remove_item(i, transaction_msg.node_id);
            item_found = 1;
            break;
        }
    }
    if (!item_found) {
        status = inventory_enroll_item(trans.item_id, trans.item_name, transaction_msg.node_id);
    }

    if (!status && node_csvs[transaction_msg.node_id].state == CSV_CLEAN)
        node_csvs[transaction_msg.node_id].state == CSV_DIRTY;

    return status;
}



/**
 * @brief Remove the item associated with the item_id from the storage unit
 * 
 * 
 * 
 */
uint8_t inventory_remove_item(uint32_t item_idx, uint8_t node_id) {
    uint8_t status = STATUS_ERR;

    if (node_csvs[node_id].num_records > 0) {
        memset((void *)trans_msg, 0, MAX_LOG_BODY_LEN);
        snprintf(trans_msg, MAX_LOG_BODY_LEN, "Item Removed: %s, %d\r\n", 
            node_csvs[node_id].unit_inventory[item_idx].name, node_csvs[node_id].unit_inventory[item_idx].id);
        status = log_transaction(trans_msg);
        if (status == STATUS_OK)
            status = client_post_message(trans_msg, strlent(trans_msg));

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
uint8_t inventory_enroll_item(uint32_t item_id, char *item_name, uint8_t node_id) {
    uint8_t status = STATUS_ERR;

    if (node_csvs[node_id].num_records < MAX_ITEMS_PER_UNIT) {
        memset((void *)trans_msg, 0, MAX_LOG_BODY_LEN);
        snprintf(trans_msg, MAX_LOG_BODY_LEN, "UNIT %02d - Item Added: %s, %d\r\n",
                node_id, item_name, item_id);
        status = log_transaction(trans_msg);
        if (status == STATUS_OK)
            status = client_post_message(trans_msg, strlent(trans_msg));


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
uint8_t inventory_flush_transactions(void) {
    // flushes the active up-to-date inventory to the csv files
    uint8_t status = STATUS_OK;
    char node_csv_file[FILE_NAME_LEN];

    for (uint8_t i = 0; i < NUM_UNITS; ++i) {
        memset((void *)node_csv_file, 0, FILE_NAME_LEN);
        snprintf(node_csv_file, FILE_NAME_LEN, "invent%02d.csv", i);
        status = sd_write_csv(node_csv_file, node_csvs[i].unit_inventory, node_csvs[i].num_records);
        node_csvs[i].state = CSV_CLEAN;
    }

    return status;
}



/****************
 * STATIC DEFS
 ****************/



