#include "../Inc/app/rfid_tag.h"
#include "../Inc/drivers/spi.h"
#include "../Inc/drivers/mfrc522.h"
#include "common/defines.h"
#include "display.h"
#include <string.h>

#define BLOCKS_PER_SECTOR       (4U)
#define NUM_OF_ALLOWED_TAGS     (2U)
#define UID_FOUND               (1U)
#define UID_UNKNOWN             (0U)

#define ITEM_SECTOR       (8U)
#define TYPE_IDX          (1U)
#define NAME_IDX          (2U)
#define TRAIL_IDX         (3U)

#define NAME_BLOCK          ((ITEM_SECTOR * BLOCKS_PER_SECTOR) + NAME_IDX)
#define TYPE_BLOCK          ((ITEM_SECTOR * BLOCKS_PER_SECTOR) + TYPE_IDX)
#define AUTH_BLOCK          ((ITEM_SECTOR * BLOCKS_PER_SECTOR) + TRAIL_IDX)

#define PAGES_PER_WRITE     (4U) // we write 16 byte data into 4 byte pages
#define COND_PAGE           (12U)
#define NAME_PAGE           (8U)
#define TYPE_PAGE           (4U)

/***********************
 * STATIC DECLARATIONS
 ************************/
static rfid_tag_t active_tag;
static uint8_t default_sec_key[SEC_KEY_LEN] = {DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY, DEFAULT_SEC_KEY};
static uint8_t item_type_block[PICC_MEM_BLOCK_LEN] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0,0,0,0,0,0,0,0,0,0};
const static uint8_t auth_card_type[PICC_MEM_BLOCK_LEN] = {0xca, 0xfe, 0xbe, 0xef, 0xde, 0xad, 0,0,0,0,0,0,0,0,0,0};


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
static mfrc_status_e tag_scan_and_select(tag_type_e type, uint8_t *card_buf, uint8_t *card_uid);


static void reader_spi_init(void);
static uint8_t reader_write_byte(uint8_t byte);
static uint8_t reader_get_byte(void);
static void reader_cs_low(void);
static void reader_cs_high(void);
static uint8_t reader_lock(void);
static void reader_unlock(void);
/********************
 * PUBLIC APIs
 *******************/


static uint8_t initialized = 0;
/**
 * @brief Initializes the RFID keycard reader
 */
uint8_t tag_init(void) {
    if (!initialized) {
        mfrc_reader_t reader = {.init = reader_spi_init, .receive_byte = reader_get_byte, .transmit_byte = reader_write_byte, 
                                .select = reader_cs_low, .deselect = reader_cs_high,
                                .req_bus = reader_lock, .rel_bus = reader_unlock};
        mfrc522_init(&reader);
        initialized = 1;
    }
    return MFRC_OK;
}


uint8_t tag_read_keycard_data(uint8_t *card_data) {
    uint8_t status = STATUS_ERR;
    memset((void *)&active_tag, 0, sizeof(active_tag));

    status = tag_scan_and_select(TAG_AUTH_CARD, active_tag.buf, active_tag.uid);
    if (status) return status;
    // if this fails its not a mifare1k keycard
    status = mfrc522_auth(PICC_AUTH_A, AUTH_BLOCK, default_sec_key, active_tag.uid);
    if (status) goto cleanup;
    status = mfrc_picc_read(TYPE_BLOCK, active_tag.buf);
    if (status) goto cleanup;
    for (uint8_t i = 0; i < PICC_MEM_BLOCK_LEN; ++i) {
        if (active_tag.buf[i] != auth_card_type[i]) {
            status = STATUS_ERR;
            goto cleanup;
        }
    }


cleanup:
    TM_MFRC522_Crypto_Off();
    mfrc_halt();
    display_change_screen(NULL, 0, status);
    return status;
}



uint8_t tag_read_product_data(uint8_t *prod_name, uint8_t *prod_cond) {
    uint8_t status = STATUS_ERR;
    memset((void *)&active_tag, 0, sizeof(active_tag));

    status = tag_scan_and_select(TAG_PRODUCT, active_tag.buf, active_tag.uid);
    if (status) return status;

    HAL_Delay(1);
    status = mfrc_picc_read(TYPE_PAGE, active_tag.buf);
    if (status) goto cleanup;
    for(uint8_t i = 0; i < PICC_MEM_BLOCK_LEN; ++i) { // compare type block read with expected type value
        if (active_tag.buf[i] != item_type_block[i]) {
            status = STATUS_ERR;
            goto cleanup;
        }
    }

    status = mfrc_picc_read(NAME_PAGE, prod_name);
    if (status) goto cleanup;
    status = mfrc_picc_read(COND_PAGE, prod_cond);
    if (status) goto cleanup;

cleanup:
    mfrc_halt();
    display_change_screen(NULL, 0, status);
    return status;
}



/**********************
 * STATIC DEFS
 *********************/


static mfrc_status_e tag_scan_and_select(tag_type_e type, uint8_t *card_buf, uint8_t *card_uid) {
   mfrc_status_e mfrc_stat = MFRC_ERR;

    mfrc_stat = mfrc_request(PICC_WUPA, card_buf);
    if (mfrc_stat != MFRC_OK) goto sel_exit;
    // 2. perform anticollision loop to retrieve id
    // display_change_screen(NULL, 0); // begins tag scanning screen cycle
    HAL_Delay(1);
    mfrc_stat = mfrc_anticollision(card_buf, MFRC_AC_CL1);
    if (mfrc_stat != MFRC_OK) goto sel_exit;

    uint8_t uid_idx = 0;
    for (uint8_t i = 0; i < SER_NUM_LEN_BYTES; ++i) {
        if (type == TAG_PRODUCT && i == 0) // NFC215 first byte is Cascade Tag
                continue;
        card_uid[uid_idx++] = card_buf[i];
    }
    // 3. select tag
    HAL_Delay(1);
    mfrc_stat = mfrc_select_picc(card_buf, MFRC_SEL_CL1);

    if (mfrc_stat == MFRC_OK && type == TAG_PRODUCT) {
        mfrc_stat = mfrc_anticollision(card_buf, MFRC_AC_CL2); // read next 4 bytes of UID
        if (mfrc_stat != MFRC_OK) goto sel_exit;
        for (uint8_t i = 0; i < SER_NUM_LEN_BYTES; ++i)
            card_uid[uid_idx++] = card_buf[i];
        
        mfrc_stat = mfrc_select_picc(card_buf, MFRC_SEL_CL2);
    }

sel_exit:
    return mfrc_stat;
}




static void reader_spi_init(void) {
    spi_init(SPI_DEVICE_MFRC522);
}


static uint8_t reader_write_byte(uint8_t byte) {
    return spi_transmit(&byte, 1);
}


static uint8_t reader_get_byte(void) {
    uint8_t byte = 0x00;
    spi_receive(&byte, 1);
    return byte;
}
static void reader_cs_low(void) {
    io_set_out(IO_SPI_CS_MFRC, LOW);
}
static void reader_cs_high(void) {
    io_set_out(IO_SPI_CS_MFRC, HIGH);
}
static uint8_t reader_lock(void) {
    return 1;
}
static void reader_unlock(void) {
    __NOP();
}