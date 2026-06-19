#include "../../Inc/app/display.h"
#include "../../Inc/common/log.h"
#include "../../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal.h"

#define DISPLAY_WIDTH   (128U)
#define DISPLAY_HEIGHT  (64U)

#define HTILE_BUF_SIZE (((DISPLAY_HEIGHT * DISPLAY_WIDTH)  >> 3) + 8)
#define VTILE_BUF_SIZE (DISPLAY_WIDTH * (DISPLAY_HEIGHT >> 3))
/**************************
 * STATIC DECLARTATIONS
 ***********************/
static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *pixel_map);
static void lvgl_flush_wait_cb(lv_display_t *display);
static void lv_round_area_dimensions_cb(lv_event_t *event);
void lv_display_flushed_cb(DMA_HandleTypeDef *h_dma);

static uint8_t htiled_buf[HTILE_BUF_SIZE] = {0};
static uint8_t vtiled_buf[VTILE_BUF_SIZE] = {0};
static lv_display_t *display = NULL;
/*****************
 * PUBLIC APIs
 *****************/


static uint8_t initialized = 0;
void display_init(void) {
    lv_init();
    // ssd1306_install_flush_cb(lv_display_flushed_cb);
    ssd1306_init();
    display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_tick_set_cb(HAL_GetTick);

    // disable antialiasing
    lv_display_set_antialiasing(display, DISABLE);
    // monochrome color format
    lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);

    // horizontally tiled display buffer configuration + flush callbacks
    lv_display_set_buffers(display, htiled_buf, NULL, HTILE_BUF_SIZE, LV_DISP_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    lv_display_add_event_cb(display, lv_round_area_dimensions_cb, LV_EVENT_INVALIDATE_AREA, display);
}


/******************
 * STATIC DEFS
 ******************/

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *pixel_map) {
    int32_t width = lv_area_get_width(area);
    int32_t height = lv_area_get_height(area);
    int32_t area_size = (width * height) >> 3;
    pixel_map += 8; // skip pallete bytes

    lv_draw_sw_i1_convert_to_vtiled(pixel_map, 
                                    area_size, 
                                    width, 
                                    height, 
                                    vtiled_buf, 
                                    VTILE_BUF_SIZE, //  1024 bytes 
                                    true);

    ssd1306_set_pixels(vtiled_buf, area->x1, area->y1, area->x2, area->y2);
    ssd1306_display();
}

static void lv_round_area_dimensions_cb(lv_event_t *event) {
    lv_area_t *area = lv_event_get_param(event); // retrieve area object from event

    // round start height/width down to nearest multiple of 8
    area->y1 = (area->y1 & ~0x7);
    // rounds end height and width up to nearest multiple of 8
    area->y2 = (area->y2 | 0x7);
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *handle) {
    if (handle->Instance == I2C1) {
        lv_display_flush_ready(display);
    }
}

