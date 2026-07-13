/**
 * @author Hector Jardines
 * 
 * message structs are transferred between storage units and 
 * central controller via RS485 physical layer using a custom 
 * protocol encoded with COBS.
 * 
 */

#ifndef _DATA_TRANS_H
#define _DATA_TRANS_H

#include <stdint.h>
#include "../../../Drivers/nanopb/messages.pb.h"

#define MAX_PAYLOAD_LEN (512U)

/*******************
 * STRUCTS/ENUMS
 *******************/

typedef enum {
    MSG_ALERT,                /* e.g. UNIT UNLOCKED, UNIT BREACHED, UNIT MOVED */
    MSG_TRANSACTION,          /* e.g. ITEM REMOVED, ITEM ADDED */
    MSG_EVENT                 /* e.g. TEMP/HUM HIGH, PRESENCE DETECTED */
} message_type_e;


typedef enum {
    MSG_CMD_NONE,
    MSG_CMD_CTS,
    MSG_CMD_SEND_CPLT
} message_cmd_e;

/*********************
 * PUBLIC APIs
 *********************/

/**
 * @brief Intialize data transfer sub-system
 * 
 * 
 * 
 */
void message_init(void);



/**
 * @brief Send message to central MCU
 * 
 * 
 * 
 */
uint8_t message_send(msg *message);



/**
 * @brief receive message from central MCU
 * 
 * 
 * 
 */
uint8_t message_receive(msg_array *message);


/**
 * @brief Returns whetehr any messages are pending from peer node
 * 
 * 
 * 
 */
uint8_t message_available(void);



/**
 * @brief Registers the callback function for peer node RX cplt
 * 
 * 
 * 
 */
void register_peer_rx_cplt_cb(void(*cb)(void));

#endif /* _DATA_TRANS_H */