#include "../../Inc/app/rfid_tag.h"
#include "../../Inc/drivers/spi.h"
#include "../../Inc/drivers/mfrc522.h"
#include "../../../Core/Inc/common/defines.h"

#define BLOCKS_PER_SECTOR   (4U)
#define NUM_OF_ALLOWED_TAGS     (2U)
#define UID_FOUND   (1U)
#define UID_UNKNOWN (0U)

#define ITEM_SECTOR       (2U)
#define TYPE_IDX          (1U)
#define NAME_IDX          (2U)
#define TRAIL_IDX         (3U)

#define NAME_BLOCK          ((ITEM_SECTOR * BLOCKS_PER_SECTOR) + NAME_IDX)
#define TYPE_BLOCK          ((ITEM_SECTOR * BLOCKS_PER_SECTOR) + TYPE_IDX)
#define AUTH_BLOCK          ((ITEM_SECTOR * BLOCKS_PER_SECTOR) + TRAIL_IDX)

typedef struct {
    uint8_t buf[PICC_MEM_BLOCK_LEN];
    uint8_t uid[UID_LEN_BYTES];
    uint8_t sec_key[SEC_KEY_LEN];
} rfid_tag_t;

/***********************
 * STATIC DECLARATIONS
 ************************/
static uint8_t default_sec_key[SEC_KEY_LEN] = {DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY};
static uint8_t item_block_buf[PICC_MEM_BLOCK_LEN] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0,0,0,0,0,0,0,0,0,0};
static uint8_t card_block_buf[PICC_MEM_BLOCK_LEN] = {0xca, 0xfe, 0xbe, 0xef, 0xde, 0xad, 0,0,0,0,0,0,0,0,0,0};

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
uint8_t tag_register(tag_type_e type, const uint8_t *data_buffer) {
    uint8_t status = STATUS_OK;
    rfid_tag_t tag;

    status = tag_scan_and_select(tag.buf, tag.uid);
    if (status == STATUS_OK) {
        status = mfrc522_auth(PICC_AUTH_A, AUTH_BLOCK, default_sec_key, tag.uid);
        if (status == STATUS_OK) {
            switch (type) {
            case TAG_PRODUCT:
                status = mfrc_picc_write(TYPE_BLOCK, item_block_buf);
                if (status == STATUS_OK)
                    status = mfrc_picc_write(NAME_BLOCK, data_buffer);
                break;
            case TAG_AUTH_CARD:
                status = mfrc_picc_write(TYPE_BLOCK, card_block_buf);
                break;
            }
            TM_MFRC522_Crypto_Off();
        }
    }
    mfrc_halt();

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
            for (uint8_t i = 0; i < SER_NUM_LEN_BYTES; ++i)
                card_uid[i] = card_buf[i];
            // 3. select tag
            HAL_Delay(1);
            mfrc_stat = mfrc_select_picc(card_buf);
        }
    }

    return mfrc_stat;
}