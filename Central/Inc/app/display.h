#ifndef _DISPLAY_H
#define _DISPLAY_H

#include "../../../Drivers/lvgl-master/lvgl.h"
#include "../../Src/ui/ui.h"

/**********************
 * TYPEDEF / MACROS
 **********************/
#define ITEMS_PER_SCREEN (7U)


/***********************
 * PUBLIC APIs
 ********************/

void display_configure(void);


/**
 * @brief Intialize LVGL display library and ILI9341 driver
 * 
 * 
 * 
 */
void display_init(void);



/**
 * @brief
 * 
 * 
 */
void display_load_scanning_screen(void);



/**
 * @brief
 * 
 * 
 */
void display_load_scanned_screen(void);


/**
 * @brief 
 * 
 * 
 * 
 */
void display_first_load_ready(void);

#endif