#include "../../Inc/app/display.h"
#include "../../Inc/drivers/sd_functions.h"
#include "../../Inc/app/inventory.h"
#include "../../Inc/drivers/ili9341.h"
#include "../../Inc/drivers/xpt2046.h"
#include "../../../Drivers/lvgl-master/include/lvgl/drivers/display/lv_ili9341.h"
#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"
#include "../ui/actions.h"


#define DISPLAY_BPP         (2U)    // bytes per pixel
#define DISPLAY_WIDTH       (240U) // px
#define DISPLAY_HEIGHT      (320U) // px


#define DISP_TOUCH_TIMEOUT  (20U) // ms
#define DISPLAY_PARTIAL_DIV (10U)
#define FRAME_BUF_SIZE      (((DISPLAY_WIDTH * DISPLAY_HEIGHT) / DISPLAY_PARTIAL_DIV) * DISPLAY_BPP)
#define DISP_TASK_STK_DEPTH (2048U)
#define DISP_TASK_PRIO      (4U)


#define NODE_ID(node_bits)      ((node_bits) & (0x3U))
#define NODE_PG_IDX(node_bits)  (((node_bits) & (0x1FU << 2)) >> 2)

typedef struct {
    lv_disp_t *dispp;
    lv_indev_t *input;
    TaskHandle_t task;
    uint8_t buf[FRAME_BUF_SIZE];
} display_t;

typedef struct {
    uint8_t pg_idx;
    uint8_t valid_records;
    CsvRecord records[ITEMS_PER_SCREEN];
} invent_screen_t;


typedef struct {
    uint8_t pg_idx;
    uint8_t valid_units;
    unit_record_t units[NODES_PER_SCREEN];
} units_screen_t;

/*************
 * STATIC DEC
 **************/
static void task_display(void *arg);
static void touch_input_cb(lv_indev_t *in, lv_indev_data_t *data);
static void update_items(void);
static void update_units(void);


static display_t disp = {{0}};
static invent_screen_t invent_content;
static units_screen_t unit_content;
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

    // touchscreen input
    disp.input = lv_indev_create();
    lv_indev_set_type(disp.input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(disp.input, touch_input_cb);

    io_configure_interrupt(IO_A0, IO_INTERRUPT_FT, xpt2046_touch_isr);
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
    invent_content.pg_idx = 0;
    loadScreen(SCREEN_ID_MAIN);

    unit_content.valid_units = inventory_get_unit_stats(unit_content.units, unit_content.pg_idx);
    if (unit_content.valid_units > 0)
        update_units();
}


/**
 * @brief Loads the next set of items to be displayed
 * 
 * The button should store hidden data, this data
 * should be initially set to index 0. Each call to 
 * action_next_items increments the index (unless 
 * there are no more items). Calls to action_previous_items
 * decrement the index (unless first page).
 * 
 * This function will retrieve values from the inventory module
 * to display...
 */
void action_next_items(lv_event_t * e) {
    // AT MOST 4 ITEMS SCREEN PER NODE (use lower 2 bits)
    // UPPER 5 BITS USED FOR NODE ID
    uint8_t node_bits = *((uint8_t *)lv_obj_get_user_data(e));    
    

    invent_content.valid_records = inventory_get_contents(NODE_ID(node_bits), &invent_content.records, invent_content.pg_idx);
    if (invent_content.valid_records > 0) {
        update_items();
        invent_content.pg_idx++;
    }
}


/**
 * @brief Loads the previous set of items to display
 * 
 * The button stores hidden data indicating the current page index,
 * initially set to 0. Refer to action_next_items for more 
 * information...
 * 
 * 
 */
void action_previous_items(lv_event_t * e) {
    uint8_t node_bits = *((uint8_t *)lv_obj_get_user_data(e));

    if (invent_content.pg_idx > 0) {
        invent_content.pg_idx--;
        invent_content.valid_records = inventory_get_contents(NODE_ID(node_bits), invent_content.records, invent_content.pg_idx);
        if (invent_content.valid_records > 0) {
            update_items(); // could optionally display a blank screen
        }
    }
}


/**
 * @brief Display the tag register prompt
 * 
 * 
 */
