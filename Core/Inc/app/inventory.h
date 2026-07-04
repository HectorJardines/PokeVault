/**
 * @Author Hector Jardines
 * 
 * This module handles the inventory management logic for 
 * a given storage unit.
 * 
 * 
 */
#ifndef _INVENTORY_H
#define _INVENTORY_H

#include <stdint.h>

#define MAX_ITEM_NAME_LEN (16U) // MAX ITEM NAME LEN IN BYTES

/*********************
 * STRUCTS/ENUMS
 ********************/
typedef enum {
    TRANS_ITEM_REMOVE,
    TRANS_ITEM_ADD
} trans_type_e;

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


/**
 * @brief Remove the item associated with the item_id from the storage unit
 * 
 * 
 * 
 */
transaction_t inventory_remove_item(uint32_t item_id);

/**
 * @brief Add the item associated with the item_id to the storage unit
 * 
 * 
 * 
 */
transaction_t inventory_add_item(uint32_t item_id);


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