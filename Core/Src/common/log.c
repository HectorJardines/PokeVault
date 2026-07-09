#include "../../Inc/common/log.h"
#include "../../Inc/common/defines.h"
#include "../../../Drivers/printf/printf.h"
#include "../Inc/drivers/rtc.h"
#include "spi.h"
#include <stdio.h>
#include <string.h>


#define MAX_MSG_CNT     (10U)
#define MAX_LOG_BODY_LEN     (64U)
#define MAX_FMT_MSG_LEN      (256U)

typedef struct {
    uint8_t log_type;
    uint8_t len;
    uint8_t msg[MAX_LOG_BODY_LEN];
} log_t;

/***********************
 * STATIC DECLARATIONS
 ***********************/
/**
 * @brief Formats message with timestamp and type
 * 
 * 
 * @param[out] fmt_msg
 * @param[in] msg_body
 * @param[in] type
 */
static void create_log_msg(char *fmt_msg, const char *msg_body, log_level_e type);

STATIC_RING_BUFFER(log_queue, MAX_MSG_CNT, log_t);
static log_t active_log;
/*******************
 * USER APIs
 *******************/

/**
 * @brief Initiliaze the serial peripheral for logging
 * 
 * 
 */
void log_init(void) {
    uint8_t status = 0x00;
    spi_init(SPI_DEVICE_BMI160);
    status = sd_mount();
    return status;
}

/**
 * @brief Writes an event message to event log file
 * 
 * 
 * 
 * @param[in] event_msg event message string
 */
uint8_t log_event(const char *event_msg) {
    memset((void *)&active_log, 0, sizeof(active_log));
    active_log.len = strlen(event_msg);
    memcpy((void *)active_log.msg, event_msg, active_log.len);
    active_log.log_type = LOG_EVENT;
    
    // pops off any log that is taking too long to TX so we don't block
    ring_buffer_push(&log_queue, (void *)&active_log);

    return STATUS_OK;
}


/**
 * @brief Writes a transaction message trans log file
 * 
 * 
 * @param[in] trans_msg transaction message string
 */
uint8_t log_transaction(const char *trans_msg) {
    memset((void *)&active_log, 0, sizeof(active_log));
    active_log.len = strlen(trans_msg);
    memcpy((void *)active_log.msg, trans_msg, active_log.len);
    active_log.log_type = LOG_TRANS;
    
    // pops off any log that is taking too long to TX so we don't block
    ring_buffer_push(&log_queue, (void *)&active_log);

    return STATUS_OK;
}


/**
 * @brief Writes a warning message to sys log file
 * 
 * 
 * 
 * @param[in] warn_msg warning message string
 */
uint8_t log_warn(const char *warn_msg) {
    memset((void *)&active_log, 0, sizeof(active_log));
    active_log.len = strlen(warn_msg);
    memcpy((void *)active_log.msg, warn_msg, active_log.len);
    active_log.log_type = LOG_ERROR;
    
    // pops off any log that is taking too long to TX so we don't block
    ring_buffer_push(&log_queue, (void *)&active_log);

    return STATUS_OK;
}


/** 
 * @brief Writes an error message to sys log file
 * 
 * 
 * 
 * @param[in] error_msg error message string
 */
uint8_t log_error(const char *err_msg) {
    memset((void *)&active_log, 0, sizeof(active_log));
    active_log.len = strlen(err_msg);
    memcpy((void *)active_log.msg, err_msg, active_log.len);
    active_log.log_type = LOG_ERR;
    
    // pops off any log that is taking too long to TX so we don't block
    ring_buffer_push(&log_queue, (void *)&active_log);

    return STATUS_OK;
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

}


/***********************
 * STATIC DECLARATIONS
 ***********************/
static void create_log_msg(char *fmt_msg, const char *msg_body, log_level_e type) {
    rtc_info_t timestamp;
    const char *type_str;
    switch (type) {
    case LOG_ERR:
        type_str = "ERROR";
        break;
    case LOG_TRANS:
        type_str = "TRANSACTION";
        break;
    case LOG_EVENT:
        type_str = "EVENT";
        break;
    case LOG_ALL:
    case LOG_DISABLE:
        break;
    }

    rtc_read_timestamp(&timestamp);
    snprintf(fmt_msg, MAX_LOG_BODY_LEN, "%s:\r\n%02d:%02d:%02d - %02d:%02d:%02d\r\n%s\r\n",
            type_str, timestamp.day, timestamp.month, timestamp.year,
            timestamp.hours, timestamp.minutes, timestamp.seconds,
            msg_body);
}


static void log_write_to_file(void) {
    int8_t status = STATUS_OK;
    char formatted_msg[MAX_LOG_BODY_LEN];
    log_t log;
    if (!ring_buffer_empty(&log_queue)) {
        ring_buffer_pop(&log_queue, (void *)&log);
        create_log_msg(formatted_msg, log.msg, log.log_type);

        switch (log.log_type) {
        case LOG_ERR:
            status = sd_append_file("logs.txt", formatted_msg);
            break;
        case LOG_TRANS:
            status = sd_append_file("trans.txt", formatted_msg);
            break;
        case LOG_EVENT:
            status = sd_append_file("events.txt", formatted_msg);
            break;
        }
    }
}

