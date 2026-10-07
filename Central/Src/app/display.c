#include "../../Inc/app/display.h"
#include "../../Inc/drivers/sd_functions.h"
#include "../../Inc/app/inventory.h"
#include "../../Inc/drivers/ili9341.h"
#include "../../Inc/drivers/xpt2046.h"
#include "../../Inc/common/log.h"
#include "../../../Drivers/lvgl-master/include/lvgl/drivers/display/lv_ili9341.h"
#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"
#include "../../../FreeRTOS_WrkSpace/include/queue.h"
#include "../ui/actions.h"


/*************
 * MACROS
 **************/

#define DISPLAY_BPP         (2U)    // bytes per pixel
#define DISPLAY_WIDTH       (240U) // px
#define DISPLAY_HEIGHT      (320U) // px

#define DISP_TOUCH_TIMEOUT  (20U) // ms
#define DISPLAY_PARTIAL_DIV (10U)
// #define FRAME_BUF_SIZE      (((DISPLAY_WIDTH * DISPLAY_HEIGHT) / DISPLAY_PARTIAL_DIV) * DISPLAY_BPP)
#define FRAME_BUF_SIZE      (DISPLAY_WIDTH * sizeof(lv_color16_t) * 8)
#define DISP_TASK_STK_DEPTH (1256U)
#define DISP_TASK_PRIO      (4U)
#define INPUT_Q_LEN         (5U)
#define EVT_Q_LEN           (3U)
#define MAX_QSTR_LEN        (4U)

#define INIT_INVENT_LOAD_Msk        (1 << 0)

#define EVT_TOUCH                   (0)
#define EVT_SCAN_START              (1)
#define EVT_SCAN_CPLT               (2)
#define EVT_UNIT_UPDATE             (4)
#define EVT_ITEM_UPDATE             (5)
#define ALL_Msk                     ( 0xFFFFFFFF )


#define TOUCH_DEBOUNCE_MS           (500U)

/******************
 * TYPEDEFS
 *****************/
typedef struct {
    lv_display_t *dispp;
    lv_indev_t *input;
    lv_timer_t *tran_tim;
    uint8_t buf[FRAME_BUF_SIZE];
    uint8_t scan_state;
    uint8_t curr_node;
    uint8_t curr_screen;

    char ta_name[MAX_ITEM_NAME_LEN];
    char ta_cond[MAX_ITEM_CND_LEN];
} display_t;


typedef struct {
    int32_t x;
    int32_t y;
} touch_coord_t;


struct display_evt {
    uint8_t type;
    union {
        invent_screen_t updated_invent;
        units_screen_t update_units;
    };
};

/*************
 * STATIC DEC
 **************/
static void display_configure(void);
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

// DISP EVT QUEUE
static QueueHandle_t evt_q;
static StaticQueue_t _evt_q;
static uint8_t evt_buf[EVT_Q_LEN * sizeof(struct display_evt)];

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
    sizeof(struct display_evt);
    sizeof(invent_content.records);
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
    evt_q = xQueueCreateStatic(EVT_Q_LEN, sizeof(struct display_evt), evt_buf, &_evt_q);
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
    struct display_evt evt;
    evt.type = EVT_SCAN_CPLT;
    xQueueSendToBack(evt_q, &evt, portMAX_DELAY);
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
    struct display_evt evt;
    evt.type = EVT_SCAN_START;
    xQueueSendToBack(evt_q, &evt, portMAX_DELAY);
}

/**
 * @brief Signal the unit status has changed for some unit
 * 
 * 
 * 
 */
void display_signal_invent_change(uint8_t type, void *update) {
    struct display_evt evt;

    if (type == DISP_UNIT_CHANGE && (ili_disp.curr_screen == SCREEN_ID_MAIN)) {
        evt.type = EVT_UNIT_UPDATE;
        memcpy((void *)&evt.update_units, (const void *)update, sizeof(evt.update_units));
    }
    else if (type == DISP_ITEM_CHANGE && (ili_disp.curr_screen == SCREEN_ID_INVENTORY)) {
        evt.type = EVT_ITEM_UPDATE;
        memcpy((void *)&evt.updated_invent, (const void *)update, sizeof(evt.updated_invent));
    }

    xQueueSendToBack(evt_q, &evt, portMAX_DELAY);
}



/**
 * @brief Retrieves unit contents and updates the display contents
 * 
 * This function is called on main screen load, or 
 * when next/prev main screen section is requested.
 * The API retrieves the necessary unit contents and
 * displays them on the screen.
 */
int32_t display_req_unit_update(uint8_t pg_idx) {
    // if (unit_content.pg_idx != unit_content.prev_pg_idx) // skip update if prev loaded content is same
    if (pg_idx == 0xFF) pg_idx = unit_content.pg_idx;
    msg req_msg = {.command = CMD_GET_NODE_STATS,
                    .which_payload = pg_idx
                };
    inventory_post_event(&req_msg);
    ili_disp.curr_screen = SCREEN_ID_MAIN;
}


/**
 * @brief Updates the items in the storage unit
 * 
 * This API is called on when a unit is selected.
 * The display task requests the unit contents from the 
 * inventory task and updates them on the display.
 * 
 */
int32_t display_req_item_update(uint8_t node_id, uint8_t pg_idx) {
    msg req_msg = {.command = CMD_GET_INVENT_STATS,
                    .node_id = node_id,
                    .which_payload = pg_idx
                };
    inventory_post_event(&req_msg);
    ili_disp.curr_screen = SCREEN_ID_INVENTORY;
}



/******************
 * STATIC DEFS
 *******************/
