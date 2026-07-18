#ifndef _DISPLAY_H
#define _DISPLAY_H

#include "../../../Drivers/lvgl-master/lvgl.h"
#include "../drivers/ssd1306.h"

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