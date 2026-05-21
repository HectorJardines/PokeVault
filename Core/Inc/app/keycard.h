/**
 * Utilizes the MFRC522 driver to implement 
 */

#ifndef _KEYCARD_H
#define _KEYCARD_H

#include "../drivers/mfrc522.h"

#define UID_LEN_BYTES       (5U) // 4 UID BYTES + 1 BCC
#define PICC_MEM_BLOCK_LEN  (16U)
#define SER_NUM_LEN_BYTES   (4U)
#define SEC_KEY_LEN         (6U)
#define SECTOR_TRAIL_BLOCK  (3U) // block address of the PICC sector trailer where KEY and access bits are held
#define SECTOR_BLOCK_2      (2U)
#define DEFAULT_SEC_KEY    (0xFFU)
#define USER_SEC_KEYB       (0x88U)
#define ACCESS_BYTE_6       (0xFFU)
#define ACCESS_BYTE_7       (0x07)
#define ACCESS_BYTE_8       (0x80U)
#define MAN_SECTOR_NUM      (0U)
#define MAN_SECTOR_BLOCK    (0U)

/********************
 * ENUMS/STRUCTS
 ********************/
typedef enum {
    KEYCARD_OK,
    KEYCARD_REGISTERED,
    KEYCARD_EXISTS,
    KEYCARD_REMOVED,
    KEYCARD_AUTHORIZED,
    KEYCARD_REJECTED,
    KEYCARD_ERR
} keycard_status_e;

typedef enum {
    KEYCARD_ENTRY1,
    KEYCARD_ENTRY2
} keycard_index_e;

/********************
 * PUBLIC APIs
 *******************/

/**
 * @brief Initializes the RFID keycard reader
 * 
 * 
 * 
 */
keycard_status_e keycard_init(void); 

/**
 * @brief
 */
keycard_status_e keycard_quick_scan(void);


/**
 * @brief Registers a keycard and saves its serialnumber for subsequent authorization
 * 
 * 
 * 
 * @param uid 4-byte serial serial number passed as a buffer of single bytes
 */
keycard_status_e keycard_register(keycard_index_e keycard_entry);


/**
 * @brief Removes a keycard from the registry to revoke access
 * 
 * @param uid 4-byte serial num associated with keycard to remove
 */
keycard_status_e keycard_forget(uint8_t *uid);

#endif