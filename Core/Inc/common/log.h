#ifndef _LOG_H
#define _LOG_H

#include <stdint.h>
#include "../../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_usart.h"

/*****************
 * ENUMS
 *************/
typedef enum {
    LOG_LEVEL_ALL,
    LOG_LEVEL_DISABLE,
    LOG_LEVEL_WARN,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_ERROR
} log_level_e;

typedef enum {
    LOG_OK,
    LOG_ERR,
} log_status_e;

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
 * @brief Sends a log debug message over serial
 * 
 * 
 * 
 * @param debug_str debug message string
 * @param num optional number sent with message
 */
uint8_t log_debug(uint8_t *debug_str, uint32_t num);


/**
 * @brief Sends a log warning message over serial
 * 
 * 
 * 
 * @param warn_str warning message string
 * @param num optional number
 */
uint8_t log_warn(uint8_t *warn_str, uint32_t num);


/** 
 * @brief Sends a log error message over serial
 * 
 * 
 * 
 * @param error_str string to log
 * @param num optional number (perhaps code)
 */
uint8_t log_error(uint8_t *error_str, uint32_t num);


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