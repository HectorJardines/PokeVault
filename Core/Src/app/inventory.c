#include "inventory.h"
#include "rfid_tag.h"
#include "message.h"
#include "ring_buffer.h"
#include "common/defines.h"


#define CACHE_SIZE          (5U)

/*************************
 * STATIC DECLARATION
 ************************/
static transaction_t active_transaction;
// BUFFER STORES RECENT TRANSACITONS IN CASE OF MESSAGE FAILURE
STATIC_RING_BUFFER(transaction_cache, CACHE_SIZE, transaction_t);

/*************************
 * PUB APIs
 ***************/



/**
 * @brief Initialize inventory management subsystem 
 * 
 * Initialize underlize RFID tag module used for item 
 * classification.
 * 
 */
uint8_t inventory_init(void) {
    uint8_t status = tag_init();

    if (status == STATUS_OK) {
        message_init();
        memset((void *)&active_transaction, 0, sizeof(active_transaction));
    }
    
    return status;
}


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
uint8_t inventory_scan_for_item(void) {
    memset((void *)&active_transaction, 0, sizeof(active_transaction));
    // verify tag scanned is an item tag by checking the sector block written on tag reg
    uint8_t status = tag_read_product_data(active_transaction.item_name,
                    active_transaction.item_cond,
                    &active_transaction.direction);
    return status;
}


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
uint8_t inventory_item_update(void) {
    ring_buffer_push(&transaction_cache, (const void *)&active_transaction);
    
    msg item_detected = msg_init_default;
    item_detected.node_id = NODE_ID;
    item_detected.which_payload = msg_type_transaction_tag;
    memcpy((void *)item_detected.payload.type_transaction.item_name, (void *)active_transaction.item_name, strlen(active_transaction.item_name));
    memcpy((void *)item_detected.payload.type_transaction.item_cond, (void *)active_transaction.item_cond, strlen(active_transaction.item_cond));
    item_detected.payload.type_transaction.direction = active_transaction.direction;

    message_send(&item_detected);

    return STATUS_OK;
}


/******************
 * STATIC DEFS
 ********************/


