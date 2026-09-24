/**
 * Utilizes the MFRC522 driver to implement 
 */

#ifndef _KEYCARD_H
#define _KEYCARD_H

#include "../drivers/mfrc522.h"

#define UID_LEN_BYTES       (9U) // 7 UID BYTES (MAX) + 1 BCC
#define PICC_MEM_BLOCK_LEN  (16U)
#define PICC_RX_LEN         (18U)
#define SER_NUM_LEN_BYTES   (4U)
#define SEC_KEY_LEN         (6U)
#define SECTOR_TRAIL_BLOCK  (3U) // block address of the PICC sector trailer where KEY and access bits are held
#define SECTOR_BLOCK_2      (2U)
#define DEFAULT_SEC_KEY     (0xFFU)
#define USER_SEC_KEYB       (0x88U)
#define ACCESS_BYTE_6       (0xFFU)
#define ACCESS_BYTE_7       (0x07)
#define ACCESS_BYTE_8       (0x80U)
#define MAN_SECTOR_NUM      (0U)
#define MAN_SECTOR_BLOCK    (0U)

#define ITEM_SECTOR         (8U)

/********************
 * ENUMS/STRUCTS
 ********************/
typedef enum {
    TAG_AUTHORIZED,
    TAG_REGISTERED,
    TAG_EXISTS,
    TAG_REMOVED,
    TAG_REJECTED,
    TAG_ERR
} tag_status_e;

typedef enum {
    TAG_PRODUCT,
    TAG_AUTH_CARD
} tag_type_e;

typedef struct {
    uint8_t buf[PICC_RX_LEN];
    uint8_t uid[UID_LEN_BYTES];
    uint8_t sec_key[SEC_KEY_LEN];
} rfid_tag_t;

/********************
 * PUBLIC APIs
 *******************/

/**
 * @brief Initializes the RFID tag reader
 * 
 * 
 * 
 */
uint8_t tag_init(void); 


/**
 * @brief Reads the 16 btyes of data stored in the specified block of the given sector
 * 
 * 
 * 
 * @param[out] uid buffer in which to store tag UID
 * @param[out] tag_data buffer in which to store read data
 * @param[in] sector sector to read from
 * @param[in] block block of sector to read from
 * 
 * @return non-neg number of bits read from PICC on success; otherwise 1
 */
uint8_t tag_read_product_data(uint8_t *prod_name, uint8_t *prod_cond, uint8_t *direction);

/**
 * @brief 
 * 
 * 
 */
uint8_t tag_read_keycard_data(uint8_t *card_data);

#endif