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
static rfid_tag_t active_tag;
static uint8_t default_sec_key[SEC_KEY_LEN] = {DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY};

static uint8_t registered_keys[NUM_OF_ALLOWED_TAGS][UID_LEN_BYTES] = {
    [TAG_ENTRY1] = {0x00,0x00,0x00,0x00,0x00}, 
    [TAG_ENTRY2] = {0x00,0x00,0x00,0x00,0x00}
};
static uint8_t xor_cipher[SEC_KEY_LEN][UID_LEN_BYTES] = {
    // SEC_KEY_BYTE1
    {0x12, 0x36, 0x77, 0x89},
    //SEC_KEY_BYTE2
    {0x67, 0x89, 0x43, 0x32},
    // SEC_KEY_BYTE3
    {0x51, 0x99, 0xAB, 0xBD},
    //SEC_KEY_BYTE4
    {0x4D, 0xF3, 0x7C, 0xEF},
    // SEC_KEY_BYTE5
    {0x04, 0x29, 0xA7, 0xBF},
    //SEC_KEY_BYTE6
    {0x14, 0xE9, 0xAA, 0xCB},
};


/**
 * @brief Utilizes a XOR cipher to set the SECTOR KEY
 * 
 * Applies a XOR cipher to the card's UID and utilizes this as the 
 * card's new type A sector key.
 * 
 * @param sec_key sector key that is being scrambled
 * @param uid the UID of the associated card
 */
static void scramble_key(uint8_t *sec_key, uint8_t *uid);

/**
 * @brief Applies the XOR cipher to scrambled key to obtain original key
 * 
 * @param sec_key the sector key to unscramble
 */
static void unscramble_key(uint8_t *sec_key);

/**
 * @brief Checks if UID is known or not
 * 
 * Compares the specified UID against all stored UIDs, if any are a match, 
 * return card authorized. else rejected
 * 
 * @param uid the UID to verify
 * @return 0 if UID unknown; 1 else
 */
static uint8_t search_uid(uint8_t *uid, uint8_t *idx);



/**
 * @brief
 */
static mfrc_status_e tag_scan_and_select(uint8_t *card_buf, uint8_t *card_uid);

/********************
 * PUBLIC APIs
 *******************/

/**
 * @brief Initializes the RFID keycard reader
 */
uint8_t tag_init(void) {
    mfrc522_init();
    return MFRC_OK;
}


/**
 * @brief Quick card scan utilizes saved UIDs
 * 
 * This API is used for quick verification of a tag. Skips 
 * card selectiona and authentication and simply retrieves UID and 
 * compares again a collection of saved UIDs.
 * 
 * @return TAG_AUTHORIZED on success
 */
tag_status_e tag_quick_scan(void) {
    tag_status_e card_stat = TAG_REJECTED;
    mfrc_status_e status = MFRC_ERR;
    memset((void *)&active_tag, 0, sizeof(active_tag));
    uint8_t dummy_idx = 0x00;

    // 1. sent WAKEUP request to all nearby PICCs
    status = mfrc_request(PICC_WUPA, active_tag.buf);
    if (status == MFRC_OK) {
        // 2. perform anticollision loop to retrieve UID
        HAL_Delay(1);
        status = mfrc_anticollision(active_tag.buf);
        if (status == MFRC_OK) {
            for (uint8_t i = 0; i < UID_LEN_BYTES; ++i)
                active_tag.uid[i] = active_tag.buf[i];
            // 3. compare retrieved UID against stored UIDs
            uint8_t match = search_uid(active_tag.uid, &dummy_idx);
            if (match)
                card_stat = TAG_AUTHORIZED; // 4. If match return authorized, else rejected
        }
    }
    return card_stat;
}


/**
 * @brief Registers a tag and saves its serialnumber for subsequent authorization
 */
