/**
 * @author Hector Jardines
 * 
 * Trace module for printing debug information 
 * to serial console.
 * 
 */


#ifndef _TRACE_H
#define _TRACE_H

#include <stdint.h>

/**********************
 * STRUCTS/ENUMS
 *********************/
typedef enum {
    TRACE_DEBUG,
    TRACE_WARN,
    TRACE_ERR,
    TRACE_ALL
} trace_level_e;

typedef struct {
    trace_level_e level;
} trace_handle_t;

/****************
 * PUBLIC APIs
 ***************/

/**
 * @brief Intialize the trace module
 * 
 * Initializes and configures the underlying USART
 * peripheral for serial communication with console.
 * 
 */
void trace_init(void);



/**
 * @brief Writes a debug message to the serial terminal
 * 
 * 
 * @param[in] dbg_msg
 */
void trace_debug(trace_handle_t *h_trace, const char *dbg_msg);



/** 
 * @brief Writes an error message to the serial terminal
 * 
 * 
 * @param[in] err_msg
*/
void trace_error(trace_handle_t *h_trace, const char *err_msg);

#endif /* _TRACE_H */