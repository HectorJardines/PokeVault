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
#include "../Inc/message.h"

#define MAX_ITEM_NAME_LEN   (16U) // MAX ITEM NAME LEN IN BYTES
#define MAX_ITEMS_PER_UNIT  (20U)
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

// typedef struct {
//     trans_type_e type;
//     uint32_t item_id;
// } transaction_t;

// typedef struct {
//     unsigned char name[MAX_ITEM_NAME_LEN];
//     uint32_t id;
// } item_t;

/******************
 * PUBLIC APIs
 ******************/


/**
 * @brief Load inventory from SD card 
 * 
 * 
 * 
 */
void inventory_init(void); 


uint8_t inventory_process_transaction(msg *transaction_msg);

/**
 * @brief Remove the item associated with the item_id from the storage unit
 * 
 * 
 * 
 */
uint8_t inventory_remove_item(uint32_t item_idx, uint8_t node_id);

/**
 * @brief Add the item associated with the item_id to the storage unit
 * 
 * 
 * 
 */
uint8_t inventory_enroll_item(uint32_t item_id, char *item_name, uint8_t node_id);


/**
 * @brief Flush transactions to transacton log file
 * 
 * This function should be periodically called when transactions
 * have been completed. Sends transaction info to central MCU that 
 * stores the information in a transaction log file.
 * 
 * @return 0 on success; else 1 
 */
uint8_t inventory_flush_transactions(void);


#endif /* _INVENTORY_H */