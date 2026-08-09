#include "../../Inc/common/trace.h"
#include "../../Inc/common/defines.h"
#include "../../../Drivers/printf/printf.h"
#include "../../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_usart.h"
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
/*******************
 * USER APIs
 *******************/

/**
 * @brief Initiliaze the serial peripheral for logging
 * 
 * 
 */
void trace_init(void) {
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
void trace_debug(trace_handle_t *h_trace, const char *dbg_msg) {
    if (h_trace->level == TRACE_ALL || h_trace->level == TRACE_DEBUG) {
        usart_transmit(dbg_msg, 1);
    }
}


// /**
//  * @brief Sends a log warning message over serial
//  * 
//  * 
//  * 
//  * @param warn_str warning message string
//  * @param num optional number
//  */
// void trace_error(trace_handle_t *h_trace, const char *dbg_msg) {
//     if (h_trace->level == LOG_LEVEL_ALL || h_trace->level == LOG_LEVEL_WARN) {
//         uint32_t len = str_len(warn_str);
//         uint8_t buffer[MAX_BUF_LEN];
//         log_create_tx_buf(buffer, warn_str, num, len);
//         usart_transmit(buffer, TX_BUF_LEN(len));
//     }
// }


/** 
 * @brief Sends a log error message over serial
 * 
 * 
 * 
 * @param error_str string to log
 * @param num optional number (perhaps code)
 */
void trace_error(trace_handle_t *h_trace, const char *err_msg) {
    if (h_trace->level == TRACE_ALL || h_trace->level == TRACE_ERR) {
        usart_transmit(err_msg, 1);
    }
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

// int putchar(int ch) {
//     usart_transmit(&ch, 1);
//     return ch;
// }