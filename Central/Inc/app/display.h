#ifndef _DISPLAY_H
#define _DISPLAY_H

#include "../../../Drivers/lvgl-master/lvgl.h"
#include "../../Src/ui/ui.h"

/**********************
 * TYPEDEF / MACROS
 **********************/
#define ITEMS_PER_SCREEN    (7U)
#define DISP_UNIT_CHANGE    (0U)
#define DISP_INVENT_CHANGE  (1U)

/***********************
 * PUBLIC APIs
 ********************/

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
 * @brief Signal the unit status has changed for some unit
 * 
 * 
 * 
 */
void display_signal_unit_change(uint8_t type);


/**
 * @brief 
 * 
 * 
 * 
 */
void display_first_load_ready(void);


/**
 * @brief
 * 
 * 
 * 
 */
void display_update_units(void);

void display_update_items(void);

#endif