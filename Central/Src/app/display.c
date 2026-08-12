#include "../../Inc/app/display.h"
#include "../../Inc/drivers/sd_functions.h"
#include "../../Inc/app/inventory.h"
#include "../../Inc/drivers/ili9341.h"
#include "../../Inc/drivers/xpt2046.h"
#include "../../../Drivers/lvgl-master/include/lvgl/drivers/display/lv_ili9341.h"
#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"
#include "../ui/actions.h"


/*************
 * MACROS
 **************/

#define DISPLAY_BPP         (2U)    // bytes per pixel
#define DISPLAY_WIDTH       (240U) // px
#define DISPLAY_HEIGHT      (320U) // px


#define DISP_TOUCH_TIMEOUT  (20U) // ms
#define DISPLAY_PARTIAL_DIV (10U)
#define FRAME_BUF_SIZE      (((DISPLAY_WIDTH * DISPLAY_HEIGHT) / DISPLAY_PARTIAL_DIV) * DISPLAY_BPP)
#define DISP_TASK_STK_DEPTH (1024U)
#define DISP_TASK_PRIO      (4U)
#define INPUT_Q_LEN         (5U)


#define NODE_ID(node_bits)      ((node_bits) & (0x3U))
#define NODE_PG_IDX(node_bits)  (((node_bits) & (0x1FU << 2)) >> 2)
#define TOUCH_Msk               (0x01 << 0)
#define SCAN_Msk                (0x01 << 1)
#define SCAN_CPLT_Msk           (0x01 << 2)
#define INIT_INVENT_LOAD_Msk    (0x01 << 3)


/******************
 * TYPEDEFS
 *****************/
