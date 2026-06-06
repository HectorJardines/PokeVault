#include "../Inc/drivers/io.h"
#include "../Inc/app/keycard.h"
#include "../Inc/main.h"
#include "../Inc/common/log.h"
#include "../Inc/drivers/aht20.h"
#include "../../Drivers/lvgl-master/lvgl.h"
#include "../Inc/drivers/ssd1306.h"
#include "../Inc/app/display.h"
#include "./ui/ui.h"

static void test_setup(void) {
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

static void test_temp_readings(void) {
    aht20_init();
    log_init();
    aht20_sensor_measurements_t measurements;
    while (1) {
        aht20_read_data(AHT20_UNIT0, &measurements);
        log_debug("UNIT 0 TEMP READING: ", measurements.UNIT0_DATA.temp);
        log_debug("UNIT 1 HUM READING: ", measurements.UNIT0_DATA.humidity);
        HAL_Delay(500);
    }
}

static void test_keycard(void) {
    keycard_init();
    uint8_t led_status = HIGH;

    while (1) {
        if (keycard_register(KEYCARD_ENTRY1) == KEYCARD_OK) {
            io_set_out(IO_TEST_LED, HIGH);
            HAL_Delay(500);
            break;
        }
    }

    while (1) {
        uint8_t uid[PICC_MEM_BLOCK_LEN];
        keycard_status_e status = keycard_quick_scan();
        if (status == KEYCARD_AUTHORIZED) {
            io_set_out(IO_TEST_LED, HIGH);
            io_set_out(IO_IR_EMIT_UNIT1, HIGH);
            io_set_out(IO_IR_EMIT_UNIT2, HIGH);
            io_set_out(IO_IR_EMIT_UNIT3, HIGH);
            HAL_Delay(10000);
            io_set_out(IO_TEST_LED, LOW);
            io_set_out(IO_IR_EMIT_UNIT1, LOW);
            io_set_out(IO_IR_EMIT_UNIT2, LOW);
            io_set_out(IO_IR_EMIT_UNIT3, LOW);
            led_status = !led_status;
        }
    }
}


static void test_ssd1306(void) {
    ssd1306_init();

    while(1) {

        ssd1306_draw_pixel(64, 32, COLOR_WHITE);
        ssd1306_display();
        HAL_Delay(500);
        ssd1306_draw_pixel(64, 32, COLOR_BLACK);
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


int main(void) {
    test_setup();
    test_lvgl();
    return 0;
}

