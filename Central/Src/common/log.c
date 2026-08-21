#include "../../Inc/common/log.h"
#include "../../Inc/common/defines.h"
#include "../../Inc/drivers/rtc.h"
#include "../../Inc/drivers/spi.h"
#include "../../Inc/app/inventory.h"

#include "../../Inc/common/printf-stdarg.h"
// #include <stdio.h>
#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"
#include "../../../FreeRTOS_WrkSpace/include/queue.h"
#include "../../../FreeRTOS_WrkSpace/include/semphr.h"

#include <string.h>


/*******************
 * MACRO/TYPEDEFS
 *******************/
#define FPATH_LOGS      ("logs.txt")
#define FPATH_TRANS     ("trans.txt")
#define FPATH_EVENTS    ("events.txt")


#define MAX_LOG_CNT             (10U)
#define FILE_SYNC_PERIOD        (pdMS_TO_TICKS(500))
#define LOG_TASK_STACK_DEPTH    (512U)
#define LOG_TASK_PRIO           (4U)
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
static void create_log_msg(char *fmt_msg, const char *msg_body, log_type_e type);
static void task_logging(void *arg);
static void log_write_to_file(log_t *log, FIL *fp);


// STATIC_RING_BUFFER(log_queue, MAX_MSG_CNT, log_t);
static log_t active_log;

static QueueHandle_t log_q;
static StaticQueue_t _log_q;
static uint8_t log_buf[MAX_LOG_CNT * sizeof(log_t)];

static SemaphoreHandle_t sem_mounted;
static StaticSemaphore_t _sem_mounted;

static TaskHandle_t log_task;
static StaticTask_t _log_task;
static StackType_t log_stk[LOG_TASK_STACK_DEPTH];
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

    sem_mounted = xSemaphoreCreateBinaryStatic(&_sem_mounted);
    log_q = xQueueCreateStatic(MAX_LOG_CNT, sizeof(log_t), log_buf, &_log_q);
    log_task = xTaskCreateStatic(task_logging, "LOG TASK", LOG_TASK_STACK_DEPTH,
                NULL, LOG_TASK_PRIO, log_stk, &_log_task);
    
    if (log_task == NULL) {
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



void log_configure(void) {
    uint8_t res;
    // will only complete once SPI task has finished intialization of SD
    res = sd_mount();
    if (res != FR_OK)
        while (1) {}
    xSemaphoreGive(sem_mounted);
}

uint8_t sd_wait_ready(void) {
    return xSemaphoreTake(sem_mounted, portMAX_DELAY);
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
    static FIL logs, trans, events;
    TickType_t prev_sync = 0, curr_sync_tick = 0;

    log_configure();

    res |= f_open(&logs, FPATH_LOGS, FA_OPEN_APPEND | FA_WRITE);
    res |= f_open(&trans, FPATH_TRANS, FA_OPEN_APPEND | FA_WRITE);
    res |= f_open(&events, FPATH_EVENTS, FA_OPEN_APPEND | FA_WRITE);

    if (res != FR_OK)
        while(1);

    for (;;) {
        f_sync(&logs);
        f_sync(&trans);
        f_sync(&events);
        if (xQueueReceive(log_q, (void *)&curr_log, portMAX_DELAY) == pdTRUE) {
            // write to file
            switch (curr_log.log_type) {
            case (LOG_ERR):
                log_write_to_file(&curr_log, &logs);
                break;
            case (LOG_EVENT):
                log_write_to_file(&curr_log, &events);
                break;
            case (LOG_TRANS):
                log_write_to_file(&curr_log, &trans);
                break;
            }
        }


        curr_sync_tick = xTaskGetTickCount();
        if (curr_sync_tick - prev_sync >= FILE_SYNC_PERIOD) {
            f_sync(&logs);
            f_sync(&trans);
            f_sync(&events);

            prev_sync = curr_sync_tick;
        }

        UBaseType_t high_stk_usage = uxTaskGetStackHighWaterMark(NULL);
    }
}




static void create_log_msg(char *fmt_msg, const char *msg_body, log_type_e type) {
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
    }

    rtc_read_timestamp(&timestamp);
    snprintf(fmt_msg, MAX_FMT_MSG_LEN, "%s:\r\n%02d:%02d:%02d - %02d:%02d:%02d\r\n%s\r\n",
            type_str, timestamp.day, timestamp.month, timestamp.year,
            timestamp.hours, timestamp.minutes, timestamp.seconds,
            msg_body);
}


static void log_write_to_file(log_t *log, FIL *fp) {
    int8_t status = STATUS_OK;
    char formatted_msg[MAX_FMT_MSG_LEN];

    create_log_msg(formatted_msg, log->msg, log->log_type);

    printf("%s\r\n", formatted_msg);
    switch (log->log_type) {
    case LOG_ERR:
        status = sd_write_file(FPATH_LOGS, fp, formatted_msg);
        break;
    case LOG_TRANS:
        status = sd_write_file(FPATH_TRANS, fp, formatted_msg);
        break;
    case LOG_EVENT:
        status = sd_write_file(FPATH_EVENTS, fp, formatted_msg);
        break;
    }

    if (status != STATUS_OK);
        // lol log that...
}

