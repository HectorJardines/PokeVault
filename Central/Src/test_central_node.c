#include <stdio.h>
#include "../Inc/app/central_node.h"
#include "../Inc/app/rfid_tag.h"
#include "../Inc/app/central_message.h"
#include "../Inc/app/client.h"
#include "../../Core/Inc/common/trace.h"
#include "../Inc/drivers/io.h"
#include "../../Core/Inc/common/defines.h"
#include "../../Core/Inc/app/display.h"
#include "main.h"

static void test_setup(void) {
    SystemClock_Config();
    IO_Init();
    trace_init();
}

// static void test_central_node_message(void) {
//     central_node_init();
//     msg rx_msg = msg_init_default;
//     uint8_t status = STATUS_WAIT;
//     uint32_t timeout = 500; // 100 ms timeout for node response
//     uint32_t start = HAL_GetTick();

//     while(1) {
//         if (!client_connected())
//             client_connect();

//         if (HAL_GetTick() - start >= timeout) {
//             printf("alive\r\n");
//             timeout_peer_poll();
//             start = HAL_GetTick();
//             status = STATUS_WAIT;
//         }
//         if (status == STATUS_WAIT)
//             status = central_node_poll_peer();
//         if (status == STATUS_ERR) {
//             status = STATUS_WAIT;
//             continue;
//         }
//         else if (status == STATUS_OK) {
//             status = central_node_process();
//         }

//         if (client_messages_pending())
//             client_send();
//     }
// }

static void test_register_tag(void) {
    tag_init();
    // display_init();
    // ui_init();

    // volatile uint32_t time_till_next = lv_timer_handler();
    // volatile uint32_t dis_start_tick = HAL_GetTick();

    while(1) {
		// if (HAL_GetTick() - dis_start_tick >= time_till_next) {
		// 	time_till_next = lv_timer_handler();
        //     if (time_till_next == LV_NO_TIMER_READY)
        //         time_till_next = LV_DEF_REFR_PERIOD;
		// 	dis_start_tick = HAL_GetTick();
		// }

        if (tag_register(TAG_AUTH_CARD, NULL) == STATUS_OK) {
            io_set_out(IO_TEST_LED, IO_OUT_HIGH);
            HAL_Delay(1000);
            io_set_out(IO_TEST_LED, IO_OUT_LOW);
            HAL_Delay(1000);
        }
        else
            printf("no card\r\n");
    }
}


int main(void) {
    test_setup();
    test_register_tag();
}
