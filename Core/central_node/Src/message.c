#include "message.h"
#include "rs485_cobs.h"
#include "common/ring_buffer.h"
#include "common/defines.h"
#include "../../../Drivers/nanopb/pb_encode.h"
#include "../../../Drivers/nanopb/pb_decode.h"
#include <stdio.h>

#define CRC16_LEN  (2U)
#define NUM_OVERHEAD_BYTES (2U)
#define MAX_COBS_OVERHEAD ((DRIVERS_NANOPB_MESSAGES_PB_H_MAX_SIZE >> 8) + NUM_OVERHEAD_BYTES)
#define MAX_FRAME_LEN (DRIVERS_NANOPB_MESSAGES_PB_H_MAX_SIZE + MAX_COBS_OVERHEAD + CRC16_LEN)
#define MAX_MSG_CNT (15U)
#define MAX_MSG_PER_CTS (5U)

/************************
 * STATIC DECLARATIONS
 ***********************/
static uint16_t compute_crc16(uint8_t *buf, uint16_t length);
static uint8_t serialize_struct(msg_array* message, uint16_t *length);
static uint8_t deserialize_msg_buf(uint8_t *serial_buf, uint16_t length, msg *message);
static uint8_t crc_is_equal(uint16_t crc, uint8_t *received_crc);

static void rs485_reception_cb(void);
static void rs485_msg_consumed_cb(void);


static uint8_t serialize_buf[DRIVERS_NANOPB_MESSAGES_PB_H_MAX_SIZE + CRC16_LEN];
static volatile uint8_t msg_cnt = 0;
static void(*peer_rx_cplt)(void);
STATIC_RING_BUFFER(msg_queue, MAX_MSG_CNT, msg);
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
void message_send(msg *message) {
    uint16_t len = 0;
    memset((void *)serialize_buf, 0, sizeof(serialize_buf));
    uint8_t status = serialize_struct(&message, &len);

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
uint8_t message_receive(msg_array *message) {
    uint8_t rx_frame[MAX_FRAME_LEN] = {0};
    uint32_t length = 0;

    uint8_t status = rs485_receive(rx_frame, &length);
    if (status == 0) {
        uint16_t crc_check = compute_crc16(rx_frame, length - CRC16_LEN);
        if (crc_is_equal(crc_check, &rx_frame[length - CRC16_LEN])) {
            status = deserialize_msg_buf(rx_frame, length - CRC16_LEN, message);
            if (status)
                printf("FAILED TO DESERIALIZE MSG: %s\r\n", rx_frame);
        }
        else {
            printf("ERROR: CRC VALUES DO NOT MATCH\r\n");
            status = 1;
        }
    }
    else {
        printf("MSG RECEIVE FAILED\r\n");
    }

    return status;
}



/**
 * @brief Checks for any pending peer node messages
 * 
 * 
 * 
 * @return 1 if peer node messages are available; else 0
 */
uint8_t message_available(void) {
    __disable_irq();
    uint8_t is_avail = msg_cnt > 0;
    __enable_irq();
    return is_avail;
}


/**
 * @brief Registers the callback function for peer node RX cplt
 * 
 * 
 * 
 */
void register_peer_rx_cplt_cb(void(*cb)(void)) {
    peer_rx_cplt = cb;
}

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


static uint8_t deserialize_msg_buf(uint8_t *serial_buf, uint16_t length, msg *message) {
    pb_istream_t stream_in = pb_istream_from_buffer(serial_buf, length);
    uint8_t status = pb_decode(&stream_in, &msg_msg, (void *)message);
    return !status;
}

static uint8_t serialize_struct(msg_array* message, uint16_t *len) {
    uint8_t status = 0;
    // create stream
    pb_ostream_t stream_out;

    status = pb_get_encoded_size((size_t *)len, &msg_msg, (void *)message);

    if (status) {
        // create stream
        stream_out = pb_ostream_from_buffer((pb_byte_t *)serialize_buf, (size_t)*len + CRC16_LEN);
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


static void rs485_reception_cb(void) {
    if (msg_cnt < MAX_MSG_CNT)
        msg_cnt++;
}

static void rs485_msg_consumed_cb(void) {
    if (msg_cnt > 0)
        msg_cnt--;
}
