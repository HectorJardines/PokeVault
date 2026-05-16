/**
 * The ADC module is shared among two sensors, ir receivers
 * for line break detection and motion sensors for presence
 * detection. Implements a circular ADC scanning all 5 of the sensors 
 * and trasnferring the data to one of two always available 
 * data buffers using the DMA peripheral's double buffering mode.
 */

#ifndef _ADC_H
#define _ADC_H

#include "stm32f4xx.h"
#include "../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_adc.h"
#include "../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_dma.h"

#define NO_OF_IR_RCVRS          (3U)
#define NO_OF_MOTION_SENSORS    (2U)
#define ADC1_NUMBER_OF_SENSORS  (5U) // 3 IR RECEIVERS + 2 INFRARED MOTION DETECTION

/**************
 * ENUMS
 **********/
typedef enum {
    IR_RECEIVER,
    MOTION_SENSOR
} adc_device_e;

typedef enum {
    DMA_BUF1_RDY,
    DMA_BUF2_RDY,
    DMA_DATA_NOT_RDY
} data_ready_e;

typedef enum {
    ADC_OK,
    ADC_BSY,
    ADC_ERR
} adc_status_e;

/**************
 * PUBLIC APIs
 *************/
/**
 * @brief Initializes the ADC1 peripheral for continuous sampling of sensors
 * 
 * The system consists of 5 sesnsors, 3 IR receivers + 2 Infrared motion sensors.
 * This ADC1 peripheral will be configured for continuous conversion and sampling of 
 * these five sensors.
 */
void adc_init(void);

/**
 * @brief Returns the ADC samples of the specified device
 * 
 * Since there is only 1 ADC peripheral the DMA will transfer 
 * all samples to the same buffer, this API will read and retrieve
 * values from said buffer according to the specified device.
 * 
 * @param samples data buffer to store samples into
 * @param dev device type
 */
uint8_t adc_read_samples(uint16_t *samples, adc_device_e dev);

#endif