#include "../../Inc/common/log.h"
#include "../../Inc/common/defines.h"
#include "../../Inc/drivers/rtc.h"
#include "../../Inc/drivers/spi.h"

#include "../../Inc/common/printf-stdarg.h"
#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"
#include "../../../FreeRTOS_WrkSpace/include/queue.h"

#include <string.h>


/*******************
 * MACRO/TYPEDEFS
 *******************/
#define FPATH_LOGS      ("logs.txt")
#define FPATH_TRANS     ("trans.txt")
#define FPATH_EVENTS    ("events.txt")


#define MAX_LOG_CNT             (10U)
#define FILE_SYNC_PERIOD        (pdMS_TO_TICKS(500))
#define LOG_TASK_STACK_DEPTH    (1024U)
#define LOG_TASK_PRIO           (5U)
#define LOG_ENQ_TIMEOUT         (pdMS_TO_TICKS(50))
#define LOG_DEQ_TIMEOUT         (pdMS_TO_TICKS(10))

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
static void task_logging(void *arg);
static void log_write_to_file(log_t *log);


// STATIC_RING_BUFFER(log_queue, MAX_MSG_CNT, log_t);
static log_t active_log;

static QueueHandle_t log_q;
static StaticQueue_t _log_q;
static uint8_t log_buf[MAX_LOG_CNT * sizeof(log_t)];
/*******************
 * USER APIs
 *******************/

/**
 * @brief Initiliaze the serial peripheral for logging
 * 
 * 
 */
void log_init(void) {
    uint8_t status = STATUS_OK;
    spi_init();

    log_q = xQueueCreateStatic(MAX_LOG_CNT, sizeof(log_t), log_buf, &_log_q);
    BaseType_t stat = xTaskCreate(task_logging, "LOG TASK", LOG_TASK_STACK_DEPTH,
                NULL, LOG_TASK_PRIO, NULL);
    
    if (stat != pdTRUE) {
        while(1) {}
    }
    (void)status;
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
    printf(event_msg);
    // pops off any log that is taking too long to TX so we don't block
    return !xQueueSendToBack(log_q, (void *)&active_log, LOG_ENQ_TIMEOUT);
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
    printf(trans_msg);
    // pops off any log that is taking too long to TX so we don't block
    return !xQueueSendToBack(log_q, (void *)&active_log, LOG_ENQ_TIMEOUT);
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
    printf(warn_msg);
    active_log.log_type = LOG_ERROR;
    
    // pops off any log that is taking too long to TX so we don't block
    return !xQueueSendToBack(log_q, (void *)&active_log, LOG_ENQ_TIMEOUT);
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
    printf(err_msg);
    // pops off any log that is taking too long to TX so we don't block
    return !xQueueSendToBack(log_q, (void *)&active_log, LOG_ENQ_TIMEOUT);
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
/**
 * @brief Processes queue log items
 */
static void task_logging(void *arg) {
    log_t curr_log = {0, 0, {0}};
    FRESULT res = FR_OK;
    FIL logs, trans, events;
    TickType_t prev_sync = 0, curr_sync_tick = 0;

    res |= f_open(&logs, FPATH_LOGS, FA_OPEN_APPEND | FA_WRITE);
    res |= f_open(&trans, FPATH_TRANS, FA_OPEN_APPEND | FA_WRITE);
    res |= f_open(&events, FPATH_EVENTS, FA_OPEN_APPEND | FA_WRITE);

    for (;;) {
        if (xQueueReceive(log_q, (void *)&curr_log, LOG_DEQ_TIMEOUT) == pdTRUE) {
            // write to file
            log_write_to_file(&curr_log);
        }

        curr_sync_tick = xTaskGetTickCount();
        if (curr_sync_tick - prev_sync >= FILE_SYNC_PERIOD) {
            f_sync(&logs);
            f_sync(&trans);
            f_sync(&events);

            prev_sync = curr_sync_tick;
        }
    }
}




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
    snprintf(fmt_msg, MAX_FMT_MSG_LEN, "%s:\r\n%02d:%02d:%02d - %02d:%02d:%02d\r\n%s\r\n",
            type_str, timestamp.day, timestamp.month, timestamp.year,
            timestamp.hours, timestamp.minutes, timestamp.seconds,
            msg_body);
}


static void log_write_to_file(log_t *log) {
    int8_t status = STATUS_OK;
    char formatted_msg[MAX_FMT_MSG_LEN];

    create_log_msg(formatted_msg, log->msg, log->log_type);

    switch (log->log_type) {
    case LOG_ERR:
        status = sd_write_file(FPATH_LOGS, formatted_msg);
        break;
    case LOG_TRANS:
        status = sd_write_file(FPATH_TRANS, formatted_msg);
        break;
    case LOG_EVENT:
        status = sd_write_file(FPATH_EVENTS, formatted_msg);
        break;
    }

    (void)status;
}

