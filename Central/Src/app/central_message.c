#include "../../Inc/app/central_message.h"
#include "../../Inc/app/central_node.h"
#include "../../Inc/drivers/rs485_cobs.h"
#include "../../../Core/Inc/common/ring_buffer.h"
#include "../../../Core/Inc/common/defines.h"
#include "../../../Drivers/nanopb/pb_encode.h"
#include "../../../Drivers/nanopb/pb_decode.h"

#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"
#include "../../../FreeRTOS_WrkSpace/include/queue.h"

#include <stdio.h>

/*************************
 * MACROS
 ***************/
#define MAX_MSG_CNT_OUT (10U) // much smaller struct
#define MAX_MSG_PER_CTS (5U)

#define MSG_POST_OUT_TIMEOUT    (pdMS_TO_TICKS(50))
#define MSG_CTLR_STACK_DEPTH    (512U)
#define MSG_CTLR_PRIO           (5U)


/**************
 * TYPEDEFS
 *****************/

/************************
 * STATIC DECLARATIONS
 ***********************/
static uint8_t serialize_struct(msg* message, uint32_t *length);
static uint8_t deserialize_msg_buf(uint8_t *serial_buf, uint32_t length, msg_array *message);
static uint16_t compute_crc16(uint8_t *buf, uint16_t length);
static uint8_t crc_is_equal(uint16_t crc, uint8_t *received_crc);

static uint8_t process_frame(uint8_t *frame_buf, uint32_t frame_len, msg_array *messages);

static uint8_t message_send(msg *message);
static void message_post_in(uint8_t *frame, uint32_t len, BaseType_t *hpt);
static void message_discover(void);


static void task_message_ctlr(void *arg);


static uint8_t serialize_buf[DRIVERS_NANOPB_MESSAGES_PB_H_MAX_SIZE + CRC16_LEN];

static QueueHandle_t msg_q;
static StaticQueue_t _msg_q;
static uint8_t msg_q_buf[MAX_MSG_CNT_OUT * sizeof(frame_msg_t)];

static TaskHandle_t msg_task;
static StaticTask_t _msg_task;
static StackType_t msg_task_stk[MSG_CTLR_STACK_DEPTH];
/************
 * APIs
 ************/

/**
 * @brief Intialize data transfer sub-system
 * 
 * 
 * 
 */
void c_message_init(void) {
    register_msg_in_cb(message_post_in);
    rs485_init();

    msg_q = xQueueCreateStatic(MAX_MSG_CNT_OUT, sizeof(frame_msg_t), msg_q_buf, &_msg_q);
    msg_task = xTaskCreateStatic(task_message_ctlr, "Msg Tsk", MSG_CTLR_STACK_DEPTH,
                                    NULL, MSG_CTLR_PRIO, msg_task_stk, &_msg_task);
    
    if (msg_task == NULL) {
        while (1) {}
    }
}



/**
 * @brief Posts a message to the message task's out buf
 * 
 * @return 0 on success; else 1
 */
uint8_t c_message_post_out(msg *message) {
    static frame_msg_t temp;
    temp.which = 0;
    memcpy((void *)&temp.message,  (void *)message, sizeof(msg));
    return !xQueueSendToBack(msg_q, &temp, MSG_POST_OUT_TIMEOUT);
}


/***********************
 *  STATIC DEFS
 **********************/


/**
 * @brief This task is responsible for de/serializing and de/encoding messages
 * 
 * This task is signaled by the UART DMA peripheral central node
 * when messages are to be received/sent from/to peer nodes.
 * 
 */
static void task_message_ctlr(void *arg) {
    uint8_t ret = 0;
    frame_msg_t object;
    receive_begin();

    for (;;) {
        if (xQueueReceive(msg_q, (void *)&object, portMAX_DELAY) == pdTRUE) {
            switch (object.which)
            {
            case FRAME_IN:
                msg_array arr = msg_array_init_zero;
                ret = process_frame(object.frame.buf, object.frame.len, &arr);
                if (ret == 0 && arr.msgs_count > 1) {
                    // push message array to central node
                    for (uint8_t i = 0; i < arr.msgs_count - 1; ++i)
                        ret = central_post_msg(&arr.msgs[i]);
                }
                break;
            case MSG_OUT:
                message_send(&object.message);
                break;
            default:
                break;
            }

            UBaseType_t high_stk_usage = uxTaskGetStackHighWaterMark(NULL);
            memset((void *)&object, 0, sizeof(object));
        }
    }
}


