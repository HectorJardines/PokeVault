#ifndef _LOG_H
#define _LOG_H

#include <stdint.h>
#include "../drivers/sd_functions.h"
#include "ring_buffer.h"
#include "../../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_usart.h"

#define LOG_WARN_Msk        (0x01)
#define LOG_ERROR_Msk       (0x01 << 1)
#define LOG_EVENT_Msk       (0x01 << 2)
#define LOG_TRANS_Msk       (0x01 << 3)
#define LOG_DISABLE_Msk     (0x00)
#define LOG_ALL_Msk         (0x0F)
#define MAX_MSG_CNT     (10U)
#define MAX_LOG_BODY_LEN     (64U)
#define MAX_FMT_MSG_LEN      (256U)

/*****************
 * ENUMS
 *************/
typedef enum {
    LOG_DISABLE,
    LOG_ERROR,
    LOG_EVENT,
    LOG_TRANS,
    LOG_ALL
} log_level_e;

typedef enum {
    LOG_OK,
    LOG_ERR,
} log_status_e;


typedef struct {
    uint8_t log_levels;
    struct ring_buffer log_buffer;
} log_handle_t;


/*****************
 * PUBLIC APIs
 *****************/

/**
 * @brief Initiliaze the serial peripheral for logging
 * 
 * 
 */
void log_init(void);

/**
 * @brief Writes an event message to event log file
 * 
 * 
 * 
 * @param[in] event_msg event message string
 */
uint8_t log_event(const char *event_msg);


/**
 * @brief Writes a transaction message trans log file
 * 
 * 
 * @param[in] trans_msg transaction message string
 */
uint8_t log_transaction(const char *trans_msg);


/**
 * @brief Writes a warning message to sys log file
 * 
 * 
 * 
 * @param[in] warn_msg warning message string
 */
uint8_t log_warn(const char *warn_msg);


/** 
 * @brief Writes an error message to sys log file
 * 
 * 
 * 
 * @param[in] error_msg error message string
 */
uint8_t log_error(const char *err_msg);


/**
 * @brief Sets the current log level
 * 
 * Sets the log module's log level, when a call 
 * to a log function is made if the current log level does
 * not match the message is not made.
 * 
 * @param level the level of log messages perimitted
 */
void log_set_level(log_level_e level);

#endif