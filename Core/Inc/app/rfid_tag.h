/**
 * Utilizes the MFRC522 driver to implement 
 */

#ifndef _KEYCARD_H
#define _KEYCARD_H

#include "../drivers/mfrc522.h"

#define UID_LEN_BYTES       (4U) // 4 UID BYTES + 1 BCC
#define PICC_MEM_BLOCK_LEN  (16U)
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

#define ITEM_SECTOR         (2U)
#define TYPE_BLOCK          (1U)
#define NAME_BLOCK          (2U)
#define TRAIL_BLOCK         (3U)

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
    TAG_ENTRY1,
    TAG_ENTRY2
} tag_index_e;

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
 * @brief 
 */
tag_status_e tag_quick_scan(void);


/**
 * @brief Registers a tag and saves its serialnumber for subsequent authorization
 * 
 * 
 * 
 * @param uid 4-byte serial serial number passed as a buffer of single bytes
 */
tag_status_e tag_register(tag_index_e tag_entry);


/**
 * @brief Removes a tag from the registry to revoke access
 * 
 * @param uid 4-byte serial num associated with tag to remove
 */
tag_status_e tag_forget(void);



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
uint8_t tag_read_data(uint8_t *uid, uint8_t *tag_data, uint8_t sector, uint8_t block);

#endif