static void task_display(void *arg) {
    static uint32_t delay = 0, curr_tick = 0, notif = 0;
    struct display_evt evt;
    
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
        if (xQueueReceive(evt_q, &evt, delay)) {
            switch (evt.type) {
            case EVT_TOUCH:
                xpt2046_read_position(&input.x, &input.y);
                xQueueSendToBack(input_q, &input, 0);
                break;
            case EVT_SCAN_START:
                loadScreen(SCREEN_ID_SCANNING);
                break;
            case EVT_SCAN_CPLT:
                ili_disp.scan_state = 3;
                loadScreen(SCREEN_ID_SCANNED);
                lv_timer_resume(ili_disp.tran_tim);
                break;
            case EVT_ITEM_UPDATE:
                if (invent_content.pg_idx == evt.updated_invent.pg_idx)
                    memcpy((void *)&invent_content, (const void *)&evt.updated_invent, sizeof(invent_content));
                update_items();
                break;
            case EVT_UNIT_UPDATE:
                if (unit_content.pg_idx == evt.update_units.pg_idx)
                    memcpy((void *)&unit_content, (const void *)&evt.update_units, sizeof(unit_content));
                update_units();
                break;
            }
        }
        volatile UBaseType_t high_stk_usage = uxTaskGetStackHighWaterMark(NULL);
        if (high_stk_usage < 25) {
            for (;;);
        }
    }
}


/**
 * @brief Configures the display and input device
 * 
 */
static void display_configure(void) {
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

        memset((void *)ili_disp.ta_cond, 0, MAX_ITEM_CND_LEN);
        const char *cond = lv_textarea_get_text(text_ar);
        lv_snprintf(ili_disp.ta_cond, MAX_ITEM_CND_LEN, "%s", cond);

        memset((void *)ili_disp.ta_name, 0, MAX_ITEM_NAME_LEN);
        const char *name = lv_textarea_get_text(objects.txt_ar_prod);
        lv_snprintf(ili_disp.ta_name, MAX_ITEM_NAME_LEN, "%s", name);

        stat = inventory_signal_scan(ili_disp.ta_name, ili_disp.ta_cond);
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
        snprintf(unit_content.units[i].id, sizeof(unit_content.units[i].id), "UNIT %d", unit_content.units[i].id_val);
        lv_label_set_text_static(label, unit_content.units[i].id);
        lv_obj_set_user_data(button, &unit_content.units[i].id_val);
        label = lv_obj_get_child(button, 1); // ARMED STATUS
        if (unit_content.units[i].data.armed == 0)
            lv_label_set_text_static(label, "DISARMED");
        else if (unit_content.units[i].data.armed == 1)
            lv_label_set_text_static(label, "ARMED");
        else 
            lv_label_set_text_static(label, "BREACHED");
        label = lv_obj_get_child(button, 2); // UNIT CAPACITY
        snprintf(unit_content.units[i].capacity, sizeof(unit_content.units[i].capacity), "%02d/%02d", unit_content.units[i].data.cap_val, MAX_ITEMS);
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
    if (tick - prev_tick < TOUCH_DEBOUNCE_MS)
        return;
    prev_tick = tick;

    struct display_evt evt;
    evt.type = EVT_TOUCH;

    BaseType_t hpt_ready = pdFALSE;
    xQueueSendToBackFromISR(evt_q, &evt, &hpt_ready);
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


void action_focus_ta(lv_event_t *e) {
    lv_obj_t *ta = lv_event_get_current_target_obj(e);
    if (ta == objects.txt_ar_cnd)
        lv_keyboard_set_textarea(objects.kb1, objects.txt_ar_cnd);
    else if (ta == objects.txt_ar_prod)
        lv_keyboard_set_textarea(objects.kb1, objects.txt_ar_prod);
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
    
    int32_t ret = display_req_item_update(node_id, invent_content.pg_idx + 1);
    if (!ret)
        invent_content.pg_idx++;
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
        int32_t ret = display_req_item_update(node_id, invent_content.pg_idx - 1);
        if (!ret)
            invent_content.pg_idx--;
    }
}



/**
 * @brief Loads the next set of units if available
 * 
 * 
 */
void action_next_units(lv_event_t *e) {
    int32_t ret = display_req_unit_update(unit_content.pg_idx + 1);
    if (!ret)
        unit_content.pg_idx++;
}




/**
 * @brief Load the previous set of units if available
 * 
 * 
 */
void action_prev_units(lv_event_t *e) {
    if (unit_content.pg_idx > 0) {
        int32_t ret = display_req_unit_update(unit_content.pg_idx - 1);
        if (!ret) 
            unit_content.pg_idx--;
    }
}



/**
 * @brief Display the tag register prompt
 * 
 * 
 */
void action_register_prompt(lv_event_t * e) {
    loadScreen(SCREEN_ID_ADD_ITEM);
    lv_obj_add_event_cb(objects.txt_ar_cnd, product_name_ready, LV_EVENT_READY, NULL);
    ili_disp.curr_screen = SCREEN_ID_ADD_ITEM;
}



/**
 * @brief Loads the inventory screen associated with the unit
 * 
 * 
 */
void action_to_inventory(lv_event_t * e) {
    lv_obj_t *obj = lv_event_get_target_obj(e);
    uint8_t node_id = *((uint8_t *)lv_obj_get_user_data(obj));

    int32_t ret = display_req_item_update(node_id, invent_content.pg_idx);
    if (!ret) {
        loadScreen(SCREEN_ID_INVENTORY); // gonna need to either block here or sleep the thread
        ili_disp.curr_node = node_id;
    }
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
    inventory_signal_scan(NULL, NULL);
    ili_disp.curr_screen = SCREEN_ID_SCAN_PROMPT;
}
