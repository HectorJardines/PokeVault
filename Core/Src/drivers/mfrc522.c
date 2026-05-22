#include "../Inc/drivers/mfrc522.h"
#include "../Inc/drivers/io.h"
#include "../Inc/drivers/spi.h"

#define MFRC522_CS_LOW  (GPIOA->BSRR |= (1 << (IO_PIN_10 + IO_BSRR_BR_OFFSET)))
#define MFRC522_CS_HIGH (GPIOA->BSRR |= (1 << IO_PIN_10))
#define MFRC_MAX_FIFO_LEN       (64U)
#define PICC_UID_LEN_BYTES      (4U)
#define PICC_UID_CLn_LEN        (5U)

#define PICC_DATA_BLOCK_LEN     (128U) // length is in bits
#define PICC_CRC_LEN            (16U) // length in bits
#define PICC_NUM_ACK_BITS       (4U)
#define PICC_ACK                (0x0A)

#define PICC_DB_LEN_BYTES   (16U)
#define PICC_CRC_LEN_BYTES  (2U)
#define PICC_DB_PAYLOAD_LEN (PICC_DB_LEN_BYTES + PICC_CRC_LEN_BYTES) // the length of a write payload for MIFARE data block

#define MF_SECTOR_KEY_LEN   (6U)
#define MF_SERNUM_LEN       (4U)
#define MF_AUTH_PAYLOAD_LEN (2 + MF_SECTOR_KEY_LEN + MF_SERNUM_LEN) // 12 bytes

// MAX 10Mbits/s data speed
#define MFRC_ADDR_SET_READ(addr)    (((addr << 1) & 0x7E) | 0x80) // set 6th bit (mode) shift to clear LSB and move mode to 7th bit
#define MFRC_ADDR_SET_WRITE(addr)   ((addr << 1) & 0x7E) // clear 6th bit (mode) shift to clear LSB and move mode to 7th bit

#define SINGLE_REG_READ     (1U)
#define SINGLE_REG_WRITE    (SINGLE_REG_READ)

/*************************
 *  STATIC DECLARATIONS
 *************************/
// APIs to modify PCD registers
static uint8_t clear_bitmask_on_reg(uint8_t reg, uint8_t reg_msk);
static uint8_t set_bitmask_on_reg(uint8_t reg, uint8_t reg_msk);
static uint8_t write_mfrc_register(uint8_t reg, uint8_t data);
static uint8_t read_mfrc_register(uint8_t reg);

// general purpose APIs
static uint8_t mfrc_reset(void);
static uint8_t mfrc_antenna_on(void);
static uint8_t mfrc_antenna_off(void);
static uint8_t mfrc_send_to_picc(uint8_t command, uint8_t *send_data, uint8_t send_len, uint8_t *rcv_data, uint16_t *rcv_len);
static uint8_t write_pcd_cmd(uint8_t pcd_cmd);
static uint8_t mfrc_calculate_crc(uint8_t *checksum_data, uint8_t data_len, uint8_t *checksum);

/**************************
 *      PUBLIC APIs
 **************************/

 /**
 * @brief Intializes and configures the MFRC522 device
 */
void mfrc522_init(void) {
    // intialize spi peripheral
    spi_init(SPI_DEVICE_MFRC522);
    // reset mfrc522
    mfrc_reset();

    //configure mfrc522 timer 15ms delay
    uint8_t value = 0x8D; //
    write_mfrc_register(MFRC_TMODER, value);
    value = 0x3E; //
    write_mfrc_register(MFRC_TPSCR, value);
    value = 30; // timer count to 30 ticks
    write_mfrc_register(MFRC_TRELOADR_L, value);
    value = 0; // high bits of timer reload cleared
    write_mfrc_register(MFRC_TRELOADR_H, value);

    value = 0x70; // 48dB gain
    write_mfrc_register(MFRC_RFCfgR, value);

    value = 0x40; // set 100% ASK rate
    write_mfrc_register(MFRC_TX_ASKR, value);

    value = 0x3D; // 
    write_mfrc_register(MFRC_MODER, value);

    mfrc_antenna_on();
}

/**
 * @brief Scans for nearby PICCs, stores the associated UID if found
 */
uint8_t mfrc_scan(uint8_t *uid) {
    mfrc_status_e status = MFRC_OK;
    // send request
    status = mfrc_request(PICC_REQA, uid);
    if (status == MFRC_OK) {
        status = mfrc_anticollision(uid);
    }
    status = mfrc_halt(); // halt the card 
    return status;
}

