#include "message.h"
#include "rs485_cobs.h"
#include "common/ring_buffer.h"
#include "common/defines.h"
#include "app/display.h"
#include "../../../Drivers/nanopb/pb_encode.h"
#include "../../../Drivers/nanopb/pb_decode.h"
#include "../../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_flash.h"
#include <stdio.h>

/***********************
 * STRUCTS/MACROS
 *********************/
#define CRC16_LEN  (2U)
#define NUM_OVERHEAD_BYTES (2U)
#define MAX_COBS_OVERHEAD ((DRIVERS_NANOPB_MESSAGES_PB_H_MAX_SIZE >> 8) + NUM_OVERHEAD_BYTES)
#define MAX_FRAME_LEN (DRIVERS_NANOPB_MESSAGES_PB_H_MAX_SIZE + MAX_COBS_OVERHEAD + CRC16_LEN)
#define MAX_MSG_CNT (15U)
#define MAX_MSG_PER_CTS (5U)

#define RESET_NODE_ID       (0xDEADBEEF)
#define NODE_ID_FLASH_ADDR  (0x0807FFFF)
#define GET_NODE_ID_FROM_FLASH() (*((volatile uint8_t *)NODE_ID_FLASH_ADDR))
#define DISC_BTN_DEBOUNCE   (250U)
#define DISC_TIMEOUT        (750U)

enum message_flags {
    FLAG_MSGS_AVAIL,
    FLAG_DISC_START,
    FLAG_DISC_ONGOING,
    FLAG_DISC_OK,
    FLAG_DISC_FAILED,
    FLAG_ALL = 0xFFFFFFFFU
};

struct message_cnf {
    volatile uint32_t node_id;
    volatile uint32_t node_uid;
    volatile uint8_t msg_cnt;
    
    volatile uint32_t flags;
};

/************************
 * STATIC DECLARATIONS
 ***********************/
static uint16_t compute_crc16(uint8_t *buf, uint16_t length);
static uint8_t serialize_struct(msg_array* message, uint32_t *length);
static uint8_t deserialize_msg_buf(uint8_t *serial_buf, uint32_t length, msg *message);
static uint8_t crc_is_equal(uint16_t crc, uint8_t *received_crc);
static uint32_t message_register_node_id(uint8_t node_id);
static void message_button_discover_cb(void);

static void rs485_reception_cb(void);
static void rs485_msg_consumed_cb(void);

static uint8_t process_message(msg *message);
static uint8_t message_flush(void);

static uint8_t serialize_buf[DRIVERS_NANOPB_MESSAGES_PB_H_MAX_SIZE + CRC16_LEN];
STATIC_RING_BUFFER(msg_queue, MAX_MSG_CNT, msg);
static struct message_cnf msg_cnf = {
    .flags = 0x0,
    .msg_cnt = 0,
    .node_id = RESET_NODE_ID,
    .node_uid = 0
};
/************
 * APIs
 ************/

/**
 * @brief Intialize data transfer sub-system
 * 
 * 
 * 
 */
void message_init(void) {
    rs485_init();
    register_msg_ready_cb(rs485_reception_cb);
    register_msg_consumed_cb(rs485_msg_consumed_cb);

    msg_cnf.node_id = GET_NODE_ID_FROM_FLASH();
    if (msg_cnf.node_id == 0xFF) msg_cnf.node_id = RESET_NODE_ID;
}


