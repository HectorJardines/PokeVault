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


#endif