/**
 * @brief Compares the keycard id with stored ID
 * 
 * @param card_id RFID associated with the keycard
 * @param registered_id predefined RFID
 * 
 * @return 0 if IDs differ; 1 else
 */
uint8_t mfrc_compare(uint8_t *id, uint8_t *comp_id) {
    uint8_t check = 1;
    for (uint8_t i = 0; i < PICC_UID_LEN_BYTES; ++i) {
        if (id[i] != comp_id[i])
            check = 0;
    }
    return check;
}



/**
 * @brief the PCD must send repeated request commands to nearby PICCs
 * 
 * There are two types of requests, REQA and REQB as defined by ISO-IEC
 * 14443. This API sends REQA commands and receives an ATQA (answer to request type A)
 * and selects a PICC to communicate with.
 * 
 * @param 
 */
uint8_t mfrc_request(uint8_t request_type, uint8_t *picc_type) {
    uint16_t rcv_len;
    mfrc_status_e status = MFRC_OK;

    uint8_t value = 0x07; // 7 bits of last byte will be transmitted
    write_mfrc_register(MFRC_BIT_FRAMING, value);

    picc_type[0] = request_type;
    status = mfrc_send_to_picc(PCD_CMD_TRANSCEIVE, picc_type, 1, picc_type, &rcv_len);

    if (status != MFRC_OK || rcv_len != 0x10) // 16 bit 
        status = MFRC_ERR;
    return status;
}



/**
 * @brief 
 */
uint8_t mfrc_anticollision(uint8_t *uid) {
    uint8_t uid_check = 0;
    uint8_t val = 0x00; // all bits of last byte in TX sequence will be transmitted
    write_mfrc_register(MFRC_BIT_FRAMING, val);

    uint16_t uid_len = 0;
    uid[0] = PICC_ANTICOLL;
    uid[1] = 0x20; // indicates to nearby PICCs that no part of UID will be sent, just send full UID
    mfrc_status_e status = mfrc_send_to_picc(PCD_CMD_TRANSCEIVE, uid, 2, uid, &uid_len); // sent PICC anticolll command and retrieve UID

    if (status == MFRC_OK) { // verify that UID is expected
        for (uint8_t i = 0; i < PICC_UID_LEN_BYTES; i++)
            uid_check ^= uid[i]; // XOR all 4 byts of the checksum, this should = the Block Check Character
        if (uid_check != uid[PICC_UID_LEN_BYTES]) // the 5th byte in the UID CLn is the Block Check Character checksum 
            status = MFRC_ERR;
    }
    return status;
}



/**
 * @brief Selects a nearby PICC to continue communication with
 * 
 * ...
 * 
 * @param uid the UID of the PICC to be selected
 */
uint8_t mfrc_select_picc(uint8_t *uid) {
    mfrc_status_e status = MFRC_OK;
    uint16_t rx_len;
    // 1 byte picc select cmd, 1 byte NVB, 5 bytes UID, and 2 bytes for CRC checksum
    uint8_t tx_buf[9];

    tx_buf[0] = PICC_SEL_CL1;
    tx_buf[1] = 0x70; // PCD will send full uid CLn, only after no collisions detected

    // copy UID to tx buffer
    for (uint8_t i = 0; i < PICC_UID_CLn_LEN; ++i)
        tx_buf[i + 2] = *(uid + i);

    // calculate checksum over buffer contents to transmit as per ISO-IEC 14443
    mfrc_calculate_crc(tx_buf, 7, &tx_buf[7]);

    status = mfrc_send_to_picc(PCD_CMD_TRANSCEIVE, tx_buf, 9, tx_buf, &rx_len);

    return status;
}



/**
 * @brief Performs keycard RFID authentication
 * 
 * @return 0 if authentication fails; 1 else
 */
