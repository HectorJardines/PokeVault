#include "../Inc/app/rfid_tag.h"
#include "../Inc/drivers/spi.h"
#include "../Inc/drivers/mfrc522.h"
#include "common/defines.h"

#define BLOCKS_PER_SECTOR   (4U)
#define NUM_OF_ALLOWED_TAGS     (2U)
#define UID_FOUND   (1U)
#define UID_UNKNOWN (0U)

typedef struct {
    uint8_t buf[PICC_MEM_BLOCK_LEN];
    uint8_t uid[UID_LEN_BYTES];
    uint8_t sec_key[SEC_KEY_LEN];
} rfid_tag_t;

/***********************
 * STATIC DECLARATIONS
 ************************/
static uint8_t default_sec_key[SEC_KEY_LEN] = {DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY};

/**
 * @brief Scan tag and set its state to active mode
 * 
 * This function is called to scan a nearby PICC and set it 
 * into the active mode. In the active mode the PICC is able
 * to receive read, write, and authorization commands.
 * 
 * @param[in] card_buf
 * @param[out] card_uid
 */
static mfrc_status_e tag_scan_and_select(uint8_t *card_buf, uint8_t *card_uid);

/********************
 * PUBLIC APIs
 *******************/

/**
 * @brief Initializes the RFID keycard reader
 */
tag_status_e tag_init(void) {
    mfrc522_init();
    return MFRC_OK;
}


/**
 * @brief Registers a tag and saves its serialnumber for subsequent authorization
 */
tag_status_e tag_register(const char *data_buffer) {
    tag_status_e card_stat = TAG_ERR;
    mfrc_status_e mfrc_stat = MFRC_ERR;
    rfid_tag_t tag;


    // 1. send request message from PCD
    mfrc_stat = tag_scan_and_select(tag.buf, tag.uid);
    if (mfrc_stat == MFRC_OK) {
        // AUTHENTICATE WITH DEFAULT KEY WHEN REGISTERING
        HAL_Delay(1);
        mfrc_stat = mfrc522_auth(PICC_AUTH_A, SECTOR_TRAIL_BLOCK, default_sec_key, tag.uid);
        if (mfrc_stat == MFRC_OK) {
            scramble_key(tag.buf, tag.uid);
            tag.buf[6] = ACCESS_BYTE_6;
            tag.buf[7] = ACCESS_BYTE_7;
            tag.buf[8] = ACCESS_BYTE_8;
            // set remaining PICC block bytes to 0x88 (unused)
            for (uint8_t i = 0; i < SEC_KEY_LEN + 1; ++i)
                tag.buf[i + 9] = DEFAULT_SEC_KEY;
            // 5. overwrite with new sector key in sector trailer
            HAL_Delay(1);
            mfrc_stat = mfrc_picc_write(SECTOR_TRAIL_BLOCK, tag.buf);
            if (mfrc_stat == MFRC_OK) {
                // 6. send halt command
                card_stat = TAG_REGISTERED;
            }
            TM_MFRC522_Crypto_Off();
        }
        else
            card_stat = TAG_EXISTS; // default SECTOR KEY didn't work must've already registered this card

        mfrc_halt();
    }

    if (mfrc_stat != MFRC_OK) {
        card_stat = TAG_ERR;
    }

    return card_stat;
}



uint8_t tag_read_data(uint8_t *uid, uint8_t *tag_data, uint8_t sector, uint8_t block) {
    uint8_t status = STATUS_OK;
    rfid_tag_t tag;

    status = tag_scan_and_select(tag.buf, uid);
    if (status == STATUS_OK) {
        status = mfrc522_auth(PICC_AUTH_A, (sector * BLOCKS_PER_SECTOR) + SECTOR_TRAIL_BLOCK, tag.sec_key, uid);

        if (status == STATUS_OK) {
            status = mfrc_picc_read(block, tag_data);
            TM_MFRC522_Crypto_Off();
        }
    }

    return status;
}



uint8_t tag_write_data(uint8_t *tag_data, uint8_t sector, uint8_t block) {
    uint8_t status = STATUS_OK;
    rfid_tag_t tag;
    
    status = tag_scan_and_select(tag.buf, tag.uid);
    if (status == STATUS_OK) {
        status = mfrc522_auth(PICC_AUTH_A, (sector * BLOCKS_PER_SECTOR) + SECTOR_TRAIL_BLOCK, tag.sec_key, tag.uid);
        if (status == STATUS_OK) {
            status = mfrc_picc_write(block, tag_data);
            TM_MFRC522_Crypto_Off();
        }
    }

    return status;
}



/**********************
 * STATIC DEFS
 *********************/

static mfrc_status_e tag_scan_and_select(uint8_t *card_buf, uint8_t *card_uid) {
    mfrc_status_e mfrc_stat = MFRC_ERR;

    mfrc_stat = mfrc_request(PICC_WUPA, card_buf);
    if (mfrc_stat == MFRC_OK) {
        // 2. perform anticollision loop to retrieve id
        HAL_Delay(1);
        mfrc_stat = mfrc_anticollision(card_buf);
        if (mfrc_stat == MFRC_OK) {
            for (uint8_t i = 0; i < UID_LEN_BYTES; ++i)
                card_uid[i] = card_buf[i];
            // 3. select tag
            HAL_Delay(1);
            mfrc_stat = mfrc_select_picc(card_buf);
        }
    }

    return mfrc_stat;
}