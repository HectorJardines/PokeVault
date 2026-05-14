/**
 * This module utilizes the IR sensors and MFRC522
 * RFID reader to implement the security mechanism of 
 * the system.
 */
#ifndef _SECURITY_H
#define _SECURITY_H

#include "../drivers/ir_sensor.h"
#include "./keycard.h"

/******************
 * ENUMS/STRUCTS
 *****************/
typedef enum {
    SECURITY_UNITIALIZED,
    SECURITY_ARMED,
    SECURITY_DISARMED,
    SECURITY_MANUAL_OVERRIDE,
    SECURITY_BREACHED
} security_state_e;

/*****************
 * USER APIs
 *****************/
/**
 * @brief Initializes the security module and necessary submodules
 * 
 * This API intializes the keycard and line_break submodules that 
 * make up the systems security mechanisms.
 */
uint8_t security_init(void);

/**
 * @brief Retrieves the current security state
 * 
 * This API queries the security components, e.g. 
 * MFRC522 card status, line break status, etc. Compares the 
 * result of these queries and returns the state of 
 * the security system depending on the combination of 
 * values.
 */
security_state_e security_check_state(void);

/**
 * @brief Updates the current security state based on the current readings
 */
void security_update_state(security_state_e sec_state);

/**
 * @brief Alternative method to disarm defenses for testing purposes
 */
void security_disarm_manual(void);


#endif