uint8_t mfrc522_auth(uint8_t auth_type, uint8_t picc_block_addr, uint8_t *sector_key, uint8_t *serial_num) {
    mfrc_status_e status = MFRC_OK;
    uint8_t buffer[MF_AUTH_PAYLOAD_LEN];

    // intialize timer to auto mode, use idleIRq/timerIRq as termination criteria if no card read
    set_bitmask_on_reg(MFRC_TMODER, 0x80); // timer auto starts at end of transmission
    clear_bitmask_on_reg(MFRC_TMODER, 0x10); // timer counts down to 0 and sets comIRq bit

    // write auth cmd, block addr, sector key, uid into DR
    buffer[0] = auth_type;
    buffer[1] = picc_block_addr;
    uint8_t i;
    for (i = 0; i < MF_SECTOR_KEY_LEN; ++i)
        buffer[i + 2] = *(sector_key + i);
    for (i = 0; i < MF_SERNUM_LEN; ++i)
        buffer[i + 8] = *(serial_num + i);

    // activate MFAuthent command
    status = mfrc_send_to_picc(PCD_CMD_MF_AUTH, buffer, MF_AUTH_PAYLOAD_LEN, NULL, NULL);

    //wait until authentication sequence complete
    uint8_t retries = 0xFF;
    uint8_t status2_reg = 0x00;
    while (retries > 0) {
        status2_reg = read_mfrc_register(MFRC_STAT2_REG);
        if (status2_reg & 0x08) // MFCrypto1On bit set, MFAuthent success
            break;
        retries--;
    }

    if (retries <= 0 || status != MFRC_OK)
        status = MFRC_ERR;

    return status;
}



void TM_MFRC522_Crypto_Off(void) {
    clear_bitmask_on_reg(MFRC_STAT2_REG, 0x08);
}



/**
 * @brief Puts the PICC associated with uid into HALT state
 * 
 * @param uid uid of card to put into HALT state
 */
uint8_t mfrc_halt(void) {
    mfrc_status_e status = MFRC_OK;
    uint8_t buffer[PICC_CRC_LEN + 2];
    uint16_t rcv_len = 0;

    buffer[0] = PICC_HALT;
    buffer[1] = 0x00;
    mfrc_calculate_crc(buffer, 2, &buffer[2]);
    status = mfrc_send_to_picc(PCD_CMD_TRANSCEIVE, buffer, PICC_UID_LEN_BYTES + 2, buffer, &rcv_len);
    return status;
}



/**
 * @brief Read 16 bytes of data from the specified PICC address block
 * 
 * @param picc_addr_block addres of the block to read from
 * @param rcv_data buffer into which data will be read
 */
uint8_t mfrc_picc_read(uint8_t picc_block_addr, uint8_t *rcv_data) {
    mfrc_status_e status = MFRC_OK;
    uint8_t buffer[4];
    uint16_t rcv_len;

    // setup PICC read command as per MIFARE datasheet
    buffer[0] = PICC_READ;
    buffer[1] = picc_block_addr;
    mfrc_calculate_crc(buffer, 2, &buffer[2]);

    status = mfrc_send_to_picc(PCD_CMD_TRANSCEIVE, buffer, 4, rcv_data, &rcv_len);
    if (status != MFRC_OK || rcv_len != (PICC_DATA_BLOCK_LEN + PICC_CRC_LEN))
        status = MFRC_ERR;
    return status;
}

/**
 * @brief Write 16 bytes to the specified PICC address block
 * 
 * @param picc_block_addr address of the block to write to
 * @param send_data buffer of data to write to block
 */
uint8_t mfrc_picc_write(uint8_t picc_block_addr, uint8_t *send_data) {
    mfrc_status_e status = MFRC_OK;
    uint8_t buffer[PICC_DB_PAYLOAD_LEN];
    uint16_t rcv_len_bits;

    buffer[0] = PICC_WRITE;
    buffer[1] = picc_block_addr;
    mfrc_calculate_crc(buffer, 2, &buffer[2]); // calculate and store checksum
    status = mfrc_send_to_picc(PCD_CMD_TRANSCEIVE, buffer, 4, buffer, &rcv_len_bits); // picc only sends back ACK
    if (status != MFRC_OK || rcv_len_bits != PICC_NUM_ACK_BITS || (buffer[0] & 0x0F) != PICC_ACK)
        return MFRC_ERR;

    for (uint8_t i = 0; i < PICC_DB_LEN_BYTES; ++i)
        buffer[i] = *(send_data + i);

    mfrc_calculate_crc(buffer, PICC_DB_LEN_BYTES, &buffer[PICC_DB_LEN_BYTES]);
    status = mfrc_send_to_picc(PCD_CMD_TRANSCEIVE, buffer, PICC_DB_PAYLOAD_LEN, buffer, &rcv_len_bits);

    if (status != MFRC_OK || rcv_len_bits != PICC_NUM_ACK_BITS || (buffer[0] & 0x0F) != PICC_ACK)
        status = MFRC_ERR;

    return status;
}

