#include "../Inc/drivers/i2c.h"

#define I2C_STD_MODE_Hz (100000U)

/**********************
 * STATIC DECLARATIONS
 **********************/
static void i2c_configure(void);
static I2C_HandleTypeDef h_i2c1;

/*******************
 * USER APIs
 *******************/

uint8_t initialized = 0;
void i2c_init(i2c_device_e dev) {
    // enable peripheral clock
    RCC->APB1ENR |= (RCC_APB1ENR_I2C1EN);
    i2c_configure();
    initialized = 1;
}

uint8_t i2c_transmit(uint8_t dev_addr, uint8_t *send_data, uint32_t data_len) {
    return HAL_I2C_Master_Transmit(&h_i2c1, dev_addr, send_data, data_len, 500);
}

uint8_t i2c_receive(uint8_t dev_addr, uint8_t *rcv_data, uint32_t data_len) {
    return HAL_I2C_Master_Receive(&h_i2c1, dev_addr, rcv_data, data_len, 500);
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
    // initializes CRR/TRISE/MODE/etc.
    HAL_I2C_Init(&h_i2c1);
}