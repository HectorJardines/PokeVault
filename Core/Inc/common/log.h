#ifndef _LOG_H
#define _LOG_H

/*****************
 * ENUMS
 *************/
typedef enum {
    LOG_LEVEL_DISABLE,
    LOG_LEVEL_WARN,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_ERROR
} log_level_e;

/*****************
 * PUBLIC APIs
 *****************/

uint8_t log_debug(uint8_t *debug_str, uint32_t num);
uint8_t log_warn(uint8_t *warn_str, uint32_t num);
uint8_t log_error(uint8_t *error_str, uint32_t num);

#endif