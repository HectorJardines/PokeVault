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

#define UID_LEN           (4U)
#define MAX_ITEM_NAME_LEN (16U) // MAX ITEM NAME LEN IN BYTES

/*********************
 * STRUCTS/ENUMS
 ********************/
typedef struct {
    uint8_t item_id[UID_LEN];
    char item_name[MAX_ITEM_NAME_LEN];
} transaction_t;



/******************
 * PUBLIC APIs
 ******************/



/**
 * @brief Initializes inventory subsystem and its dependency modules
 * 
 * 
 * @return 0 on successful init; else 1
 */
uint8_t inventory_init(void);



/**
 * @brief Activates RFID reader for scanning of nearby items
 * 
 * This function activates the RFID reader's RF field for
 * scanning of nearby item's. I.e. it checks if a product is 
 * present and sends message to central node if detected.
 * 
 * 
 * @return 0 on successful detecet and write, else 1
 */
uint8_t inventory_scan_for_item(void);



/**
 * @brief Sends the scanned item's UID to central node
 * 
 * This function simply sends the item ID and item Name to the central node.
 * This keeps the peer nodes stateless and simple. The central node 
 * will handle the add/remove logic by checking the associated peer node's
 * inventory CSV file for presence of the tag's UID.
 * 
 * @param[in] item_id enrolled item UID
 *
 * @return 0 on success; 1 else
 */
uint8_t inventory_item_update(void);


#endif /* _INVENTORY_H */