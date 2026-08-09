#include "../../Inc/drivers/rs485_cobs.h"
#include "../../Inc/common/defines.h"
#include "../../Inc/common/ring_buffer.h"
#include <stdio.h>


#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"


#define USARTx USART1
#define USART_BAUD_RATE (115200)
#define COBS_DELIM  (0x00U)
#define COBS_TERMINATED (0U)
#define COBS_NOT_TERM   (-1)

#define MAX_MESSAGES (5U)

#define TX_OK       (0U)
#define TX_ERR      (1U)
#define RX_OK       (TX_OK)
#define RX_ERR      (TX_ERR)

/*************************
 * STATIC DECLARATIONS
 *************************/
static void dma_init(void);
static void usart_init(void);
static uint8_t transmit_begin(void);

static uint16_t rs485_cobs_encode(void *data, uint32_t length, uint8_t *encoded_buf);

// STATIC TX BUFFER INFO
STATIC_RING_BUFFER(frame_rb, MAX_MESSAGES, frame_info_t);
static frame_info_t active_tx_buf = {0};
static volatile uint8_t tx_ongoing = FALSE;

// STATIC RECEIVE BUFFER INFO
// STATIC_RING_BUFFER(rx_frame_rb, MAX_MESSAGES, frame_info_t);
static uint16_t active_rx_idx = 0;
static frame_info_t active_rx_buf = {0};
static uint8_t rx_buf[MAX_FRAME_LEN] = {0};


// USART1 HANDLERS
UART_HandleTypeDef huart1 = {0};
DMA_HandleTypeDef dma_tx = {0};
DMA_HandleTypeDef dma_rx = {0};

// MESSAGE NOTIFICATION CALLBACKS
static void(*msg_in_cb)(uint8_t *frame, uint32_t len, uint32_t *hpt) = NULL;
static void(*msg_consumed_cb)(void) = NULL;
/**************
 *   APIs
 **************/

/**
  * @brief Intialize underlying USART peripheral that handles data tx/rx
  * 
  * 
  * 
  */
uint8_t rs485_init(void) {
    usart_init();
    dma_init();
    // receive_begin();
    return 0;
}


uint8_t rs485_transmit(uint8_t *data, uint32_t length) {
    uint8_t status = TX_ERR;
    taskENTER_CRITICAL();
    if (!ring_buffer_full(&frame_rb)) {
        frame_info_t tmp_buf = {0, {0}};
        tmp_buf.len = rs485_cobs_encode(data, length, tmp_buf.buf);
        ring_buffer_push(&frame_rb, (void *)&tmp_buf);
        status = TX_OK;
        if (!tx_ongoing)
            status = transmit_begin();
    }
    taskEXIT_CRITICAL();

    return status;
}

/**
 * @brief Begins circular reception of bytes over RS485
 * 
 * 
 * 
 */
void receive_begin(void) {
    memset((void *)rx_buf, 0, sizeof(rx_buf));
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buf, MAX_FRAME_LEN);
}


// uint8_t rs485_receive(uint8_t *encoded_buf, uint32_t *length) {
//     uint8_t status = RX_OK;
//     __disable_irq();
//     if (!ring_buffer_empty(&rx_frame_rb)) {
//         serial_t tmp_buf = {0};

//         ring_buffer_pop(&rx_frame_rb, (void *)&tmp_buf);
//         __enable_irq();
//         *length = rs485_cobs_decode(tmp_buf.buf, tmp_buf.len, (void *)encoded_buf);
//         if (*length <= 1)
//             status = RX_ERR;
//         msg_consumed_cb();
//     }
//     __enable_irq();

//     return status;
// }



/**
 * @brief Decode COBS encoded buffer into arbitrary data pointer
 * 
 * 
 * 
 * @param encoded_buf
 * @param length
 * @param data
 */
uint16_t rs485_cobs_decode(uint8_t *encoded_buf, uint32_t length, void *data) {
    // check first 
    uint8_t *decode_data = (uint8_t *) data;
    uint8_t bytes_to_next_group = 0xFF; // 0xFF is COBS delimiter

    for (uint8_t *byte = encoded_buf, group_len = 0; byte < encoded_buf + length; --group_len) {
        if (group_len > 0)
            *decode_data++ = *byte++;
        else {
            group_len = *byte++;
            if (group_len && (bytes_to_next_group != 0xFF)) // bytes_to_next should not be delimiter if writing 0x00
                *decode_data++ = 0x00;
            bytes_to_next_group = group_len;
            if (!bytes_to_next_group)
                break;
        }
    }

    return decode_data - (uint8_t *)data;
}



void register_msg_in_cb(void(*in_cb)(uint8_t *frame, uint32_t len, uint32_t *hpt)) {
    msg_in_cb = in_cb;
}


// void register_msg_consumed_cb(void(*consumed_cb)(void)) {
//     msg_consumed_cb = consumed_cb;
// }

/***************
 * STATIC DEFS
 ***************/

static uint8_t transmit_begin(void) {
    ring_buffer_pop(&frame_rb, (void *)&active_tx_buf);
    uint8_t status = HAL_UART_Transmit_DMA(&huart1, active_tx_buf.buf, active_tx_buf.len);
    __HAL_DMA_DISABLE_IT(huart1.hdmatx, DMA_IT_HT);
    if (status == TX_OK)
        tx_ongoing = TRUE;
    return status;
}



