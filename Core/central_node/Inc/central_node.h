/**
 * @author Hector Jardines
 * 
 * Central Node contoller logic implementation.
 * Defines the polling of peer nodes and processing
 * of commands received from peer nodes as well as
 * commands sent to peer nodes.
 * 
 * 
 */
#ifndef _CENTRAL_H
#define _CENTRAL_H


#include <stdint.h>

#define PEER_RX_CPLT_Msk        (0x1)
#define PEER_MSG_READY_Msk      (0x1 << 1)

typedef struct {
    uint8_t flags;
    uint8_t curr_node;
    uint8_t pending_msg_cnt;
} node_state_t;

/**
 * @brief Initialize the central node subsystems
 * 
 * Initializes the client, inventory, messaging,
 * and rfid_tag subsystems of the central node.
 * 
 */
void central_node_init(void);


/**
 * @brief Polls the peer nodes for any pending messages
 * 
 * This function polls one peer node and returns. Should not be 
 * called again until the peer node has sent it's pending messages or 
 * a tiemout occurs.
 * 
 */
uint8_t central_node_poll_peer(void);


/**
 * @brief Processes a single pending message
 * 
 * This function retrieves and processes a message received
 * from a peer node. E.g. if the buffered message contains 
 * a security breach alert, the central node will send
 * the alert via the ethernet module to the mobile device.
 * 
 * @return 0 on success; 1 else
 * 
 */
uint8_t central_node_process(void);


#endif