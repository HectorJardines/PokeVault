#include "../../Inc/app/display.h"
#include "../../Inc/drivers/ili9341.h"
#include "../../../Drivers/lvgl-master/include/lvgl/drivers/display/lv_ili9341.h"
#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"
#include "../ui/actions.h"


#define DISPLAY_BPP         (2U)    // bytes per pixel
#define DISPLAY_WIDTH       (240U) // px
#define DISPLAY_HEIGHT      (320U) // px
#define DISPLAY_PARTIAL_DIV (10U)
#define FRAME_BUF_SIZE      (((DISPLAY_WIDTH * DISPLAY_HEIGHT) / DISPLAY_PARTIAL_DIV) * DISPLAY_BPP)
#define DISP_TASK_STK_DEPTH (2048U)
#define DISP_TASK_PRIO      (4U)

typedef struct {
    uint8_t buf[FRAME_BUF_SIZE];
    lv_disp_t *dispp;
} display_t;

/*************
 * STATIC DEC
 **************/
static void task_display(void *arg);



static display_t disp = {{0}};
/***************
 * PUBLIC APIs
 *****************/

/**
 * @brief Initializes LVGL screens and ILI9341 display driver
 * 
 * 
 */
void display_init(void) {
    lv_init();

    disp.dispp = lv_ili9341_create(DISPLAY_WIDTH, DISPLAY_HEIGHT, 0x00, ili9341_send_cmd, ili9341_send_pixels);
    lv_display_set_color_format(disp.dispp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp.dispp, disp.buf, NULL, FRAME_BUF_SIZE, LV_DISP_RENDER_MODE_PARTIAL);

    ui_init();
    uint8_t stat = xTaskCreate(task_display, "Display Task", DISP_TASK_STK_DEPTH, 
                NULL, DISP_TASK_PRIO, NULL);

    if (stat != pdPASS) {
        while (1) {}
    }
}


void action_back_to_main(lv_event_t * e) {

}


void action_next_items(lv_event_t * e) {

}


void action_previous_items(lv_event_t * e) {

}



/******************
 * STATIC DEFS
 *******************/
static void task_display(void *arg) {
    uint32_t delay = 0;
    ui_init();

    for(;;) {
        delay = lv_timer_handler();
        if (delay == LV_NO_TIMER_READY)
            delay = LV_DEF_REFR_PERIOD;
        lv_sleep_ms(delay);
    }
}
