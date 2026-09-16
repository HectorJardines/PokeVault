#include "../Inc/drivers/io.h"
#include "../Inc/main.h"
#include "../Inc/drivers/aht20.h"
#include "../Inc/drivers/ssd1306.h"
#include "../Inc/app/display.h"
#include "../Inc/common/trace.h"
#include "./ui/ui.h"
#include "movement_detect.h"
// #include "../../Drivers/w5500_eth/TLS/SSLInterface.h"
#include "ir_sensors.h"
#include "security.h"
#include <stdio.h>
#include <string.h>

#define SCAN_DEBOUNCE   (750U) // MS
#define INIT_TICK       (0)
#define AUTH_PERIOD     (200U)
#define INVENT_PERIOD   (200U)
#define SENS_PERIOD     (100U)

static void test_setup(void) {
    trace_init();
    SystemClock_Config();
    IO_Init();
}

static void test_logging(void) {
    log_init();

    while(1) {
        log_debug("Hello...", 1);
        HAL_Delay(500);
    }
}

static void test_ir_sensors(void) {
    ir_init();

    ir_sens_state_t state;

    while (1) {
        ir_read_sens_state(&state);
        if (state.line_state == IR_LINE_CONNECTED)
            printf("LINE CONNECTED\r\n");
        if (state.presence_state == PIR_PRESENCE)
            printf("PRESENCE DETECTED\r\n");

        HAL_Delay(500);
    }
}


static void test_movement_sens(void) {
    movement_init();

    while (1) {
        if (movement_detected())
            printf("MOVEMENT DETECTED\r\n");
        if (movement_stopped())
            printf("NO MOVEMENT\r\n");
        if (movement_tap_detected()) 
            printf("DOUBLE TAP DETECTEd\r\n");
        
        // clear_movement();
        HAL_Delay(800);
    }
}

// static void test_temp_readings(void) {
//     aht20_init();
//     log_init();
//     aht20_sensor_measurements_t measurements;
//     while (1) {
//         aht20_read_data(AHT20_UNIT0, &measurements);
//         log_debug("UNIT 0 TEMP READING: ", measurements.UNIT0_DATA.temp);
//         log_debug("UNIT 1 HUM READING: ", measurements.UNIT0_DATA.humidity);
//         HAL_Delay(500);
//     }
// }

// static void test_keycard(void) {
//     keycard_init();
//     uint8_t led_status = HIGH;

//     while (1) {
//         if (keycard_register(KEYCARD_ENTRY1) == KEYCARD_OK) {
//             io_set_out(IO_TEST_LED, HIGH);
//             HAL_Delay(500);
//             break;
//         }
//     }

//     while (1) {
//         uint8_t uid[PICC_MEM_BLOCK_LEN];
//         keycard_status_e status = keycard_quick_scan();
//         if (status == KEYCARD_AUTHORIZED) {
//             io_set_out(IO_TEST_LED, HIGH);
//             io_set_out(IO_IR_EMIT_UNIT1, HIGH);
//             io_set_out(IO_IR_EMIT_UNIT2, HIGH);
//             io_set_out(IO_IR_EMIT_UNIT3, HIGH);
//             HAL_Delay(10000);
//             io_set_out(IO_TEST_LED, LOW);
//             io_set_out(IO_IR_EMIT_UNIT1, LOW);
//             io_set_out(IO_IR_EMIT_UNIT2, LOW);
//             io_set_out(IO_IR_EMIT_UNIT3, LOW);
//             led_status = !led_status;
//         }
//     }
// }


static void test_ssd1306(void) {
    ssd1306_init();

    while(1) {
        ssd1306_draw_pixel(62, 32, COLOR_WHITE);
        ssd1306_draw_pixel(63, 32, COLOR_WHITE);
        ssd1306_draw_pixel(64, 32, COLOR_WHITE);
        ssd1306_draw_pixel(65, 32, COLOR_WHITE);
        ssd1306_draw_pixel(66, 32, COLOR_WHITE);
        ssd1306_display();
        HAL_Delay(500);
        ssd1306_draw_pixel(32, 32, COLOR_BLACK);
        ssd1306_display();
        HAL_Delay(500);
    }
}

