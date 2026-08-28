/**
 * Utilizes the MFRC522 driver to implement 
 */

#ifndef _KEYCARD_H
#define _KEYCARD_H

#include "../../../Core/Inc/drivers/mfrc522.h"

#define UID_MAX_LEN         (7U) // accomodate 4 and 7-byte UIDs
#define UID_LEN_BYTES       (5U) // 4 UID BYTES + 1 BCC
#define PICC_MEM_BLOCK_LEN  (16U)
#define SER_NUM_LEN_BYTES   (4U)
#define SEC_KEY_LEN         (6U)
#define DEFAULT_SEC_KEY     (0xFFU)

/********************
 * ENUMS/STRUCTS
 ********************/
typedef enum {
    TAG_OK,
    TAG_REGISTERED,
    TAG_EXISTS,
    TAG_REMOVED,
    TAG_AUTHORIZED,
    TAG_REJECTED,
    TAG_ERR
} tag_status_e;

typedef enum {
    TAG_PRODUCT,
    TAG_AUTH_CARD
} tag_type_e;

/********************
 * PUBLIC APIs
 *******************/

/**
 * @brief Initializes the RFID tag reader
 * 
 * 
 * 
 */
tag_status_e tag_init(void); 

/**
 * @brief Registers a tag and saves its serialnumber for subsequent authorization
 * 
 * 
 * 
 * @param uid 4-byte serial serial number passed as a buffer of single bytes
 */
uint8_t tag_register(tag_type_e type, const uint8_t *name_buf, const uint8_t *cond_buf);


#endif