/**************************
 *  STATIC DEFINITIONS
 **************************/

/**
 * @brief Calculates a CRC checksum over the input data and stores the checksum value
 * 
 * ...
 * 
 * @param checksum_data
 * @param data_len
 * @param checksum
 */
static uint8_t mfrc_calculate_crc(uint8_t *checksum_data, uint8_t data_len, uint8_t *checksum) {
    mfrc_status_e status = MFRC_OK;
    // set coprocessor preset && clear crc_irq value
    // set_bitmask_on_reg(MFRC_MODER, 0x01); // set CRC coprocessor preset value to 0x6363
    clear_bitmask_on_reg(MFRC_DIV_IRQ, 0x04); // clears the CRCIRq bit

    // clear FIFO
    set_bitmask_on_reg(MFRC_FIFO_LVL, 0x80);
    
    // add checksum_data to FIFO
    for (uint8_t i = 0; i < data_len; ++i)
        write_mfrc_register(MFRC_FIFO_DR, checksum_data[i]);
    
    // send calc CRC command
    write_pcd_cmd(PCD_CMD_CALC_CRC);
    // send idle command when complete
    uint8_t retries = 0xFF;
    uint8_t irq_status = 0x00;
    while (retries > 0) {
        irq_status = read_mfrc_register(MFRC_DIV_IRQ);
        if (irq_status & 0x04) // CRC COMPLETE FLAG
            break;
        retries--;
    }

    if (retries <= 0)
        status = MFRC_ERR;
    // read msb and lsb from CRC reg
    checksum[0] = read_mfrc_register(MFRC_CRC_RESR_LSB); // read 8 MSB into checksum[0:7]
    checksum[1] = read_mfrc_register(MFRC_CRC_RESR_MSB); // read 8 LSB into checksum[8:15] 

    return status;
}

static uint8_t mfrc_antenna_on(void) {
    uint8_t curr_reg = read_mfrc_register(MFRC_TX_CTLR);
    uint8_t rslt = 0;
    if (!(curr_reg & 0x03)) {
        rslt = set_bitmask_on_reg(MFRC_TX_CTLR, 0x03); // activate 13.56MHz signal on TX1/2 pins
    }
    return rslt;
}

static uint8_t mfrc_antenna_off(void) {
    // clear lower two bits which control TX1/1 RF pins
    return clear_bitmask_on_reg(MFRC_TX_CTLR, 0x03); 
}

/**
 * @brief Reset the MFRC522 device
 */
static uint8_t mfrc_reset(void) {
    return write_pcd_cmd(PCD_CMD_SOFT_RST);
}

/**
 * @brief writes command and data to the PICC and receives bytes if applicable
 * 
 * There are two PCD commands that interface with the PICC, MF_AUTH and TRANSCEIVE. 
 * The former is used to authenticate a MiFare card, the latter is used to send data 
 * and receive data from the PICC. Data is sent in the form of PICC commands.
 * 
 * @param command the PCD command to executes
 * @param send_data data to send to the PICC
 * @param send_len length of the data to send to PICC in bytes
 * @param rcv_data data received from the PICC in the case of transceive command sent
 * @param rcv_len number of bits read from the PICC
 */
