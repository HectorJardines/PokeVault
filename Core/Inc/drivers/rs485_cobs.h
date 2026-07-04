/**
 * @author Hector Jardines
 * 
 */

#ifndef _RS_COBS_H
#define _RS_COBS_H

#include <stdint.h>
#include "stm32f4xx.h"

/******************
 * STRUCTS/ENUMS
 ******************/



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
 * @brief 
 * 
 * 
 * 
 */
uint8_t rs485_receive(uint8_t *encoded_buf, uint32_t *length);


void register_msg_ready_cb(void(*ready_cb)(void));


 #endif /* _RS_COBS_H */