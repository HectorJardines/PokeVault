/**
 * @author Hector Jardines
 * 
 */

#ifndef _RS_COBS_H
#define _RS_COBS_H

#include <stdint.h>
#include "stm32f4xx.h"
#include "../../../Drivers/nanopb/messages.pb.h"

/**************
 * MACROS
 *************/
#define CRC16_LEN  (2U)
#define NUM_OVERHEAD_BYTES (2U)
#define MAX_COBS_OVERHEAD ((DRIVERS_NANOPB_MESSAGES_PB_H_MAX_SIZE >> 8) + NUM_OVERHEAD_BYTES)
#define MAX_FRAME_LEN (DRIVERS_NANOPB_MESSAGES_PB_H_MAX_SIZE + MAX_COBS_OVERHEAD + CRC16_LEN)


/*****************
 * STRUCTS/ENUMS
 ******************/
typedef enum {    
    MSG_OUT,
    FRAME_IN
} payload_type_e;

typedef uint8_t msg_frame[MAX_FRAME_LEN];

typedef struct {
    uint16_t len;
    msg_frame buf;
} frame_info_t;


typedef struct {
    uint8_t which;
    union {
        frame_info_t frame;     /* EXTERNAL MESSAGE RX FROM PEER */
        msg message;            /* INTERNAL MESSAGE SENT TO PEER */
    };
} frame_msg_t;

/******************
 *      APIs
 *******************/


 /**
  * @brief Intialize underlying USART peripheral that handles data tx/rx
  * 
  * 
  * 
  */
uint8_t rs485_init(void);



/**
 * @brief 
 * 
 * 
 */
uint8_t rs485_transmit(uint8_t *data, uint32_t length);


/**
 * @brief Begins circular reception of bytes over RS485
 * 
 * 
 * 
 */
void receive_begin(void);

/**
 * @brief Decode COBS encoded buffer into arbitrary data pointer
 * 
 * 
 * 
 * @param encoded_buf
 * @param length
 * @param data
 */
uint16_t rs485_cobs_decode(uint8_t *encoded_buf, uint32_t length, void *data);


void register_msg_in_cb(void(*in_cb)(uint8_t *frame, uint32_t len, uint32_t *hpt));


// /**
//  * @brief 
//  * 
//  * 
//  * 
//  */
// uint8_t rs485_receive(uint8_t *encoded_buf, uint32_t *length);


// void register_msg_ready_cb(void(*ready_cb)(void));
// void register_msg_consumed_cb(void(*consumed_cb)(void));


 #endif /* _RS_COBS_H */