void message_task_process(void) {
    static uint32_t last_tick = 0;
    uint32_t curr_tick = HAL_GetTick();

    if (!msg_cnf.flags) return;

    if (msg_cnf.flags & FLAG_DISC_START) {
        uint32_t device_uid[3] = {0};
        device_uid[0] = HAL_GetUIDw0();
        device_uid[1] = HAL_GetUIDw1();
        device_uid[2] = HAL_GetUIDw2();
        uint32_t hased_uid = HASH_VALUE((const void *)device_uid, sizeof(uint32_t) * 3);
        msg disc_msg = {.command = MSG_CMD_DISCOVER, .has_node_uid = true, .node_id = 0xff, .node_uid = hased_uid};
        message_send(&disc_msg);

        msg_cnf.flags &= ~(FLAG_DISC_START);
        msg_cnf.flags |= (FLAG_DISC_ONGOING);
        last_tick = curr_tick;
    }
    if (msg_cnf.flags & FLAG_DISC_ONGOING) {
        if (curr_tick - last_tick >= DISC_TIMEOUT)
            msg_cnf.flags |= FLAG_DISC_FAILED;
    }
    if (msg_cnf.flags & FLAG_DISC_FAILED) { // retry until node is discovered
        msg_cnf.flags |= (FLAG_DISC_START);
        msg_cnf.flags &= ~(FLAG_DISC_ONGOING);
    }
    if (msg_cnf.flags & FLAG_DISC_OK) {
        msg_cnf.flags &= ~(FLAG_DISC_ONGOING);
        display_refresh_value(LABEL_NODE_ID, (uint16_t)msg_cnf.node_id);
    }

    if (msg_cnf.flags & FLAG_MSGS_AVAIL) {
        msg message = msg_init_default;
        message_receive(&message);
    }
}


/**
 * @brief Post message to peer node message queue
 * 
 * Posted messaged are periodically flushed to the central
 * node in a msg_array protobuf when the peer node 
 * receives a CTS message from the central node.
 * 
 * @param[in] message message to post
 */
int32_t message_send(msg *message) {
    int32_t ret = STATUS_OK;
    ring_buffer_push(&msg_queue, (void *)message);
    return ret;
}



/**
 * @brief Retrieves message from central MCU and processes accordingly
 * 
 * Deserializes a message from the central node if any are present.
 * Compares the node ID of message receive against that of the peer node
 * and processes the message on match, or discards the message on no match
 * 
 * @param[out] message
 * 
 * 
 * @return 0 on successful processing and reception; else 1
 */
uint8_t message_receive(msg *message) {
    uint8_t rx_frame[MAX_FRAME_LEN] = {0};
    uint32_t length = 0;

    uint8_t status = rs485_receive(rx_frame, &length);
    if (status == STATUS_OK && length > 0) {
        uint16_t crc_check = compute_crc16(rx_frame, length - CRC16_LEN);
        if (crc_is_equal(crc_check, &rx_frame[length - CRC16_LEN])) {
            status = deserialize_msg_buf(rx_frame, length - CRC16_LEN, message);
            if (status)
                printf("FAILED TO DESERIALIZE MSG: %s\r\n", rx_frame);
        }
        else {
            printf("ERROR: CRC VALUES DO NOT MATCH\r\n");
            status = STATUS_ERR;
        }
    }
    else {
        printf("MSG RECEIVE FAILED\r\n");
    }

    if (status == STATUS_OK)
        status = process_message(message);

    return status;
}


// /**
//  * @brief Checks for any pending peer node messages
//  * 
//  * 
//  * 
//  * @return 1 if peer node messages are available; else 0
//  */
// uint8_t message_available(void) {
//     __disable_irq();
//     uint8_t is_avail = msg_cnf.msg_cnt > 0;
//     __enable_irq();
//     return is_avail;
// }



/***********************
 *  STATIC DEFS
 **********************/

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


static uint8_t deserialize_msg_buf(uint8_t *serial_buf, uint32_t length, msg *message) {
    pb_istream_t stream_in = pb_istream_from_buffer(serial_buf, length);
    uint8_t status = pb_decode(&stream_in, &msg_msg, (void *)message);
    return !status;
}