tag_status_e tag_register(tag_index_e tag_entry) {
    tag_status_e card_stat = TAG_ERR;
    mfrc_status_e mfrc_stat = MFRC_ERR;
    memset((void *)&active_tag, 0, sizeof(active_tag));

    // 1. send request message from PCD
    mfrc_stat = tag_scan_and_select(active_tag.buf, active_tag.uid);
    if (mfrc_stat == MFRC_OK) {
        // AUTHENTICATE WITH DEFAULT KEY WHEN REGISTERING
        HAL_Delay(1);
        mfrc_stat = mfrc522_auth(PICC_AUTH_A, SECTOR_TRAIL_BLOCK, default_sec_key, active_tag.uid);
        if (mfrc_stat == MFRC_OK) {
            scramble_key(active_tag.buf, active_tag.uid);
            active_tag.buf[6] = ACCESS_BYTE_6;
            active_tag.buf[7] = ACCESS_BYTE_7;
            active_tag.buf[8] = ACCESS_BYTE_8;
            // set remaining PICC block bytes to 0x88 (unused)
            for (uint8_t i = 0; i < SEC_KEY_LEN + 1; ++i)
                active_tag.buf[i + 9] = DEFAULT_SEC_KEY;
            // 5. overwrite with new sector key in sector trailer
            HAL_Delay(1);
            mfrc_stat = mfrc_picc_write(SECTOR_TRAIL_BLOCK, active_tag.buf);
            if (mfrc_stat == MFRC_OK) {
                // 6. send halt command
                card_stat = TAG_REGISTERED;
                for (uint8_t i = 0; i < UID_LEN_BYTES; ++i)
                    registered_keys[tag_entry][i] = active_tag.uid[i];
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


/**
 * @brief Removes a tag from the registry to revoke access
 */
tag_status_e tag_forget(void) {
    tag_status_e card_stat = TAG_ERR;
    mfrc_status_e mfrc_stat = MFRC_ERR;
    memset((void *)&active_tag, 0, sizeof(active_tag));

    uint8_t card_idx = -1;
    mfrc_stat = tag_scan_and_select(active_tag.buf, active_tag.uid);
    if (mfrc_stat == MFRC_OK && search_uid(active_tag.uid, &card_idx) == UID_FOUND) {
        // 4. authenticate
        scramble_key(active_tag.sec_key, active_tag.uid);
        HAL_Delay(1);
        mfrc_stat = mfrc522_auth(PICC_AUTH_A, SECTOR_TRAIL_BLOCK, active_tag.sec_key, active_tag.uid);
        if (mfrc_stat == MFRC_OK) {
            // 5. overwrite content in the sector trailer
            for (uint8_t i = 0; i < SEC_KEY_LEN; ++i)
                active_tag.buf[i] = DEFAULT_SEC_KEY;
            active_tag.buf[6] = ACCESS_BYTE_6;
            active_tag.buf[7] = ACCESS_BYTE_7;
            active_tag.buf[8] = ACCESS_BYTE_8;
            for (uint8_t i = 0; i < SEC_KEY_LEN + 1; ++i)
                active_tag.buf[i + 9] = DEFAULT_SEC_KEY;
            HAL_Delay(1);
            mfrc_stat = mfrc_picc_write(SECTOR_TRAIL_BLOCK, active_tag.buf);
            if (mfrc_stat == MFRC_OK) {
                for (uint8_t i = 0; i < UID_LEN_BYTES; ++i)
                    registered_keys[card_idx][i] = 0x00;
                card_stat = TAG_REMOVED;
            }
            TM_MFRC522_Crypto_Off();
        }
        mfrc_halt();
    }

    return card_stat;
}



uint8_t tag_read_data(uint8_t *uid, uint8_t *tag_data, uint8_t sector, uint8_t block) {
    uint8_t status = STATUS_OK;
    memset((void *)&active_tag, 0, sizeof(active_tag));

    status = tag_scan_and_select(active_tag.buf, uid);
    if (status == STATUS_OK) {
        status = mfrc522_auth(PICC_AUTH_A, (sector * BLOCKS_PER_SECTOR) + SECTOR_TRAIL_BLOCK, active_tag.sec_key, uid);

        if (status == STATUS_OK) {
            status = mfrc_picc_read(block, tag_data);
            TM_MFRC522_Crypto_Off();
        }
    }

    return status;
}



/**********************
 * STATIC DEFS
 *********************/

static void scramble_key(uint8_t *sec_key, uint8_t *uid) {
    for (uint8_t i = 0; i < SEC_KEY_LEN; ++i) {
        sec_key[i] = 0;
        for (uint8_t j = 0; j < UID_LEN_BYTES - 1; ++j)
            sec_key[i] += (xor_cipher[i][j] ^ uid[j]);
    }
}


static void unscramble_key(uint8_t *sec_key) {
    for (uint8_t i = 0; i < SEC_KEY_LEN; ++i) {
        uint8_t scrambled_byte = sec_key[i];
        sec_key[i] = 0;
        for (uint8_t j = 0; j < UID_LEN_BYTES - 1; ++j)
            sec_key[i] += (xor_cipher[i][j] ^ scrambled_byte); // TODO: FIX THIS WE CANT UNSCRAMBLE THE BYTE DIRECTLY
    }
}

static uint8_t search_uid(uint8_t *uid, uint8_t *idx) {
    uint8_t match = 0;
    for (uint8_t i = 0; i < NUM_OF_ALLOWED_TAGS; ++i) {
        match = mfrc_compare(uid, registered_keys[i]);
        if (match) {
            *idx = i;
            break;
        }
    }
    return match;
}

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