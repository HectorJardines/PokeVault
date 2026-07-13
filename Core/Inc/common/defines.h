#ifndef _DEFINES_H
#define _DEFINES_H

#define SYSCLK_Hz       (75000000U)
#define APB2_CLK_RATE    (SYSCLK_Hz)
#define APB1_CLK_RATE   (37500000U)

#define DIV_2       (1U)
#define DIV_4       (2U)
#define DIV_8       (3U)

#define FALSE   (0U)
#define TRUE    (!FALSE)
#define SET     (TRUE)
#define CLR     (FALSE)

#define INITIALIZED (1U)
#define UNITIALIZED (0U)

#define STATUS_OK   (0U)
#define STATUS_ERR  (1U)

#define NODE_ID (0x00U)

typedef enum {
    ALERT_SYS_TEMP,
    ALERT_SYS_HUM,
    ALERT_PRESENCE,
    ALERT_SECURITY_BREACH,
    ALERT_SEC_STATUS_CHANGE
} alert_type_e;

typedef enum {
    MSG_EVENT_DISARMED,
    MSG_EVENT_UNIT_OPEN,
    MSG_EVENT_UNIT_CLOSE,
    MSG_EVENT_UNIT_MOVE,
    MSG_EVENT_PRESENCE,
    MSG_EVENT_NO_PRESENCE
} event_type_e;

#endif