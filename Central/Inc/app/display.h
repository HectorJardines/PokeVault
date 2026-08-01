#ifndef _DISPLAY_H
#define _DISPLAY_H

#include "../../../Drivers/lvgl-master/lvgl.h"
#include "../../Src/ui/ui.h"

/**********************
 * TYPEDEF / MACROS
 **********************/
#define ITEMS_PER_SCREEN (7U)

typedef struct {
    uint8_t node_id;
    uint8_t valid_records;
    CsvRecord records[ITEMS_PER_SCREEN];
} invent_screen_t;


typedef struct {
    uint8_t event_type;
    invent_screen_t screen_contents;
} disp_event_t;


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


#endif