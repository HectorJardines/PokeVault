#ifndef _DISPLAY_H
#define _DISPLAY_H

#include "lvgl.h"
#include "../drivers/ssd1306.h"

/**
 * @brief Intializes display GUI
 * 
 * 
 * 
 */
void display_init(void);



/**
 * @brief Turn the display ON allowing UI interaction
 * 
 * 
 * 
 */
void display_on(void);



/**
 * @brief Turns the display OFF disabling UI interaction
 * 
 * 
 * 
 * 
 */
void display_off(void);


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
void display_change_screen(struct _lv_obj_t *screen);

#endif