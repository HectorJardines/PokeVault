#include "../Inc/drivers/i2c.h"

#define I2C_STD_MODE_Hz (100000U)

/**********************
 * STATIC DECLARATIONS
 **********************/
static void i2c_configure(void);
static void i2c_dma_configure(void);

static I2C_HandleTypeDef h_i2c1;
static DMA_HandleTypeDef h_dma1;
/*******************
 * USER APIs
 *******************/

static uint8_t initialized = 0;
void i2c_init(i2c_device_e dev) {
    // enable peripheral clock
    if (initialized != 1) {
        RCC->APB1ENR |= (RCC_APB1ENR_I2C1EN);
        i2c_configure();
        i2c_dma_configure();
        initialized = 1;
    }
}

uint8_t i2c_transmit(uint8_t dev_addr, uint8_t *send_data, uint32_t data_len) {
    dev_addr = (dev_addr << 1) | 0x01;
    return HAL_I2C_Master_Transmit(&h_i2c1, dev_addr, send_data, data_len, 500);
}

uint8_t i2c_receive(uint8_t dev_addr, uint8_t *rcv_data, uint32_t data_len) {
    dev_addr = (dev_addr << 1);
    return HAL_I2C_Master_Receive(&h_i2c1, dev_addr, rcv_data, data_len, 500);
}

uint8_t i2c_transmit_dma(uint8_t dev_addr, uint8_t *data, uint16_t data_len) {
    dev_addr = (dev_addr << 1) | 0x01;
    uint8_t res = HAL_I2C_Master_Transmit_DMA(&h_i2c1, dev_addr, data, data_len);
    HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
    return res;
}

/**
 * @brief registers the i2c tx complete cb used with DMA tx
 * 
 * This API will be used to register the cb function that will call
 * the LVGL flush ready API to notify LVGL that the transmission of 
 * display buffer has finished
 * 
 * @param tx_cmplt_cb pointer to callback function
 */
void i2c_set_dma_tx_cplt_cb(void(*tx_cmplt_cb)(DMA_HandleTypeDef * hdma)) {
    h_i2c1.MasterTxCpltCallback = tx_cmplt_cb;
}

/*******************
 * STATIC DEFS
 *******************/

 /**
 * @brief Configure and initialize I2C peripheral
 */
static void i2c_configure(void) {
    // set I2C1 peripheral
    h_i2c1.Instance = I2C1;
    // set 7 bit addressing mode
    h_i2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    // set clockspeed in Hz (standard mode)
    h_i2c1.Init.ClockSpeed = I2C_STD_MODE_Hz;
    h_i2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    
    // DISABLE GEN CALL, NOSTRETCH, ETC...
    h_i2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    h_i2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    h_i2c1.Init.OwnAddress1 = 0;
    h_i2c1.Init.OwnAddress2 = 0;

    // initializes CRR/TRISE/MODE/etc.
    HAL_I2C_Init(&h_i2c1);
}

/**
 * @brief Configures DMA1 peripheral 
 * 
 * This function configures the DMA1 peripheral's
 * Stream0 channel 0 in Normal mode, with mem-to-periph
 * data flow. Single byte memory alignment and auto 
 * memory incremement.
 * 
 */
static void i2c_dma_configure(void) {
    RCC->AHB1ENR |= (RCC_AHB1ENR_DMA1EN);
    // configure DMA periph in MEM-TO-PERIPH, NORMAL Mode with single byte MINC
    h_dma1.Init.Channel = DMA_CHANNEL_0;
    h_dma1.Init.Direction = DMA_MEMORY_TO_PERIPH;
    h_dma1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    h_dma1.Init.MemBurst = DMA_MBURST_SINGLE;
    h_dma1.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    h_dma1.Init.MemInc = DMA_MINC_ENABLE;
    h_dma1.Init.Mode = DMA_NORMAL;

    // NO PERIPHERAL INCREMENT
    h_dma1.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    h_dma1.Init.PeriphInc = DMA_PINC_DISABLE;

    h_dma1.Instance = DMA1_Stream1;
    HAL_DMA_Init(&h_dma1);

    // assign stream to i2c1 instance and vice versa
    __HAL_LINKDMA(&h_i2c1, hdmatx, h_dma1);
}

void I2C1_EV_IRQHandler(void) {
    HAL_I2C_EV_IRQHandler(&h_i2c1);
}

void DMA1_Stream1_IRQHandler(void) {
    HAL_DMA_IRQHandler(&h_dma1);
}