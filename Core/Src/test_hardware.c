#include "../Inc/drivers/io.h"
#include "../Inc/app/keycard.h"
#include "../Inc/main.h"
#include "../Inc/common/log.h"

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

int main(void) {
    test_setup();
    test_logging();
    return 0;
}