void action_register_prompt(lv_event_t * e) {
    loadScreen(SCREEN_ID_ADD_ITEM);
}

/**
 * @brief Loads the inventory screen associated with the unit
 * 
 * 
 */
void action_to_inventory(lv_event_t * e) {
    loadScreen(SCREEN_ID_INVENTORY); // gonna need to either block here or sleep the thread    
    uint8_t node_bits = *((uint8_t *)lv_obj_get_user_data(e));

    invent_content.valid_records = inventory_get_contents(NODE_ID(node_bits), &invent_content.records, invent_content.pg_idx);
    if (invent_content.valid_records > 0) {
        update_items();
    }
}

/**
 * @brief Skips item registration, used for RFID card registering
 * 
 * 
 */
void action_scan_prompt(lv_event_t * e) {
    loadScreen(SCREEN_ID_SCAN_PROMPT);
    // signal to inventory task to scan for tag
    inventory_signal_scan(NULL);
}


void product_name_ready(lv_event_t *e) {
    uint8_t stat = 0;
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *text_ar = lv_event_get_target(e);
    if (code == LV_EVENT_READY) {
        const char *name = lv_textarea_get_text(text_ar);
        stat = inventory_signal_scan(name);
    }
}



/******************
 * STATIC DEFS
 *******************/
static void task_display(void *arg) {
    uint8_t touch_active = pdFALSE, notif = 0;
    uint32_t delay = 0;

    disp.task = xTaskGetCurrentTaskHandle();
    ui_init();
    io_irq_enable_interrupt(IO_A0);

    for(;;) {
        delay = lv_timer_handler();
        if (touch_active == pdTRUE) {
            delay = delay > DISP_TOUCH_TIMEOUT ? DISP_TOUCH_TIMEOUT : delay;
        }
        else if (delay == LV_NO_TIMER_READY)
            delay = LV_DEF_REFR_PERIOD;

        if (xTaskNotifyWait(0x00, 0x01, &notif, pdMS_TO_TICKS(delay)) == pdTRUE)
            touch_active == pdTRUE;

        if (lv_indev_get_state(disp.input) == LV_INDEV_STATE_RELEASED)
            touch_active = pdFALSE;
    }
}


static void touch_input_cb(lv_indev_t *in, lv_indev_data_t *data) {
    xpt2046_read_position(&data->point.x, &data->point.y);
    if (data->point.x == -1 || data->point.y == -1) {
        data->point.x = 0;
        data->point.y = 0;
        data->state = LV_INDEV_STATE_RELEASED;
    }
    else
        data->state = LV_INDEV_STATE_PRESSED;
}


static void update_items(void) {
    lv_obj_t *container = NULL;
    lv_obj_t *label = NULL;
    for (uint8_t i = 0; i < ITEMS_PER_SCREEN; ++i) {
        container = lv_group_get_obj_by_index(groups.invent_items, i);
        label = lv_obj_get_child(container, 0);
        lv_label_set_text_static(label, invent_content.records[i].name);
        label = lv_obj_get_child(container, 1);
        lv_label_set_text_static(label, "NM");
        label = lv_obj_get_child(container, 2);
        lv_label_set_text_static(label, "2");
    }
}



static void update_units(void) {
    lv_obj_t *button = NULL;
    lv_obj_t *label;
    for (uint8_t i = 0; i < unit_content.valid_units; ++i) {
        button = lv_group_get_obj_by_index(groups.grp_units, i);
        label = lv_obj_get_child(button, 0); // UNIT ID
        lv_label_set_text_static(label, unit_content.units[i].id);
        label = lv_obj_get_child(button, 1);
        if (unit_content.units[i].armed == 0)
            lv_label_set_text_static(label, "ARMED");
        else
            lv_label_set_text_static(label, "DISARMED");
        label = lv_obj_get_child(button, 2);
        lv_label_set_text_static(label, unit_content.units[i].capacity);
    }
}


/**
 * @brief Signal touch event to display task
 * 
 * 
 * 
 */
static void xpt2046_touch_isr(void) {
    BaseType_t hpt_ready = pdFALSE;
    xTaskNotifyFromISR(disp.task, 0x01, eSetBits, &hpt_ready);
    portYIELD_FROM_ISR(hpt_ready);
}