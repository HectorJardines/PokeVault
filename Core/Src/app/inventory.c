#include "inventory.h"
#include "rfid_tag.h"
#include "message.h"
#include "ring_buffer.h"
#include "common/defines.h"


#define CACHE_SIZE          (5U)

/*************************
 * STATIC DECLARATION
 ************************/

static uint8_t item_type_block[PICC_MEM_BLOCK_LEN] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0,0,0,0,0,0,0,0,0,0};
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
    uint8_t status = tag_read_data(active_transaction.item_id, active_transaction.item_name,
                                    ITEM_SECTOR, TYPE_BLOCK);

    if (status == STATUS_OK) {
        for(uint8_t i = 0; i < PICC_MEM_BLOCK_LEN; ++i) { // compare type block read with expected type value
            status = !(active_transaction.item_name[i] == item_type_block[i]);
            if (status)
                break;
        }
        if (status == STATUS_OK) // read actual item name
            status = tag_read_data(active_transaction.item_id, active_transaction.item_name, ITEM_SECTOR, NAME_BLOCK); 
    }

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
    item_detected.payload.type_transaction.item_id = *((uint32_t *)active_transaction.item_id);
    memcpy((void *)item_detected.payload.type_transaction.item_name, (void *)active_transaction.item_name, strlen(active_transaction.item_name));

    message_send(&item_detected);

    return STATUS_OK;
}


/******************
 * STATIC DEFS
 ********************/


