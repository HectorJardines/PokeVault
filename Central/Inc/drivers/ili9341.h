#ifndef _ILI9341_H
#define _ILI9341_H

#include <stdint.h>
#include "../../../Drivers/lvgl-master/lvgl.h"

/**
 * @brief Reset and initialize the ILI9341 disp driver
 * 
 * 
 */
void ili9341_init(void);


/**
 * @brief Sends commands to the ILI9341 disp driver
 * 
 * 
 * 
 */
void ili9341_send_cmd(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, const uint8_t *param, size_t param_size);


/**
 * @brief Flushes a partial display buffer to the display
 * 
 * 
 * 
 */
void ili9341_send_pixels(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, uint8_t * param, size_t param_size);



#endif
