#include "rtc.h"
#include "common/defines.h"
#include "../../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_pwr.h"
#include "../../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_rcc.h"
#include "../../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_exti.h"

#define SYNCH_PSCL  (255U)
#define ASYCNH_PSCL (124U)
#define RTCx        (RTC)

/***********
 * STATIC DECLARTIONS
 **********************/
static void rtc_boot_process(void);
static void rtc_get_date(rtc_info_t *datetime);
static void rtc_get_time(rtc_info_t *datetime);
static void rtc_periph_configure(void);

static rtc_info_t active_datatime = {.seconds = 0x04, .minutes = 0x02, .hours = 0x08, .weekday = LL_RTC_WEEKDAY_TUESDAY,
                                    .day = 0x07, .month = LL_RTC_MONTH_JULY, .year = 26};
/**************
 * PUB APIs
 **************/

static uint8_t initialized = UNITIALIZED;
/**
 * @brief RTC intialization function configures the low-level peripheral
 * 
 * 
 */
uint8_t rtc_init(void) {
    rtc_boot_process();
    rtc_write_datetime(&active_datatime);
    initialized = INITIALIZED;
}



/**
 * @brief retrieve a timestamp from the RTC peripheral
 * 
 * 
 * @param[out] timestamp
 * 
 * @return 0 on success; else 1
 */
uint8_t rtc_read_timestamp(rtc_info_t *timestamp) {
    uint8_t status = STATUS_OK;
    if (timestamp != NULL) {
        rtc_get_time(timestamp);
        rtc_get_date(timestamp);
    }
    else
        status = STATUS_ERR;
    
    return status;
}



/**
 * @brief Sets the RTC seconds, hours, day, etc.
 * 
 * 
 * @param[in] datetime struct with RTC time info
 */
uint8_t rtc_write_datetime(rtc_info_t *datetime) {
    uint8_t status = STATUS_OK;
    uint8_t try = 0xFF;

    LL_RTC_DisableWriteProtection(RTCx);

    // set init bit to enter intializaiton mode
    LL_RTC_EnableInitMode(RTCx);
    while (!LL_RTC_IsActiveFlag_INIT(RTCx) && try--);
    if (try > 0) {
        // load initial time and date values
        LL_RTC_TIME_Config(RTCx, LL_RTC_TIME_FORMAT_AM_OR_24, datetime->hours, datetime->minutes, datetime->seconds);
        LL_RTC_DATE_Config(RTCx, datetime->weekday, datetime->day, datetime->month, datetime->year);

        // exit initialization mode and restart counter
        LL_RTC_DisableInitMode(RTCx);
    }
    else
        status = STATUS_ERR;

    LL_RTC_EnableWriteProtection(RTCx);
    LL_RTC_ClearFlag_RS(RTCx);
    while (!LL_RTC_IsActiveFlag_RS(RTCx));

    return status;
}

/**************
 * STATIC DEFS
 ***************/
static void rtc_boot_process(void) {
    volatile uint8_t dummy_read = 0x00;

    LL_PWR_EnableBkUpAccess();
    dummy_read = PWR->CR; // see PWR_CR section in RM
    
    LL_RCC_LSI_Enable();
    while (!LL_RCC_LSI_IsReady());
    LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSI);
    LL_RCC_EnableRTC();
    dummy_read = RTCx->ISR;

    LL_RTC_DisableWriteProtection(RTCx);
    LL_RTC_EnableInitMode(RTCx);
    while (!LL_RTC_IsActiveFlag_INIT(RTCx));

    LL_RTC_SetHourFormat(RTCx, LL_RTC_HOURFORMAT_24HOUR);
    // Configure 1Hz RTC clock (tick every second)
    LL_RTC_SetSynchPrescaler(RTCx, SYNCH_PSCL);
    LL_RTC_SetAsynchPrescaler(RTCx, ASYCNH_PSCL);

    // set init bit to enter intializaiton mode
    LL_RTC_DisableInitMode(RTCx);
    LL_RTC_EnableWriteProtection(RTCx);
    while (LL_RTC_IsActiveFlag_INIT(RTCx));

    LL_RTC_ClearFlag_RS(RTCx);
    while (!LL_RTC_IsActiveFlag_RS(RTCx));

    (void) dummy_read;
}


static void rtc_get_date(rtc_info_t *datetime) {
    uint32_t temp = LL_RTC_TIME_GetSecond(RTCx);
    datetime->seconds = __LL_RTC_CONVERT_BCD2BIN(temp);
    temp = LL_RTC_TIME_GetMinute(RTCx);
    datetime->minutes = __LL_RTC_CONVERT_BCD2BIN(temp);
    temp = LL_RTC_TIME_GetHour(RTCx);
    datetime->hours = __LL_RTC_CONVERT_BCD2BIN(temp);
}


static void rtc_get_time(rtc_info_t *datetime) {
    uint32_t temp = LL_RTC_DATE_GetWeekDay(RTCx);
    datetime->weekday = __LL_RTC_CONVERT_BCD2BIN(temp);
    temp = LL_RTC_DATE_GetDay(RTCx);
    datetime->day = __LL_RTC_CONVERT_BCD2BIN(temp);
    temp = LL_RTC_DATE_GetMonth(RTCx);
    datetime->month = __LL_RTC_CONVERT_BCD2BIN(temp);
    temp = LL_RTC_DATE_GetYear(RTCx);
    datetime->year = __LL_RTC_CONVERT_BCD2BIN(temp);
}

