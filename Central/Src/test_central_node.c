#include "../Inc/common/printf-stdarg.h"
#include "../Inc/app/central_node.h"
#include "../Inc/app/rfid_tag.h"
#include "../Inc/app/central_message.h"
#include "../Inc/app/client.h"
#include "../../Core/Inc/common/trace.h"
#include "../Inc/drivers/io.h"
#include "../../Core/Inc/common/defines.h"
#include "../Inc/app/display.h"
#include "main.h"
# include "../Inc/app/inventory.h"
#include "../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../FreeRTOS_WrkSpace/include/task.h"

/* Static memory allocation buffers for the Idle Task */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

/* Static memory allocation buffers for the Timer Task */
#if (configUSE_TIMERS == 1)
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
#endif

/**
 * @brief Provides memory for the FreeRTOS Idle Task when configSUPPORT_STATIC_ALLOCATION = 1.
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                  StackType_t **ppxIdleTaskStackBuffer,
                                  uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/**
 * @brief Provides memory for the FreeRTOS Timer Task when configSUPPORT_STATIC_ALLOCATION = 1.
 */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                   StackType_t **ppxTimerTaskStackBuffer,
                                   uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* Silence compiler warnings about unused parameters */
    (void) xTask;
    (void) pcTaskName;

    /* CRITICAL: The stack has overflowed here. Do not try to recover. */
    /* Insert code to safely stop your system, log the task name, or reset. */
    taskDISABLE_INTERRUPTS();
    for( ;; );
}

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

static void test_system_messaging(void) {
    central_node_init();
    c_message_init();
    client_init();
    init_print();
    log_init();

    vTaskStartScheduler();
    while(1) {
        
    }
}


static void test_system_display(void) {
    init_print();
    display_init();

    vTaskStartScheduler();
    while(1) {
        
    }
}


static void test_log_to_sd(void) {
    init_print();
    log_init();

    vTaskStartScheduler();
    while (1) {

    }
}


static void test_tag_register(void) {
    init_print();
    log_init();
    display_init();
    c_inventory_init();

    vTaskStartScheduler();
    while(1) {

    }
}


int main(void) {
    test_setup();
    test_tag_register();
}