// NOTE: ALWAYS MAKE SURE YOU CAST CORRECTLY... fixed bug where casted uint16_t * to size_t *
// get_enc_size writes 4 bytes to the len pointer which only owns 2 bytes of space since
// len is right above message on the stack we overflow and corrupt the msg_array struct...
static uint8_t serialize_struct(msg_array* message, uint32_t *len) { 
    uint8_t status = 0;
    // create stream
    pb_ostream_t stream_out;

    status = pb_get_encoded_size((size_t *)len, &msg_array_msg, (const void *)message);
    if (status) {
        // create stream
        stream_out = pb_ostream_from_buffer((pb_byte_t *)serialize_buf, (size_t)*len + CRC16_LEN);
        status = pb_encode(&stream_out, &msg_array_msg, (const void *)message);

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
 * @brief Compares NODE ID and processes/discards message
 * 
 * This function compares the NODE ID of the received message
 * against the ID of the receiving node. Discards the message 
 * if the node IDs do not match OR the received message is 
 * NOT a command. Otherwise processes the command, e.g. remote 
 * disarm/CTS
 * 
 * @param[in] message
 * 
 * @return 0 if message successfully processed; 1 if message
 * was discarded or error in processing 
 */
static uint8_t process_message(msg *message) {
    uint8_t status = STATUS_ERR;

    switch (message->command) {
    case MSG_CMD_NONE:
        break;
    case MSG_CMD_CTS:
        if (message->node_id == msg_cnf.node_id)
            status = message_flush();
        break;
    case MSG_CMD_DISCOVER_OK:
        if (message->has_node_uid) {
            if ((msg_cnf.node_id == RESET_NODE_ID) && (message->node_uid == msg_cnf.node_uid))
                message_register_node_id(message->node_id); // node id is technically used for messages primarily
        }
        msg_cnf.flags |= FLAG_DISC_OK;
        break;
    case MSG_CMD_DISCOVER_ERR:
        msg_cnf.flags |= FLAG_DISC_FAILED;
        break;
    case MSG_CMD_DISCOVER:
    case MSG_CMD_SEND_CPLT:
    default:
        break;
    }

    return status;
}



/**
 * @brief Sends a msg_array protobuf to the central node
 * 
 * Central node polls each peer node, sending a CTS message to 
 * the node currently being polled. When a peer node receives 
 * a CTS this function should be called to send any messages
 * buffered by message_send(). With a final done_sending message
 * appended. 
 * 
 * 
 * @note The done_sending message should be sent regardless of if 
 * the peer node has any messages buffered at the moment
 * 
 * @return 0 on success; 1 else
 */
static uint8_t message_flush(void) {
    msg_array arr = msg_array_init_default;
    uint8_t i = 0;

    for (i = 0; i < MAX_MSG_PER_CTS - 1; ++i) {
        if (ring_buffer_empty(&msg_queue))
            break;

        ring_buffer_pop(&msg_queue, (void *)&arr.msgs[i]);
        arr.msgs_count++;
    }

    arr.msgs[i].node_id = GET_NODE_ID_FROM_FLASH();
    arr.msgs[i].command = MSG_CMD_SEND_CPLT;
    arr.msgs_count++;

    uint32_t len = 0;
    memset((void *)serialize_buf, 0, sizeof(serialize_buf));
    uint8_t status = serialize_struct(&arr, &len);

    if (len > MAX_FRAME_LEN)
        printf("ERROR: SERIAL BUF LEN GREATER THAN MAX ENCODED BUFFER LENGTH\r\n");

    if (status == 1) {
        status = rs485_transmit(serialize_buf, len);
        if (status == STATUS_OK)
            printf("FLUSHING %d MESSAGES FROM PEER\n\r", arr.msgs_count);
    }
    return status;
}

static void message_button_discover_cb(void) {
    static uint32_t prev_tick = 0;
    uint32_t curr_tick = HAL_GetTick();
    if ((curr_tick - prev_tick) < DISC_BTN_DEBOUNCE) return;
    msg_cnf.flags |= FLAG_DISC_START;
    prev_tick = curr_tick;
}

static uint32_t message_register_node_id(uint8_t node_id) {
    uint32_t ret = HAL_FLASH_Unlock();
    if (ret) goto cleanup;
    ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, NODE_ID_FLASH_ADDR, node_id);
    HAL_FLASH_Lock();
cleanup:
    msg_cnf.node_id = node_id;
    return ret;
}


static void rs485_reception_cb(void) {
    if (msg_cnf.msg_cnt < MAX_MSG_CNT) {
        msg_cnf.msg_cnt++;
        msg_cnf.flags |= FLAG_MSGS_AVAIL;
    }
}

static void rs485_msg_consumed_cb(void) {
    if (msg_cnf.msg_cnt > 0)
        msg_cnf.msg_cnt--;
    if (msg_cnf.msg_cnt == 0)
        msg_cnf.flags &= ~(FLAG_MSGS_AVAIL);
}