static void test_lvgl(void) {
    log_init();
    display_init();
    ui_init();

    while(1) {
        uint32_t time_till_next = lv_timer_handler();
        lv_sleep_ms(time_till_next);
    }
}
static lv_subject_t seconds;
static void test_lvgl_update_temp(void) {
    display_init();
    ui_init();

	uint32_t sec_elapsed = 0;
	uint16_t ms_per_sec = 1000;
    volatile uint32_t time_till_next = 0;
    volatile uint32_t dis_start_tick = HAL_GetTick(); 
    volatile uint32_t tim_start_tick = HAL_GetTick();
	uint8_t temp_buf[4];

    while(1) {
		if (HAL_GetTick() - dis_start_tick >= time_till_next) {
			time_till_next = lv_timer_handler();
            if (time_till_next == LV_NO_TIMER_READY)
                time_till_next = LV_DEF_REFR_PERIOD;
			dis_start_tick = HAL_GetTick();
		}

		if(HAL_GetTick() - tim_start_tick >= (ms_per_sec)) {
			sec_elapsed++;
			snprintf(temp_buf, 4, "%d", sec_elapsed);
			lv_label_set_text_static(objects.label_temp_val, temp_buf);
            tim_start_tick = HAL_GetTick();
		}
    }
}


// static void test_tag_read_data(void) {
//     uint8_t data[PICC_MEM_BLOCK_LEN];

//     rfid_tag_t tag;
//     tag_init();

//     while (1) {
//         if (tag_read_data(tag.uid, data, ITEM_SECTOR, TYPE_BLOCK)) {
//             printf("got data...\r\n");
//             HAL_Delay(1000);
//         }
//         else
//             printf("no card\r\n");
//     }
// }

static void test_system_messaging(void) {
    uint8_t status = STATUS_OK;
    struct security_sm_t main_sm;
    msg rx_msg = msg_init_default;
    memset((void *)&main_sm, 0, sizeof (struct security_sm_t));
    display_init();
    status |= system_monitor_init();
    message_init();
    status |= inventory_init();
    security_init(&main_sm);

    if (status) {
        while (1) {}
    }

    uint32_t display_tick = INIT_TICK, invent_tick = INIT_TICK, 
             sensor_tick = INIT_TICK, auth_tick = INIT_TICK;
    uint32_t disp_period = 0;

    uint32_t now = HAL_GetTick();

    while (1) {
        if (message_available())
            status = message_receive(&rx_msg);
        if ((now - auth_tick >= AUTH_PERIOD) && display_scan_cplt()
            && main_sm.current_state != SECURITY_DISARMED) // NO NEED TO SCAN FOR AUTH WHEN UNIT DISARMED
        {
            uint8_t card_present = system_check_card_auth();
            if (card_present)
                security_post_event(&main_sm, EVENT_TAG_AUTH);
            auth_tick = now;
        }
        if ((now - invent_tick >= INVENT_PERIOD) && display_scan_cplt()
            && main_sm.current_state != SECURITY_ARMED) // NO NEED TO SCAN FOR PRODUCTS IF UNIT ARMED
        {
            uint8_t item_present = inventory_scan_for_item();
            if (item_present == STATUS_OK)
                security_post_event(&main_sm, EVENT_ITEM_SCAN);
            invent_tick = now;
        }
        if (now - sensor_tick >= SENS_PERIOD) {
            system_process_state();
            sensor_tick = now;
        }
        if (display_is_on()) {
            if (now - display_tick >= disp_period) {
                disp_period = lv_timer_handler();
                if (disp_period == LV_NO_TIMER_READY)
                    disp_period = LV_DEF_REFR_PERIOD;
                display_tick = now;
            }
        }

        security_run(&main_sm, NULL);
        now = HAL_GetTick();
    }
}


int main(void) {
    SystemClock_Config();
    IO_Init();
    trace_init();
    test_system_messaging();
    return 0;
}