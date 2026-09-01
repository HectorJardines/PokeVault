#include "../../Inc/app/display.h"
#include "../../Inc/drivers/sd_functions.h"
#include "../../Inc/app/inventory.h"
#include "../../Inc/drivers/ili9341.h"
#include "../../Inc/drivers/xpt2046.h"
#include "../../Inc/common/log.h"
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
#define DISP_TASK_STK_DEPTH (1256U)
#define DISP_TASK_PRIO      (4U)
#define INPUT_Q_LEN         (5U)
#define MAX_QSTR_LEN        (4U)

#define TOUCH_Msk                   (0x01 << 0)
#define SCAN_Msk                    (0x01 << 1)
#define SCAN_CPLT_Msk               (0x01 << 2)
#define INIT_INVENT_LOAD_Msk        (0x01 << 3)
#define ALL_Msk                     ( SCAN_CPLT_Msk| TOUCH_Msk | SCAN_Msk)


/******************
 * TYPEDEFS
 *****************/
typedef struct {
    lv_display_t *dispp;
    lv_indev_t *input;
    lv_timer_t *tran_tim;
    uint8_t buf[FRAME_BUF_SIZE];
    uint8_t scan_state;
} display_t;

typedef struct {
    uint8_t pg_idx;
    uint8_t valid_records;
    CsvRecord records[ITEMS_PER_SCREEN];
    char qty_strs[ITEMS_PER_SCREEN][4];
} invent_screen_t;


typedef struct {
    uint8_t pg_idx;
    uint8_t prev_pg_idx;
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
static void load_screen_cb(lv_timer_t *timer);
static void product_name_ready(lv_event_t *e);

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

    unit_content.prev_pg_idx = 0xFF;
    input_q = xQueueCreateStatic(INPUT_Q_LEN, sizeof(touch_coord_t), input_buf, &_input_q);
    disp_tsk = xTaskCreateStatic(task_display, "Display Task", DISP_TASK_STK_DEPTH, 
                                NULL, DISP_TASK_PRIO, disp_stk, &_disp_tsk);
    if (disp_tsk == NULL || input_q == NULL) {
        while (1) {}
    }
}



/**
 * @brief Signals inventory loaded to display task
 * 
 * This function notifies the display task of inventory
 * being loaded by the inventory task. I.e. the display task
 * can now update its contents.
 */
void display_first_load_ready(void) {
    xTaskNotify(disp_tsk, INIT_INVENT_LOAD_Msk, eSetBits);
}



/**
 * @brief Signals rfid tag scan complete to display task 
 * 
 * This API signals that an RFID tag has been successfully
 * scanned. Display task can then update its current screen
 * 
 */
void display_load_scanned_screen(void) {
    xTaskNotify(disp_tsk, SCAN_CPLT_Msk, eSetBits);
}



/**
 * @brief Signals start of RFID tag sequence to display task
 * 
 * This API signals that an RFID tag scan
 * is pending. Display task can then update its 
 * current screen to display a scan prompt
 *
 */
void display_load_scanning_screen(void) {
    xTaskNotify(disp_tsk, SCAN_Msk, eSetBits);
}



/**
 * @brief Retrieves unit contents and updates the display contents
 * 
 * This function is called on main screen load, or 
 * when next/prev main screen section is requested.
 * The API retrieves the necessary unit contents and
 * displays them on the screen.
 */
void display_update_units(void) {
    if (unit_content.pg_idx != unit_content.prev_pg_idx) // skip update if prev loaded content is same
        unit_content.valid_units = inventory_get_unit_stats(&unit_content.units, unit_content.pg_idx);
    if (unit_content.valid_units > 0)
            update_units();
}



/**
 * @brief Updates the items in the storage unit
 * 
 * This API is called on when a unit is selected.
 * The display task requests the unit contents from the 
 * inventory task and updates them on the display.
 * 
 */
void display_update_items(void) {
    update_items();
}



/**
 * @brief Configures the display and input device
 * 
 */
