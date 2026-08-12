/**
 * @author Hector Jardines
 * 
 * This module handles the inventory management logic for 
 * a given storage unit. The idea is going to be to pull 
 * a csv file of item names and UIDs. User initiates an
 * enrollment mode with the central node and selects one 
 * of the pre-defined item names. The central node then polls
 * each target node for an enrollment event. When one of the peer 
 * nodes replies with an enrollment event + UID the central node 
 * stores that UID in the CSV file and logs the transaction event.
 * 
 * 
 */
#ifndef _INVENTORY_H
#define _INVENTORY_H

#include <stdint.h>
#include "../app/central_message.h"
#include "../../Inc/drivers/sd_functions.h"

#define SCAN_TAG_CMD        (1U)
#define SCAN_PRODUCT_CMD    (2U)
#define CMD_GET_INVENT      (3U)
#define CMD_GET_NODE_STAT   (4U)
#define RECORDS_READY_CMD   (5U)

#define ITEMS_PER_SCREEN    (7U)
#define NODES_PER_SCREEN    (6U)
#define MAX_ITEM_NAME_LEN   (16U) // MAX ITEM NAME LEN IN BYTES
#define MAX_ITEMS_PER_UNIT  (21U)
#define NUM_UNITS           (3U)

/*********************
 * STRUCTS/ENUMS
 ********************/

typedef enum {
    CSV_CLEAN,      // NO CHNAGES HAVE BEEN MADE TO UNIT INVENT SINCE LAST LOAD/WRITE
    CSV_DIRTY       // CHANGES HAVE BEEN MADE TO UNIT INVENTORY SINCE IT WAS LAST LOADED/WRITTEN
} csv_state_e;


typedef enum {
    TRANS_ITEM_NONE,
    TRANS_ITEM_REMOVE,
    TRANS_ITEM_ENROLL
} trans_type_e;


typedef struct {
    char id[8];
    char capacity[6];
    uint8_t armed;
} unit_record_t;

/******************
 * PUBLIC APIs
 ******************/


/**
 * @brief Load inventory from SD card 
 * 
 * 
 * 
 */
void c_inventory_init(void);


/**
 * @brief Receive transaction message and process accordingly
 * 
 * 
 * 
 */
uint8_t inventory_post_event(msg *transaction_msg);



/**
 * @brief Retrieves as many records as are available to fit on current screen
 * 
 * @return number of records read on success; else 0
 */
uint8_t inventory_get_contents(uint8_t node_id, CsvRecord *records, uint8_t pg_idx);



/**
 * @brief
 * 
 * 
 */
uint8_t inventory_get_unit_stats(unit_record_t *records, uint8_t pg_idx);



/**
 * @brief Signal a rfid scan is requested
 * 
 * 
 * @param[in] name optional param, if NULL signals a tag scan
 * else signals a product scan
 * @return 0 on successful signal; else 1
 */
uint8_t inventory_signal_scan(char *name);



// /**
//  * @brief Remove the item associated with the item_id from the storage unit
//  * 
//  * 
//  * 
//  */
// uint8_t inventory_remove_item(uint32_t item_idx, uint8_t node_id);

// /**
//  * @brief Add the item associated with the item_id to the storage unit
//  * 
//  * 
//  * 
//  */
// uint8_t inventory_enroll_item(uint32_t item_id, char *item_name, uint8_t node_id);


// /**
//  * @brief Flush transactions to transacton log file
//  * 
//  * This function should be periodically called when transactions
//  * have been completed. Sends transaction info to central MCU that 
//  * stores the information in a transaction log file.
//  * 
//  * @return 0 on success; else 1 
//  */
// uint8_t inventory_flush_transactions(void);


#endif /* _INVENTORY_H */