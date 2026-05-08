/**
 * This module implements the mfrc522 reader driver. Used 
 * for key card reading and authorization. A submodule for the
 * system's security measures.
 */
#ifndef _MFRC522_H
#define _MFRC522_H

#include <stdint.h>

/************************
 *  REGISTER DEFINITIONS
 ************************/

/***********************
 *   ENUMS/STRUCTS
 ***********************/


/**********************
 *      USER APIs
 **********************/

/**
 * @brief Intializes and configures the MFRC522 device
 */
void mfrc522_init(void);

/**
 * @brief Performs keycard RFID authentication
 * 
 * @return 0 if authentication fails; 1 else
 */
uint8_t mfrc522_auth(void);

/**
 * @brief Compares the keycard id with stored ID
 * 
 * @param card_id RFID associated with the keycard
 * @param registered_id predefined RFID
 * 
 * @return 0 if IDs differ; 1 else
 */
uint8_t mfrc_compare(uint32_t card_id, uint32_t registered_id);

#endif