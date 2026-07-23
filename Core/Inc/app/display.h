#ifndef _DISPLAY_H
#define _DISPLAY_H

#include "../../../Drivers/lvgl-master/lvgl.h"
#include "../drivers/ssd1306.h"

typedef enum {
    LABEL_HUM,
    LABEL_TEMP,
    LABEL_STATUS
} disp_label_e;

/**
 * @brief Intializes display GUI
 * 
 * 
 * 
 */
void display_init(void);

/**
 * @brief Checks whether the display is ON or OFF
 * 
 * This function should be called before making any calls
 * to lv_timer_handler(). If the display is not ON, do NOT
 * make a call to lv_timer_handler
 * 
 * @return 1 if display is ON; else 0
 */
uint8_t display_is_on(void);



/**
 * @brief Returns whether display scan visual is complete
 * 
 * This function should be called before scanning for any 
 * tags. Helps to mitigate multiple scans since MCU is much 
 * faster than user in terms of "scanning" items.
 * 
 * @return 1 if visual is complete; 0 else
 */
uint8_t display_scan_cplt(void);


/**
 * @brief Refresh temp/humidity value on screen
 * 
 * 
 * @param[in] val
 * @param[in] hum_or_temp
 * 
 * @return 0 on success; else 1
 */
uint8_t display_refresh_value(disp_label_e hum_or_temp, uint16_t val);


/**
 * @brief Turn the display ON allowing UI interaction
 * 
 * Wakes the display from sleep mode. 
 * 
 */
void display_wake(void);



/**
 * @brief Turns the display OFF disabling UI interaction
 * 
 * Changes the display state frome ON to sleep
 * 
 */
void display_sleep(void);


/**
 * @brief Changes the active screen
 * 
 * This function updates the displays active screen.
 * When NULL is passed to the function, it assumes that 
 * the tag scanning cycle is being initiated, i.e. load
 * tag scanning screen for 800ms, then load tag scanned for 
 * 800 ms, then back to main screen.
 * 
 * @param[in] screen the screen to be set as active; or NULL to begin
 * tag scanning screen cycle
 */
void display_change_screen(struct _lv_obj_t *screen, uint16_t screen_id);

#endif