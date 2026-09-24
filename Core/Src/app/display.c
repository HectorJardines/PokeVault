#include "../../Inc/app/display.h"
#include "../ui/ui.h"
#include "../../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal.h"
#include "../../Inc/common/defines.h"

#define DISPLAY_WIDTH   (128U)
#define DISPLAY_HEIGHT  (64U)
#define DISPLAY_REFR_DELAY  (800U) //ms

#define HTILE_BUF_SIZE (((DISPLAY_HEIGHT * DISPLAY_WIDTH)  >> 3) + 8)
#define VTILE_BUF_SIZE (DISPLAY_WIDTH * (DISPLAY_HEIGHT >> 3))

#define DISP_ON_Msk         (0x1)
#define DISP_SCAN_CPLT_Msk  (0x1 << 1)
/**************************
 * STATIC DECLARTATIONS
 ***********************/
static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *pixel_map);
static void lvgl_flush_wait_cb(lv_display_t *display);
static void lv_round_area_dimensions_cb(lv_event_t *event);
static void change_screen_cb(lv_timer_t *tim);

static uint8_t htiled_buf[HTILE_BUF_SIZE] = {0};
static uint8_t vtiled_buf[VTILE_BUF_SIZE] = {0};
static lv_display_t *display = NULL;
static lv_timer_t *tim = NULL;
static uint16_t curr_screen_id = SCREEN_ID_MAIN;
static uint8_t scan_stat = 0;
static uint8_t flags = 0x00 | (DISP_ON_Msk | DISP_SCAN_CPLT_Msk);
static char armed_val[9] = {'A', 'r', 'm', 'e', 'd', '\0'};
static char temp_val[4] = {'6', '7', '\0'};
static char hum_val[4] = {'6', '7', '\0'};
/*****************
 * PUBLIC APIs
 *****************/


static uint8_t initialized = 0;
void display_init(void) {
    lv_init();
    ssd1306_init();
    display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_tick_set_cb(HAL_GetTick);

    // disable antialiasing
    lv_display_set_antialiasing(display, DISABLE);
    // monochrome color format
    lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);

    // horizontally tiled display buffer configuration + flush callbacks
    lv_display_set_buffers(display, htiled_buf, NULL, HTILE_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    lv_display_add_event_cb(display, lv_round_area_dimensions_cb, LV_EVENT_INVALIDATE_AREA, display);

    tim = lv_timer_create(change_screen_cb, DISPLAY_REFR_DELAY, NULL);
    lv_timer_pause(tim);

    ui_init();

    lv_label_set_text_static(objects.label_hum_val, hum_val);
    lv_label_set_text_static(objects.label_temp_val, temp_val);
    lv_label_set_text_static(objects.label_armed_status_val, armed_val);
}


/**
 * @brief Checks whether the display is ON or OFF
 * 
 * This function should be called before making any calls
 * to lv_timer_handler(). If the display is not ON, do NOT
 * make a call to lv_timer_handler
 * 
 * @return 1 if display is ON; else 0
 */
uint8_t display_is_on(void) {
    return flags & DISP_ON_Msk;
}


/**
 * @brief Returns whether display scan visual is complete
 * 
 * This function should be called before scanning for any 
 * tags. Helps to mitigate multiple scans since MCU is much 
 * faster than user in terms of "scanning" items.
 * 
 * @return 1 if visual is complete; 0 else
 */
uint8_t display_scan_cplt(void) {
    return flags & DISP_SCAN_CPLT_Msk;
}


/**
 * @brief Turn the display ON allowing UI interaction
 * 
 * 
 * 
 */
void display_wake(void) {
    // wakes the display from sleep mode
    ssd1306_display_ctl(1);
    // additional logic to resume lvgl rendering logic
    flags |= DISP_ON_Msk;
}



/**
 * @brief Turns the display OFF disabling UI interaction
 * 
 * This function sets the display flag to OFF state. Should
 * start a timer that delays and calls a callback. If the flag
 * is still set to OFF the display should sleep. Else 
 * simply return. This makes it so that display isn't turning 
 * on and off unecessarily
 * 
 */
void display_sleep(void) {
    // puts the display into sleep mode
    ssd1306_display_ctl(0);
    // additional logic to pause lvgl updates and such
    flags &= ~DISP_ON_Msk;
}



void display_change_screen(struct _lv_obj_t *screen, uint16_t screen_id, uint8_t scan_failed) {
    if (screen == NULL) {
        screen = objects.item_scanning;
        curr_screen_id = SCREEN_ID_ITEM_SCANNING;
        flags &= ~(DISP_SCAN_CPLT_Msk);
    }
    else
        curr_screen_id = screen_id;

    scan_stat = scan_failed;
    lv_screen_load(screen);

    if (screen_id != SCREEN_ID_MAIN) {
        lv_timer_set_user_data(tim, (void *)&scan_stat);
        lv_timer_resume(tim);
    }
    else
        flags |= (DISP_SCAN_CPLT_Msk);
}


/**
 * @brief Refresh temp/humidity value on screen
 * 
 * 
 * @param[in] val
 * @param[in] hum_or_temp
 * 
 * @return 0 on success; else 1
 */
uint8_t display_refresh_value(disp_label_e label, uint16_t val) {
    switch (label) {
    case LABEL_HUM:
        lv_snprintf(hum_val, sizeof(hum_val), "%d", val);
        lv_label_set_text_static(objects.label_hum_val, NULL);
        break;
    case LABEL_TEMP:
        lv_snprintf(temp_val, sizeof(temp_val), "%d", val);
        lv_label_set_text_static(objects.label_temp_val, NULL);
        break;
    case LABEL_STATUS:
        lv_memset(armed_val, 0, sizeof(armed_val));
        if (val == 1)
            lv_snprintf(armed_val, sizeof(armed_val),  "ARMED");
        else if (val == 0)
            lv_snprintf(armed_val, sizeof(armed_val),  "DISARMED");
        else
            lv_snprintf(armed_val, sizeof(armed_val),  "BREACHED");

        lv_label_set_text_static(objects.label_armed_status_val, NULL);
        break;
    }
    
    return STATUS_OK;
}


/******************
 * STATIC DEFS
 ******************/

static void change_screen_cb(lv_timer_t *tim) {
    lv_timer_reset(tim);
    lv_timer_pause(tim);
    if (lv_display_get_screen_loading(display) == NULL) {
        uint8_t scan_stat = *((uint8_t *)lv_timer_get_user_data(tim));
        if (curr_screen_id == SCREEN_ID_ITEM_SCANNING) {
            if (scan_stat == 0)
                display_change_screen(objects.item_scanned, SCREEN_ID_ITEM_SCANNED, scan_stat);
            else
                display_change_screen(objects.item_scan_failed, SCREEN_ID_ITEM_SCAN_FAILED, scan_stat);
        }
        else if ((curr_screen_id == SCREEN_ID_ITEM_SCANNED) || (curr_screen_id == SCREEN_ID_ITEM_SCAN_FAILED))
            display_change_screen(objects.main, SCREEN_ID_MAIN, 0);
    }
}

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