static void dma_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    /* USART TX DMA CONFIG */
    dma_tx.Instance = DMA2_Stream7;
    dma_tx.Parent = &huart1;

    dma_tx.Init.Channel = DMA_CHANNEL_4;
    dma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    dma_tx.Init.Mode = DMA_NORMAL;
    dma_tx.Init.MemInc = DMA_MINC_ENABLE;
    dma_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    dma_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    dma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;

    /* USART RX DMA CONFIG */
    dma_rx.Instance = DMA2_Stream5;
    dma_rx.Parent = &huart1;

    dma_rx.Init.Channel = DMA_CHANNEL_4;
    dma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    dma_rx.Init.Mode = DMA_CIRCULAR;
    dma_rx.Init.MemInc = DMA_MINC_ENABLE;
    dma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    dma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    dma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;

    HAL_DMA_Init(&dma_tx);
    HAL_DMA_Init(&dma_rx);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream5_IRQn);
    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 5, 5);
    HAL_NVIC_SetPriority(DMA2_Stream5_IRQn, 5, 5);
}

static void usart_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    huart1.Instance = USART1;
    huart1.hdmatx = &dma_tx;
    huart1.hdmarx = &dma_rx;

    huart1.Init.BaudRate = USART_BAUD_RATE;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.OverSampling = UART_OVERSAMPLING_8;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;

    HAL_UART_Init(&huart1);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 5);
}


/**
 * @brief Encode data buffer using Consistent Overhead Byte Stuffing
 * 
 * 
 * 
 * @param encoded_buf
 * @param length
 * @param data
 */
static uint16_t rs485_cobs_encode(void *data, uint32_t length, uint8_t *encoded_buf) {
    // [0x11, 0x22, 0x33, 0x44]

    // [ , 0x11, 0x22, 0x33, 0x44]
    uint8_t *encode_data = encoded_buf;
    uint8_t *group_header = encode_data++;
    uint8_t bytes_to_next_group = 1;

    for (uint8_t *byte = (uint8_t *)data; length > 0; --length, byte++) {
        if (*byte) { // 0x11
            *encode_data++ = *byte;
            bytes_to_next_group++;
        }

        if (*byte == 0x00 || bytes_to_next_group == 0xFF) {
            *group_header = bytes_to_next_group;
            bytes_to_next_group = 1;
            group_header = encode_data;
            
            if (!*byte || length > 0)
                encode_data++;
        }
    }
    *group_header = bytes_to_next_group;

    return (encode_data - encoded_buf) + 1; // + 1 for delimiter byte
}



static int8_t process_bytes(uint8_t *frame_buf, uint16_t len, uint16_t *prev_buf_pos) {
    int8_t frame_terminated = -1;
    
    uint16_t bytes_consumed = 0;
    for (uint16_t i = 0; i < len; ++i) {
        active_rx_buf.buf[active_rx_idx++] = frame_buf[i];
        active_rx_buf.len++;
        bytes_consumed++;

        if (frame_buf[i] == 0x00) {
            frame_terminated = 0;
            active_rx_idx = 0;
            break;
        }
    }
    *prev_buf_pos = (*prev_buf_pos + bytes_consumed) % MAX_FRAME_LEN;
    return frame_terminated;
}


void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USARTx) {
        if (!ring_buffer_empty(&frame_rb)) {
            ring_buffer_pop(&frame_rb, (void *)&active_tx_buf);
            HAL_UART_Transmit_DMA(&huart1, active_tx_buf.buf, active_tx_buf.len);
            __HAL_DMA_DISABLE_IT(huart1.hdmatx, DMA_IT_HT);
        }
        else {
            tx_ongoing = FALSE;
        }
    }
}


void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    static uint16_t prev_buf_pos = 0;
    uint16_t curr_buf_pos = Size;
    uint8_t frame_status = COBS_NOT_TERM;
    if (huart->Instance == USARTx) {
        if (curr_buf_pos != prev_buf_pos) {
            if (curr_buf_pos > prev_buf_pos) {
                uint16_t num_bytes = curr_buf_pos - prev_buf_pos;
                frame_status = process_bytes(&rx_buf[prev_buf_pos], num_bytes, &prev_buf_pos);
            }
            else {
                frame_status = process_bytes(&rx_buf[prev_buf_pos], MAX_FRAME_LEN - prev_buf_pos, &prev_buf_pos);
                if (frame_status != COBS_TERMINATED)
                    frame_status = process_bytes(&rx_buf[0], curr_buf_pos, &prev_buf_pos);
            }
        }

        if (frame_status == COBS_TERMINATED) {
            uint32_t hpt = pdFALSE;
            msg_in_cb(&active_rx_buf.buf, (uint32_t)active_rx_buf.len, &hpt);
            memset((void *)&active_rx_buf, 0, sizeof(active_rx_buf));
            portYIELD_FROM_ISR(hpt);
        }
    }
}

void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);
}

void DMA2_Stream5_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_rx);
}

void DMA2_Stream7_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_tx);
}

