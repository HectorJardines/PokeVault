/**
 * @author Hector Jardines
 * 
 * This module implements an RTC driver for 
 * generation of timestamps used for data/event
 * logging.
 * 
 */

 #ifndef _RTC_H
 #define _RTC_H

#include <stdint.h>
#include "../../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_rtc.h"
#include "stm32f4xx.h"

/**
 * ENUMS/STRUCTS
 */
typedef struct {
    uint32_t seconds;
    uint32_t minutes;
    uint32_t hours;

    uint32_t weekday;
    uint32_t day;
    uint32_t month;
    uint32_t year;
} rtc_info_t;


/**
 * PUB APIs
 */


/**
 * @brief RTC intialization function configures the low-level peripheral
 * 
 * 
 */
uint8_t rtc_init(void);



/**
 * @brief retrieve a timestamp from the RTC peripheral
 * 
 * 
 * @param[out] timestamp
 * 
 * @return 0 on success; else 1
 */
uint8_t rtc_read_timestamp(rtc_info_t *timestamp);



/**
 * @brief Sets the RTC seconds, hours, day, etc.
 * 
 * 
 * @param[in] datetime struct with RTC time info
 */
uint8_t rtc_write_datetime(rtc_info_t *datetime);


 #endif /* _RTC_H */