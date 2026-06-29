// #include "lvgl.h"
#include "../Inc/drivers/io.h"
#include "../Inc/app/keycard.h"
#include "../Inc/main.h"
#include "../Inc/common/log.h"
#include "../Inc/drivers/aht20.h"
#include "../Inc/drivers/ssd1306.h"
#include "../Inc/app/display.h"
#include "./ui/ui.h"
#include "../Inc/drivers/w5500_ethernet.h"
#include "../../Drivers/w5500_eth/TLS/SSLInterface.h"
#include <stdio.h>
#include <string.h>

static void test_setup(void) {
    log_init();
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

static void test_w5500_connect(void) {
    spi_init(SPI_DEVICE_W5500);
    w5500_init();

    while (1) {

    }
}

static void test_w5500_tls_connect(void) {
    wiz_tls_context tls_context;
    int sock_num = 0, SERVER_PORT = 443;
    unsigned char sslHostName[] = "api.telegram.org";
    uint8_t host_ip[4];

    spi_init(SPI_DEVICE_W5500);
    w5500_init();
    w5500_resolve_hostname(sslHostName, host_ip);
    wiz_tls_init(&tls_context, &sock_num);
    wiz_tls_connect(&tls_context, SERVER_PORT, host_ip);

    while(1) {

    }
}

static char https_req[512] = {0};
static char out_buf[1024] = {0};
static char token[] = "8917121504:AAFOWveV3FwXW2nBlwKqddMPV_wNWngRNeU";
static char message[] = "Ping From ST...";
static void test_w5500_https_post(void) {
    wiz_tls_context tls_context;
    int sock_num = 0, SERVER_PORT = 443;
    unsigned char sslHostName[] = "api.telegram.org";
    uint8_t host_ip[4];
    uint64_t chat_id = 8807953801;
    uint32_t chat_id_h = (uint32_t)(chat_id / 1000000000ULL);
    uint32_t chat_id_l = (uint32_t)(chat_id % 1000000000ULL);

    snprintf(https_req, sizeof(https_req), "{\"chat_id\": %u%09u, \"text\": \"%s\"}",
            chat_id_h, chat_id_l, message);
    snprintf(out_buf, sizeof(out_buf), 
            "POST /bot%s/sendMessage HTTP/1.1\r\n"
            "Host: api.telegram.org\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %u\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            token, (uint32_t)strlen((const char *)https_req), https_req);
    
    spi_init(SPI_DEVICE_W5500);
    w5500_init();
    w5500_resolve_hostname(sslHostName, host_ip);
    wiz_tls_init(&tls_context, &sock_num);
    wiz_tls_connect(&tls_context, SERVER_PORT, host_ip);

    while (1) {
        wiz_tls_write(&tls_context, (unsigned char *)out_buf, strlen((const char*)out_buf));
        HAL_Delay(600000);
    }
}


int main(void) {
    test_setup();
    test_w5500_https_post();
    return 0;
}

