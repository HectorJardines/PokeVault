#include <stdio.h>
#include "../Inc/app/central_node.h"
#include "../Inc/app/central_message.h"
#include "../Inc/app/client.h"
#include "../../Core/Inc/common/trace.h"
#include "../Inc/drivers/io.h"
#include "../../Core/Inc/common/defines.h"
#include "main.h"

static void test_setup(void) {
    SystemClock_Config();
    IO_Init();
    trace_init();
}

static void test_central_node_message(void) {
    central_node_init();
    msg rx_msg = msg_init_default;
    uint8_t status = STATUS_WAIT;
    uint32_t timeout = 3000; // 100 ms timeout for node response
    uint32_t start = HAL_GetTick();

    while(1) {
        if (HAL_GetTick() - start >= timeout) {
            printf("alive\r\n");
            timeout_peer_poll();
            start = HAL_GetTick();
            status = STATUS_WAIT;
        }
        if (status == STATUS_WAIT)
            status = central_node_poll_peer();
        if (status == STATUS_ERR) {
            status = STATUS_WAIT;
            continue;
        }
        else if (status == STATUS_OK) {
            status = central_node_process();
            if (status == STATUS_OK)
                client_send();
        }
    }
}


int main(void) {
    test_setup();
    test_central_node_message();
}