void display_configure(void) {
    ili_disp.dispp = lv_ili9341_create(DISPLAY_WIDTH, DISPLAY_HEIGHT, 0x00, ili9341_spi_send_cmd, ili9341_spi_send_pixels);
    xpt2046_reset_state();
    lv_display_set_color_format(ili_disp.dispp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(ili_disp.dispp, ili_disp.buf, NULL, FRAME_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_rotation(ili_disp.dispp, LV_DISPLAY_ROTATION_90);

    // DISPLAY TOUCH INPUT DEV
    ili_disp.input = lv_indev_create();
    lv_indev_set_type(ili_disp.input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(ili_disp.input, touch_input_cb);
    
    ili_disp.tran_tim = lv_timer_create(load_screen_cb, 700, &ili_disp.scan_state);
    lv_timer_pause(ili_disp.tran_tim);
    lv_timer_set_repeat_count(ili_disp.tran_tim, 1);

    io_irq_enable_interrupt(IO_TOUCH_IT);
}



/******************
 * STATIC DEFS
 *******************/
static void task_display(void *arg) {
    static uint32_t delay = 0, curr_tick = 0, notif = 0;
    
    // if (spi1_wait_init() == HAL_OK);
    display_configure();
    do {
        xTaskNotifyWait(0x00, INIT_INVENT_LOAD_Msk, &notif, portMAX_DELAY);
    } while (!(notif & INIT_INVENT_LOAD_Msk));
    ui_init();

    for(;;) {
        delay = lv_timer_handler();
        if (delay == LV_NO_TIMER_READY)
            delay = LV_DEF_REFR_PERIOD;

        static touch_coord_t input;
        if (xTaskNotifyWait(0x00, ALL_Msk, &notif, pdMS_TO_TICKS(delay)) == pdTRUE) {
            if (notif & TOUCH_Msk) {
                xpt2046_read_position(&input.x, &input.y);
                xQueueSendToBack(input_q, &input, 0);
            }
            if (notif & SCAN_Msk)
                loadScreen(SCREEN_ID_SCANNING);
            if (notif & SCAN_CPLT_Msk) {
                ili_disp.scan_state = 3;
                loadScreen(SCREEN_ID_SCANNED);
                lv_timer_resume(ili_disp.tran_tim);
            }
        }
        volatile UBaseType_t high_stk_usage = uxTaskGetStackHighWaterMark(NULL);
        if (high_stk_usage < 25) {
            for (;;);
        }
    }
}


static void load_screen_cb(lv_timer_t *timer) {
    uint8_t *load_screen = ((uint8_t *)lv_timer_get_user_data(timer));
    if (*load_screen == 3) {
        *load_screen = 0;
        loadScreen(SCREEN_ID_MAIN);
        lv_timer_reset(timer);
        lv_timer_pause(timer);
        lv_timer_set_repeat_count(ili_disp.tran_tim, 1);
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


static void product_name_ready(lv_event_t *e) {
    uint8_t stat = 0;
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *text_ar = lv_event_get_target(e);
    if (code == LV_EVENT_READY) {
        ili_disp.scan_state = 1;
        const char *name = lv_textarea_get_text(text_ar);
        stat = inventory_signal_scan(name);
        loadScreen(SCREEN_ID_SCAN_PROMPT);
    }

    (void)stat;
}


static void update_items(void) {
    lv_obj_t *container = NULL;
    lv_obj_t *label = NULL;
    for (uint8_t i = 0; i < ITEMS_PER_SCREEN; ++i) {
        container = lv_group_get_obj_by_index(groups.invent_items, i);
        if (i >= invent_content.valid_records) {
            lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
            continue;
        } else
            lv_obj_remove_flag(container, LV_OBJ_FLAG_HIDDEN);
        label = lv_obj_get_child(container, 0);
        lv_label_set_text_static(label, invent_content.records[i].name);
        label = lv_obj_get_child(container, 1);
        lv_label_set_text_static(label, invent_content.records[i].condition);
        label = lv_obj_get_child(container, 2);
        lv_snprintf(invent_content.qty_strs[i], MAX_QSTR_LEN, "%d", invent_content.records[i].qty);
        lv_label_set_text_static(label, invent_content.qty_strs[i]);
    }
}


static void update_units(void) {
    lv_obj_t *button = NULL;
    lv_obj_t *label;
    for (uint8_t i = 0; i < NODES_PER_SCREEN; ++i) {
        button = lv_group_get_obj_by_index(groups.grp_units, i);

        if (i >= unit_content.valid_units) {
            lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
            continue;
        } else
            lv_obj_remove_flag(button, LV_OBJ_FLAG_HIDDEN);

        label = lv_obj_get_child(button, 0); // UNIT ID
        lv_label_set_text_static(label, unit_content.units[i].id);
        lv_obj_set_user_data(button, &unit_content.units[i].id_val);
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
    static uint32_t prev_tick = 0;
    uint32_t tick = xTaskGetTickCount();
    if (tick - prev_tick < 100) {
        return;
    }
    prev_tick = tick;

    BaseType_t hpt_ready = pdFALSE;
    xTaskNotifyFromISR(disp_tsk, TOUCH_Msk, eSetBits, &hpt_ready);
    portYIELD_FROM_ISR(hpt_ready);
}



/***********************
 * DIISPLAY ACTIONS
 ***********************/
void action_back_to_main(lv_event_t * e) {
    unit_content.prev_pg_idx = unit_content.pg_idx;
    unit_content.pg_idx = 0;
    loadScreen(SCREEN_ID_MAIN);
}



/**
 * @brief Loads the next set of items to be displayed
 * 
 * The button should store hidden data, this data
 * should be initially set to index 0. Each call to 
 * action_next_items increments the index (unless 
 * there are no more items). Calls to  
 * decrement the index (unless first page).
 * 
 * This function will retrieve values from the inventory module
 * to display...
 */
void action_next_items(lv_event_t * e) {
    // AT MOST 4 ITEMS SCREEN PER NODE (use lower 2 bits)
    // UPPER 5 BITS USED FOR NODE ID
    lv_obj_t *obj = lv_event_get_target_obj(e);
    uint8_t node_id = *((uint8_t *)lv_obj_get_user_data(obj)); 
    
    invent_content.valid_records = inventory_get_contents(node_id, invent_content.records, invent_content.pg_idx + 1);
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
    uint8_t node_id = *((uint8_t *)lv_obj_get_user_data(obj));

    if (invent_content.pg_idx > 0) {
        invent_content.valid_records = inventory_get_contents(node_id, invent_content.records, invent_content.pg_idx - 1);
        if (invent_content.valid_records > 0) {
            update_items(); // could optionally display a blank screen
            invent_content.pg_idx--;
        }
    }
}



/**
 * @brief Loads the next set of units if available
 * 
 * 
 */
void action_next_units(lv_event_t *e) {
    uint8_t ret = inventory_get_unit_stats(unit_content.units, unit_content.pg_idx + 1);
    if (ret > 0) {
        update_units();
        unit_content.pg_idx++;
        unit_content.valid_units = ret;
    }
}




/**
 * @brief Load the previous set of units if available
 * 
 * 
 */
void action_prev_units(lv_event_t *e) {
    if (unit_content.pg_idx > 0) {
        uint8_t ret = inventory_get_unit_stats(unit_content.units, unit_content.pg_idx - 1);
        if (ret > 0) {
            unit_content.valid_units = ret;
            update_units();
            unit_content.pg_idx--;
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
    lv_obj_add_event_cb(objects.txt_ar_prod, product_name_ready, LV_EVENT_READY, NULL);
}



/**
 * @brief Loads the inventory screen associated with the unit
 * 
 * 
 */
void action_to_inventory(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target_obj(e);
    uint8_t node_id = *((uint8_t *)lv_obj_get_user_data(obj));

    invent_content.valid_records = inventory_get_contents(node_id, invent_content.records, invent_content.pg_idx);
    loadScreen(SCREEN_ID_INVENTORY); // gonna need to either block here or sleep the thread
}



/**
 * @brief Skips item registration, used for RFID card registering
 * 
 * 
 */
void action_scan_prompt(lv_event_t * e) {
    // signal to inventory task to scan for tag
    ili_disp.scan_state = 1;
    loadScreen(SCREEN_ID_SCAN_PROMPT);
    inventory_signal_scan(NULL);
}