/**
 * @brief Node discover sequence
 * 
 * This function implements the storage node
 * discovery sequence. The implementation utilizes
 * the 96-bit UID on each stm32f411xe mcu in a BT 
 * search. If collision is detected on the line ,e.g.
 * failed CRC check, the mask is extended by 1 bit
 * until no collision occurrs.
 * 
 */
static void message_discover(void) {
    
}


/**
 * @brief Sends a message to all peer nodes with node ID set
 * 
 * Messages sent from central node are broadcast to all peer nodes.
 * Peer nodes compare the received NODE ID against their own and
 * respond accordingly.
 * 
 * @param[in] message message to send
 */
static uint8_t message_send(msg *message) {
    uint32_t len = 0;
    memset((void *)serialize_buf, 0, sizeof(serialize_buf));
    uint8_t status = serialize_struct(message, &len);

    if (len > MAX_FRAME_LEN)
        printf("ERROR: SERIAL BUF LEN GREATER THAN MAX ENCODED BUFFER LENGTH\r\n");

    if (status == 1) {
        status = rs485_transmit(serialize_buf, len);
    }
    return status;
}



/**
 * @brief Receives a message array from peer node
 * 
 * 
 * @param[out] message array of messages sent by peer node
 * 
 * @return 0 on successful reception; else 1
 */
static uint8_t process_frame(uint8_t *frame_buf, uint32_t frame_len, msg_array *messages) {
    uint8_t status = 0;
    uint8_t tmp[DRIVERS_NANOPB_MESSAGES_PB_H_MAX_SIZE + CRC16_LEN];
    frame_len = rs485_cobs_decode(frame_buf, frame_len, tmp);

    if (frame_len > 1) {
        uint16_t crc_check = compute_crc16(tmp, frame_len - CRC16_LEN);
        if (crc_is_equal(crc_check, &tmp[frame_len - CRC16_LEN])) {
            status = deserialize_msg_buf(tmp, frame_len - CRC16_LEN, messages);
            if (status)
                printf("FAILED TO DESERIALIZE MSG: %s\r\n", tmp);
        }
        else {
            printf("ERROR: CRC VALUES DO NOT MATCH\r\n");
            status = 1;
        }
    } else {
        printf("ERROR: FAILED TO DECODE MESSAGE\n\r");
        status = 1;
    }
 
    return status;
}



static uint8_t crc_is_equal(uint16_t crc, uint8_t *received_crc) {
    return (crc == *((uint16_t *)received_crc));
}

static uint16_t compute_crc16(uint8_t *buf, uint16_t length) {
    uint16_t crc = 0xFFFF;
    uint16_t polynomial = 0xA001;
    uint16_t i, j;

    for (i = 0; i < length; ++i) {
        crc ^= buf[i];
        for (j = 0; j < 8; ++j) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ polynomial;
            else
                crc >>= 1;
        }
    }

    return crc;
}



static uint8_t deserialize_msg_buf(uint8_t *serial_buf, uint32_t length, msg_array *message) {
    pb_istream_t stream_in = pb_istream_from_buffer(serial_buf, length);
    uint8_t status = pb_decode(&stream_in, &msg_array_msg, (void *)message);
    return !status;
}



static uint8_t serialize_struct(msg* message, uint32_t *len) {
    uint8_t status = 0;
    // create stream
    pb_ostream_t stream_out;

    status = pb_get_encoded_size((size_t *)len, &msg_msg, (void *)message);

    if (status) {
        // create stream
        stream_out = pb_ostream_from_buffer((pb_byte_t *)serialize_buf, (size_t)(*len + CRC16_LEN));
        status = pb_encode(&stream_out, &msg_msg, (void *)message);

        if (status) {
            uint16_t crc = compute_crc16(serialize_buf, *len);
            // append crc
            status = pb_write(&stream_out, (const pb_byte_t*)&crc, (size_t)sizeof(uint16_t));
            *len = stream_out.bytes_written;
        }
    }
    return status;
}



/**
 * @brief Posts a message to the message task's in buf
 * 
 * 
 */
static void message_post_in(uint8_t *frame, uint32_t len, BaseType_t *hpt) {
    frame_msg_t temp;
    temp.which = FRAME_IN;
    memcpy((void *)temp.frame.buf, (void *)frame, sizeof(temp.frame.buf));
    temp.frame.len = len;
    xQueueSendToBackFromISR(msg_q, &temp, hpt);
}


