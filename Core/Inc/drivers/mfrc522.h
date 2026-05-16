/**
 * This module implements the mfrc522 reader driver. Used 
 * for key card reading and authorization. A submodule for the
 * system's security measures.
 */
#ifndef _MFRC522_H
#define _MFRC522_H

#include <stdint.h>

/************************
 *  REGISTER DEFINITIONS
 ************************/

// Proximity Coupling Device (PCD) Commands
#define PCD_CMD_IDLE        (0x00U)
#define PCD_CMD_MEM         (0x01U)
#define PCD_CMD_RAND_ID     (0x02U)
#define PCD_CMD_CALC_CRC    (0x03U)
#define PCD_CMD_TRANSMIT    (0x04U)
#define PCD_CMD_NO_CMD_CHNG (0x07U)
#define PCD_CMD_RECEIVE     (0x08U)
#define PCD_CMD_TRANSCEIVE  (0x0CU)
#define PCD_CMD_MF_AUTH     (0x0EU)
#define PCD_CMD_SOFT_RST    (0x0FU)

// Proximity Inter-Integrated Circuit (PICC) Commands
#define PICC_REQA       (0x26U) // forces tags in IDLE to respond and enter READY state
#define PICC_WUPA       (0x52U) // wakes up tags in IDLE/HALT state
#define PICC_ANTICOLL   (0x93U) 
#define PICC_SEL_CL1    (0x93U)
#define PICC_HALT       (0x50U)
#define PICC_AUTH_A     (0x60U) // auth with Key A
#define PICC_AUTH_B     (0x61U) // auth with Key B
#define PICC_READ       (0x30U)
#define PICC_WRITE      (0xA0U)
#define PICC_DECREMENT  (0xC0U)
#define PICC_INCREMENT  (0xC1U)
#define PICC_RESTORE    (0xC2U)
#define PICC_XFER       (0xB0U)

// MFRC522 Registers

// PAGE 0:
#define MFRC_CMD_REG        (0x01)
#define MFRC_COM_INT_EN     (0x02) // irq control bits enable/disable
#define MFRC_DIV_INT_EN     (0x03)
#define MFRC_COM_IRQ        (0x04)
#define MFRC_DIV_IRQ        (0x05)
#define MFRC_ERR_REG        (0x06)
#define MFRC_STAT1_REG      (0x07)
#define MFRC_STAT2_REG      (0x08)
#define MFRC_FIFO_DR        (0x09)
#define MFRC_FIFO_LVL       (0x0A)
#define MFRC_WATER_LVL      (0x0B)
#define MFRC_CTL_REG        (0x0C)
#define MFRC_BIT_FRAMING    (0x0D)
#define MFRC_COLL_REG       (0x0E)

// PAGE 1:
#define MFRC_MODER          (0x11U)
#define MFRC_TX_MODER       (0x12U)
#define MFRC_RX_MODER       (0x13U)
#define MFRC_TX_CTLR        (0x14U)
#define MFRC_TX_ASKR        (0x15U)
#define MFRC_TX_SELR        (0x16U)
#define MFRC_RX_SELR        (0x17U)
#define MFRC_RX_THRESHR     (0x18U)
#define MFRC_DEMODR         (0x19U)
#define MFRC_MfTXR          (0x1CU) // MiFare communication TX parameters
#define MFRC_MfRXR          (0x1DU) // MiFare communication RX parmameters
#define MFRC_MfSSR          (0x1FU)

// PAGE 2: CONFIGURATION REGISTERS
#define MFRC_CRC_RESR_MSB   (0x21U) // shows MSB and LSB of CRC calc
#define MFRC_CRC_RESR_LSB   (0x22U)
#define MFRC_MODWIDTHR      (0x24U)
#define MFRC_RFCfgR         (0x26U) // configure receiver gain
#define MFRC_GsNR           (0x27U) // conductance of antenna driver pins
#define MFRC_CWGsPR         (0x28U) // select conductance of p-driver output during no modulation
#define MFRC_MOD_GsPR       (0x29U) // selects conductance of p-driver out during modulation
#define MFRC_TMODER         (0x2AU) // internal timer settings
#define MFRC_TPSCR          (0x2BU)
#define MFRC_TRELOADR_H     (0x2CU)
#define MFRC_TRELOADR_L     (0x2DU)
#define MFRC_TCNTR_H        (0x2E)
#define MFRC_TCNTR_L        (0x2FU)

// PAGE 3: TEST REGISTERS
#define MFRC_TEST_SEL1R     (0x31U) // general test signal configuration
#define MFRC_TEST_SEL2R     (0x32U)
#define MFRC_TEST_PINENR    (0x33U)
#define MFRC_TEST_PINVALR   (0x34U)
#define MFRC_TEST_BUSR      (0x35U)
#define MFRC_TEST_AUTOR     (0x36) // digital self test register
#define MFRC_VERSIONR       (0x37U)
#define MFRC_TEST_ANALOGR   (0x38U) // controls AUX1/2 pins
#define MFRC_TEST_DAC1R     (0x39U)
#define MFRC_TEST_DAC2R     (0x3AU)
#define MFRC_TEST_ADCR      (0x3BU)

/***********************
 *   ENUMS/STRUCTS
 ***********************/
typedef enum {
    MFRC_OK,
    MFRC_ERR,
    MFRC_TIMEOUT
} mfrc_status_e;

typedef struct {
    void (*spi_init)(void);
    uint8_t(*spi_tx)(uint8_t *data, uint32_t len);
    uint8_t(*spi_rx)(uint8_t *data, uint32_t len);
} mfrc_reader_t;

/**********************
 *      USER APIs
 **********************/

/**
 * @brief Intializes and configures the MFRC522 device
 */
void mfrc522_init(void);

/**
 * @brief Performs keycard RFID authentication
 * 
 * @return 0 if authentication fails; 1 else
 */
uint8_t mfrc522_auth(uint8_t auth_type, uint8_t picc_block_addr, uint8_t *sector_key, uint8_t *serial_num);

/**
 * @brief Compares the keycard id with stored ID
 * 
 * @param card_id RFID associated with the keycard
 * @param registered_id predefined RFID
 * 
 * @return 0 if IDs differ; 1 else
 */
uint8_t mfrc_compare(uint8_t *id, uint8_t *comp_id);

#endif