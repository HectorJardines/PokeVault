#include "../../Inc/common/log.h"
#include "../../Inc/common/defines.h"
#include "../../../Drivers/printf/printf.h"
#include <stdio.h>

#define USARTx              (USART2)
#define USART_BAUDRATE      (115200)
#define MAX_BUF_LEN         (64) // 64 bytes max tx len 
#define TX_BUF_LEN(str_len) ((str_len) + 3 + 3) // string len + 4 bytes for num + 3 bytes for colon, carriage return, and newline
#define MAX_DIGITS  (3U)

/***********************
 * STATIC DECLARATIONS
 ***********************/
static void usart_init(void);
static uint8_t usart_transmit(uint8_t *data, uint32_t len);
static uint32_t str_len(uint8_t *str);
static void num_to_bytes(uint8_t *num_buf, uint8_t num);
static void log_create_tx_buf(uint8_t *dst_buf, uint8_t *str, uint32_t num, uint32_t len);


static log_level_e log_level = LOG_LEVEL_ALL;
/*******************
 * USER APIs
 *******************/

/**
 * @brief Initiliaze the serial peripheral for logging
 * 
 * 
 */
void log_init(void) {
    usart_init();
}

/**
 * @brief Sends a log debug message over serial
 * 
 * 
 * 
 * @param debug_str debug message string
 * @param num optional number sent with message
 */
uint8_t log_debug(uint8_t *debug_str, uint32_t num) {
    log_status_e status = LOG_ERR;
    if (log_level == LOG_LEVEL_ALL || log_level == LOG_LEVEL_DEBUG) {
        uint32_t len = str_len(debug_str);
        uint8_t buffer[MAX_BUF_LEN];
        log_create_tx_buf(buffer, debug_str, num, len);
        usart_transmit(buffer, TX_BUF_LEN(len));
    }
    return status;
}


/**
 * @brief Sends a log warning message over serial
 * 
 * 
 * 
 * @param warn_str warning message string
 * @param num optional number
 */
uint8_t log_warn(uint8_t *warn_str, uint32_t num) {
    log_status_e status = LOG_ERR;
    if (log_level == LOG_LEVEL_ALL || log_level == LOG_LEVEL_WARN) {
        uint32_t len = str_len(warn_str);
        uint8_t buffer[MAX_BUF_LEN];
        log_create_tx_buf(buffer, warn_str, num, len);
        usart_transmit(buffer, TX_BUF_LEN(len));
    }
    return status;
}


/** 
 * @brief Sends a log error message over serial
 * 
 * 
 * 
 * @param error_str string to log
 * @param num optional number (perhaps code)
 */
uint8_t log_error(uint8_t *error_str, uint32_t num) {
    log_status_e status = LOG_ERR;
    if (log_level == LOG_LEVEL_ALL || log_level == LOG_LEVEL_WARN) {
        uint32_t len = str_len(error_str);
        uint8_t buffer[MAX_BUF_LEN];
        log_create_tx_buf(buffer, error_str, num, len);
        status = usart_transmit(buffer, TX_BUF_LEN(len));
    }
    return status;
}


/**
 * @brief Sets the current log level
 * 
 * Sets the log module's log level, when a call 
 * to a log function is made if the current log level does
 * not match the message is not made.
 * 
 * @param level the level of log messages perimitted
 */
void log_set_level(log_level_e level) {
    log_level = level;
}


/****************
 * STATIC DEFS
 ****************/

static void usart_init(void) {
    // enable peripheral clock
    RCC->APB1ENR |= (RCC_APB1ENR_USART2EN);

    // enable the peripheral
    LL_USART_Enable(USARTx);
    // set 8 bit word len
    LL_USART_SetDataWidth(USARTx, LL_USART_DATAWIDTH_8B);
    // disable HW flow control
    LL_USART_SetHWFlowCtrl(USARTx, LL_USART_HWCONTROL_NONE);
    // configure 115200 baudrate for USART peripheral
    LL_USART_SetBaudRate(USARTx, APB1_CLK_RATE, LL_USART_OVERSAMPLING_8, USART_BAUDRATE);
    // set oversampling of 8x the clock rate
    LL_USART_SetOverSampling(USARTx, LL_USART_OVERSAMPLING_8);
    // set no parity error checking
    LL_USART_SetParity(USARTx, LL_USART_PARITY_NONE);
    // set single stop bit
    LL_USART_SetStopBitsLength(USARTx, LL_USART_STOPBITS_1);
    // set USART peripheral transmit enable
    LL_USART_SetTransferDirection(USARTx, LL_USART_DIRECTION_TX);
}


static uint8_t usart_transmit(uint8_t *data, uint32_t len) {
    uint8_t status = 0;
    for (uint32_t i = 0; i < len; ++i) {
        while (!LL_USART_IsActiveFlag_TXE(USARTx));
        LL_USART_TransmitData8(USARTx, *(data));
        data++;
    }
    // transmission complete, clear TC flag
    while (!LL_USART_IsActiveFlag_TC(USARTx));
    LL_USART_ClearFlag_TC(USARTx);
    return status;
}

/**
 * OVERWRITE PRINTF UNDERLYING FUNCTIONS, CALLS TO PRINTF SHOULD INCLUDE \r\n
 */
int __io_putchar(int ch) {
    usart_transmit(&ch, 1);
    return ch;
}

int _write(int file, char *ptr, int len) {
    for (int i = 0; i < len; ++i) {
        __io_putchar(*ptr++);
    }
    return len;
}