typedef struct {
    lv_display_t *dispp;
    lv_indev_t *input;
    uint8_t buf[FRAME_BUF_SIZE];
    uint8_t scan_state;
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


typedef struct {
    int32_t x;
    int32_t y;
} touch_coord_t;

/*************
 * STATIC DEC
 **************/
static void task_display(void *arg);
static void touch_input_cb(lv_indev_t *in, lv_indev_data_t *data);
static void update_items(void);
static void update_units(void);
static void xpt2046_touch_isr(void);
static void load_screen_cb(lv_event_t *e);

// DISPLAY DATA
static display_t ili_disp;
static invent_screen_t invent_content;
static units_screen_t unit_content;

// DISPLAY TASK
static TaskHandle_t disp_tsk;
static StaticTask_t _disp_tsk;
static StackType_t disp_stk[DISP_TASK_STK_DEPTH];


// INPUT QUEUE
static QueueHandle_t input_q;
static StaticQueue_t _input_q;
static uint8_t input_buf[INPUT_Q_LEN * sizeof(touch_coord_t)];
/***************
 * PUBLIC APIs
 *****************/

/**
 * @brief Initializes LVGL screens and ILI9341 display driver
 * 
 * 
 */
void display_init(void) {
    // INIT SUBMODULES
    spi_init();
    lv_init();
    xpt2046_init();
    lv_tick_set_cb(xTaskGetTickCount);

    // DISPLAY TOUCH INTERRUPT
    io_set_interrupt_prio(EXTI0_IRQ_NO, 5);
    io_configure_interrupt(IO_TOUCH_IT, IO_INTERRUPT_FT, xpt2046_touch_isr);

    input_q = xQueueCreateStatic(INPUT_Q_LEN, sizeof(touch_coord_t), input_buf, &_input_q);
    disp_tsk = xTaskCreateStatic(task_display, "Display Task", DISP_TASK_STK_DEPTH, 
                                NULL, DISP_TASK_PRIO, disp_stk, &_disp_tsk);
    if (disp_tsk == NULL || input_q == NULL) {
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
    lv_obj_t *obj = lv_event_get_target_obj(e);
    uint8_t node_bits = *((uint8_t *)lv_obj_get_user_data(obj)); 
    

    invent_content.valid_records = inventory_get_contents(NODE_ID(node_bits), invent_content.records, invent_content.pg_idx);
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
    lv_obj_t *obj = lv_event_get_target_obj(e);
    uint8_t node_bits = *((uint8_t *)lv_obj_get_user_data(obj));

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
    lv_obj_t *obj = lv_event_get_target_obj(e);
    uint8_t node_bits = *((uint8_t *)lv_obj_get_user_data(obj));

    invent_content.valid_records = inventory_get_contents(NODE_ID(node_bits), invent_content.records, invent_content.pg_idx);
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
    // signal to inventory task to scan for tag
    loadScreen(SCREEN_ID_SCAN_PROMPT);
    inventory_signal_scan(NULL);
    ili_disp.scan_state = 1;
}


void product_name_ready(lv_event_t *e) {
    uint8_t stat = 0;
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *text_ar = lv_event_get_target(e);
    if (code == LV_EVENT_READY) {
        loadScreen(SCREEN_ID_SCAN_PROMPT);
        const char *name = lv_textarea_get_text(text_ar);
        stat = inventory_signal_scan(name);
        ili_disp.scan_state = 1;
    }

    (void)stat;
}


void display_load_scanned_screen(void) {
    xTaskNotify(disp_tsk, SCAN_CPLT_Msk, eSetBits);
}

void display_load_scanning_screen(void) {
    xTaskNotify(disp_tsk, SCAN_Msk, eSetBits);
}

void display_first_load_ready(void) {
    xTaskNotify(disp_tsk, INIT_INVENT_LOAD_Msk, eSetBits);
}



/******************
 * STATIC DEFS
 *******************/
static void task_display(void *arg) {
    static uint32_t delay = 0, curr_tick = 0, notif = 0;


    // CONFIGURE DISPLAY SETTINGS
    ili_disp.dispp = lv_ili9341_create(DISPLAY_WIDTH, DISPLAY_HEIGHT, 0x00, ili9341_send_cmd, ili9341_send_pixels);
    lv_display_set_color_format(ili_disp.dispp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(ili_disp.dispp, ili_disp.buf, NULL, FRAME_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_rotation(ili_disp.dispp, LV_DISPLAY_ROTATION_90);
    // DISPLAY TOUCH INPUT DEV
    ili_disp.input = lv_indev_create();
    lv_indev_set_type(ili_disp.input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(ili_disp.input, touch_input_cb);
    lv_display_add_event_cb(ili_disp.dispp, load_screen_cb, LV_EVENT_SCREEN_LOADED, &ili_disp.scan_state);

    ui_init();
    io_irq_enable_interrupt(IO_TOUCH_IT);

    for(;;) {
        delay = lv_timer_handler();
        if (delay == LV_NO_TIMER_READY)
            delay = LV_DEF_REFR_PERIOD;

        curr_tick = xTaskGetTickCount;
        static touch_coord_t input;
        if (xTaskNotifyWait(0x00, (SCAN_CPLT_Msk | TOUCH_Msk | INIT_INVENT_LOAD_Msk | SCAN_Msk), &notif, pdMS_TO_TICKS(delay)) == pdTRUE) {
            if (notif & INIT_INVENT_LOAD_Msk)
                update_units();
            if (notif & TOUCH_Msk) {
                xpt2046_read_position(&input.x, &input.y);
                xQueueSendToBack(input_q, &input, 0);
            } else {
                input.x = -1; input.y = -1;
                xQueueSendToBack(input_q, &input, 0);
            }
            if (notif & SCAN_Msk)
                ili_disp.scan_state = 2;
            if (notif & SCAN_CPLT_Msk)
                ili_disp.scan_state = 3;
        }
    }

    UBaseType_t high_stk_usage = uxTaskGetStackHighWaterMark(NULL);
    // printf("DISPLAY TASK: FREE RAM = %d - %d\r\n", DISP_TASK_STK_DEPTH, high_stk_usage);
}


static void load_screen_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *d = lv_event_get_current_target_obj(e);
    uint8_t *load_state = ((uint8_t *)lv_obj_get_user_data(d));

    if (code == LV_EVENT_SCREEN_LOADED && *load_state > 1) {
        if (*load_state == 2) {
            loadScreen(SCREEN_ID_SCANNING);
        } else if (*load_state == 3) {
            loadScreen(SCREEN_ID_SCANNED);
            *load_state = 4;
        } else if (*load_state == 4) {
            loadScreen(SCREEN_ID_MAIN);
            *load_state = 0;
        }
    }
}


static void touch_input_cb(lv_indev_t *in, lv_indev_data_t *data) {
    static touch_coord_t touch;
    if (xQueueReceive(input_q, (void *)&touch, 0) == pdTRUE) {
        if (touch.x == -1 || touch.y == -1) {
            data->state = LV_INDEV_STATE_RELEASED;
        }
        else {
            data->point.x = touch.x;
            data->point.y = touch.y;
            data->state = LV_INDEV_STATE_PRESSED;
        }
    }
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
        lv_obj_set_user_data(button, &unit_content.units[i].id);
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
    vTaskNotifyGiveFromISR(disp_tsk, &hpt_ready);
    // portYIELD_FROM_ISR(hpt_ready);
}