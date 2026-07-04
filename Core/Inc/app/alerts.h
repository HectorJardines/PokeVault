/**
 * @author Hector Jardines
 * 
 * 
 * This module handles delivery of alerts from unit to central MCU.
 * Different types of alerts are supported and should "inherit"
 * from the generic "alert_t" alert type.
 * 
 */

#ifndef _ALERTS_H
#define _ALERTS_H

#include <stdint.h>

/**************
 * STRUCT/ENUM
 **************/

typedef enum {
    ALERT_SYS_TEMP,
    ALERT_PRESENCE,
    ALERT_SECURITY_BREACH,
    ALERT_SEC_STATUS_CHANGE,
    ALERT_TRANSACTION
} alert_type_e;


/*****************
 * PUBLIC APIs
 *****************/

/**
 * @brief Initialize the CAN/RS-485 peripheral (undecided) and alert buffers
 * 
 * 
 * 
 */
void alert_init(void);


/**
 * @brief Posts an alert message to the central MCU
 * 
 * Alerts are buffered and should be sent as soon as possible.
 * Different types of alerts can be sent via this API as long as they are cast
 * into alert_t type.
 *  
 * @note May include alert priorities since some alerts may be more critical 
 * than others
 * 
 * @param alert 
 */
uint8_t alert_post(alert_t alert);

#endif /* _ALERTS_H */