static uint8_t mfrc_send_to_picc(uint8_t command, uint8_t *send_data, uint8_t send_len, uint8_t *rcv_data, uint16_t *rcv_len) {
    mfrc_status_e status = MFRC_ERR;
    uint8_t irq_en = 0x00;
    uint8_t wait_irq = 0x00;
    uint16_t retries = 2000;
    switch (command) {
        case PCD_CMD_MF_AUTH:
            irq_en = 0x12; // enable idle irq and error irq
            wait_irq = 0x10; // check idle interrupt bit set
            break;
        case PCD_CMD_TRANSCEIVE:
            irq_en = 0x77; // enable idle, tx, and rx irq and timer, error, and LoAlert irq
            wait_irq = 0x30; // check idle or rx interrupt bit set
            break;
    }
    
    uint8_t value = irq_en | 0x80;
    write_mfrc_register(MFRC_COM_INT_EN, value);
    clear_bitmask_on_reg(MFRC_COM_IRQ, 0x80); // all marked bits in ComIrqReg cleared
    set_bitmask_on_reg(MFRC_FIFO_LVL, 0x80); // flush the FIFO contents
    
    write_pcd_cmd(PCD_CMD_IDLE); // set reader in idle mode

    // write data to the FIFO
    for (uint8_t i = 0; i < send_len; ++i)
        write_mfrc_register(MFRC_FIFO_DR, send_data[i]); // VERIFY THAT READER ALLOWS SEQUENTIAL WRITES

    // enable the command to send FIFO contents
    write_pcd_cmd(command);
    if (command == PCD_CMD_TRANSCEIVE) {
        set_bitmask_on_reg(MFRC_BIT_FRAMING, 0x80); // starts transmission of data in transceive mode
    }

    // loop until expected irq bits are set or error/timeout
    uint8_t irq_status = 0;
    while (retries > 0) {
        irq_status= read_mfrc_register(MFRC_COM_IRQ);
        if ((irq_status & wait_irq) || (irq_status & 0x01)) // check if timer timeout or if irq we set earlier occurrred (i.e. RX irq)
            break;
        retries--;
    }
    clear_bitmask_on_reg(MFRC_BIT_FRAMING, 0x80); // end data transmission

    if (retries != 0) {
        uint8_t err_check = 0x00;
        err_check = read_mfrc_register(MFRC_ERR_REG);
        if (!(err_check & 0x1B)) { // check for RX buffer overflow, collision err, or RX CRC err
            status = MFRC_OK;
            if (irq_status & irq_en & 0x01) // checks timeout irq bit + enabled interrupts above
                status = MFRC_TIMEOUT;
            
            if (command == PCD_CMD_TRANSCEIVE) { // read data bytes from card
                uint8_t num_bytes = 0;
                uint8_t valid_bits = 0; // number of valid bits in the last data RX
                num_bytes = read_mfrc_register(MFRC_FIFO_LVL);
                num_bytes = num_bytes & 0x7F; // only lower 7 bits tell us how many bytes are in DR
                valid_bits = read_mfrc_register(MFRC_CTL_REG);
                valid_bits = valid_bits & 0x07; // lower three bits hold the valid bits values

                if (valid_bits) // num of receviced bits 
                    *rcv_len = (num_bytes - 1) * 8 + valid_bits;
                else
                    *rcv_len = num_bytes * 8;

                if (num_bytes < 1)
                    num_bytes = 1;
                if (num_bytes > MFRC_MAX_FIFO_LEN)
                    num_bytes = MFRC_MAX_FIFO_LEN;

                // read data from card to rcv_buf
                for (uint8_t i = 0; i < num_bytes; ++i)
                    rcv_data[i] = read_mfrc_register(MFRC_FIFO_DR);
            }
        }
        else
            status = MFRC_ERR;
    }
    return status;
}

/**
 * @brief Write PCD command to command register
 */
static uint8_t write_pcd_cmd(uint8_t pcd_cmd) {
    write_mfrc_register(MFRC_CMD_REG, pcd_cmd);
    return 0;
}

/**
 * @brief Sets specified bits in the bitmask for the specified register
 */
static uint8_t set_bitmask_on_reg(uint8_t reg, uint8_t reg_msk) {
    uint8_t curr_reg_sttg = 0;
    curr_reg_sttg = read_mfrc_register(reg);
    // mask specified bits
    curr_reg_sttg |= reg_msk;
    write_mfrc_register(reg, curr_reg_sttg);
    return 0;
}

/**
 * @brief Clears the bits specified in the bit mask for the specified register
 */
static uint8_t clear_bitmask_on_reg(uint8_t reg, uint8_t reg_msk) {
    uint8_t curr_reg_sttg = 0;
    curr_reg_sttg = read_mfrc_register(reg);
    // mask specified bits
    curr_reg_sttg &= ~reg_msk;
    write_mfrc_register(reg, curr_reg_sttg);
    return 0;
}

static uint8_t write_mfrc_register(uint8_t reg, uint8_t data) {
    MFRC522_CS_LOW;
    // send register address
    reg = MFRC_ADDR_SET_WRITE(reg);
    uint8_t rslt = TM_SPI_Send(SPI1, reg);
    // send data
    rslt = TM_SPI_Send(SPI1, data);
    MFRC522_CS_HIGH;
    return rslt;
}

static uint8_t read_mfrc_register(uint8_t reg) {
    MFRC522_CS_LOW;
    // send register address
    reg = MFRC_ADDR_SET_READ(reg);
    uint8_t rslt = TM_SPI_Send(SPI1, reg);
    // read bytes from register
    rslt = TM_SPI_Send(SPI1, 0x00);
    MFRC522_CS_HIGH;
    return rslt;
}


