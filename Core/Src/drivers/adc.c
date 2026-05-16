#include "../Inc/drivers/adc.h"
#include "../Inc/common/log.h"

#define NO_OF_SENSOR_SAMPLES    (15U) // 3 samples of each sensor, for false flags

/********************
 * STATIC DEFS
 *****************/
static uint16_t cached_sensor_samples[NO_OF_SENSOR_SAMPLES];
static uint16_t dma_buffer_1[NO_OF_SENSOR_SAMPLES];
static uint16_t dma_buffer_2[NO_OF_SENSOR_SAMPLES];
static volatile data_ready_e data_ready_flag = DMA_DATA_NOT_RDY;
static volatile uint8_t tx_err_flag = 0;

 
static ADC_HandleTypeDef h_adc1;
static DMA_HandleTypeDef h_dma2;

static uint8_t adc_configure(void);
static void adc_begin_conversion(void);
static uint8_t dma_configure(void);

static void dma2_stream0_TC_callback(DMA_HandleTypeDef *dma);
static void dma2_stream0_M1_cplt_callback(DMA_HandleTypeDef *dma);
static void dma2_stream0_TE_callaback(DMA_HandleTypeDef *dma);

/******************
 * PUBLIC APIs
 ******************/

static uint8_t initialized = 0;
void adc_init(void) {
    if (initialized)
        log_warn((uint8_t *)"Peripheral already enabled by other module\n", -1);

    uint8_t status = adc_configure();
    if (status) {
        log_error((uint8_t *)"ADC Peripheral failed to initialize: ", status);
        return;
    }
    status = dma_configure();
    if (status) {
        log_error((uint8_t *)"DMA Peripheral failed to initialize: ", status);
        return;
    }
    adc_begin_conversion();
    initialized = 1;
}

/**
 * @brief Retrieve ADC samples from the DMA buffers
 * 
 * Retrieving the samples from the DMA buffers is considered
 * critical code. I.e. the buffers are shared resources with the 
 * DMA peripheral, we need to disable DMA interrupts when we 
 * go to access either of the buffer to ensure that the contents are 
 * not modified while we read them.
 * 
 */
uint8_t adc_read_samples(uint16_t *samples, adc_device_e dev) {
    if (tx_err_flag)
        return ADC_ERR;
    if (data_ready_flag == DMA_DATA_NOT_RDY)
        return ADC_BSY;

    /** critical sections **/
    if (data_ready_flag == DMA_BUF1_RDY) {
        HAL_NVIC_DisableIRQ(DMA2_Stream0_IRQn);
        for (uint8_t i = 0; i < NO_OF_SENSOR_SAMPLES; ++i)
            cached_sensor_samples[i] = *(dma_buffer_1 + i);
        HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    }
    else if (data_ready_flag == DMA_BUF2_RDY) {
        HAL_NVIC_DisableIRQ(DMA2_Stream0_IRQn);
        for (uint8_t i = 0; i < NO_OF_SENSOR_SAMPLES; ++i)
            cached_sensor_samples[i] = *(dma_buffer_2 + i);
        HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    }

    if (dev == IR_RECEIVER) {
        for (uint8_t i = 0; i < NO_OF_SENSOR_SAMPLES;) { // skip sample 3,4,8,9,13,14
            // [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]
            if (i == 3 || i == 8 || i == 13) {
                i += 2; // skips all motion sensor samples
                continue;
            }
            samples[i] = *(cached_sensor_samples + i);
            ++i;
        }
    }
    else if (dev == MOTION_SENSOR) {
        for (uint8_t i = 0; i < NO_OF_SENSOR_SAMPLES;) { // take samples 3,4,8,9,13,14
            if (i == 0 || i == 5 || i == 10) {
                i += 3; // skips all IR_RECEIVER samples
                continue;
            }
            samples[i] = *(cached_sensor_samples + i);
            ++i;
        }
    }
    return ADC_OK;
}


/**********************
 * STATIC DECLARATIONS
 ***********************/

/**
 * @brief configure the DMA2_Stream0 in double buffer mode
 * 
 * 
 */
static uint8_t dma_configure(void) {
    // DMA instance configs
    h_dma2.Instance = DMA2_Stream0;
    h_dma2.StreamBaseAddress = DMA2_Stream0_BASE;

    // DMA memory configurations
    h_dma2.Init.Direction = DMA_PERIPH_TO_MEMORY;
    h_dma2.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    h_dma2.Init.MemInc = DMA_MINC_ENABLE;
    h_dma2.Init.PeriphInc = DMA_PINC_DISABLE;
    h_dma2.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    h_dma2.Init.MemBurst = DMA_MBURST_SINGLE;

    h_dma2.Init.Mode = DMA_NORMAL;
    h_dma2.Init.FIFOMode = DISABLE;
    h_dma2.Init.Channel = DMA_CHANNEL_0;

    return HAL_DMA_Init(&h_dma2);
}


/**
 * @brief configures the ADC1 peripheral
 * 
 * We configure the ADC1 peripheral to support continuous
 * scanning of channels (we need to constantly check the values
 * of multiple sensors time sensitive).
 * 
 */
static uint8_t adc_configure(void) {
    h_adc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    // continuous conversion scan mode
    h_adc1.Init.ContinuousConvMode = ENABLE;
    h_adc1.Init.ScanConvMode = ENABLE;
    // DMA requests for efficient sampling
    h_adc1.Init.DMAContinuousRequests = ENABLE;
    // config 12 bit resolution
    h_adc1.Init.Resolution = ADC_RESOLUTION12b;
    h_adc1.Init.NbrOfConversion = ADC1_NUMBER_OF_SENSORS;
    // software starts conversion
    h_adc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    // 36MHz adc clock speed
    h_adc1.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
    h_adc1.Init.DiscontinuousConvMode = DISABLE;
    h_adc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    h_adc1.Instance = ADC1;

    return HAL_ADC_Init(&h_adc1);
}

static void adc_begin_conversion(void) {
    HAL_DMAEx_MultiBufferStart_IT(&h_dma2, (uint32_t)&ADC1->DR, (uint32_t)dma_buffer_1, (uint32_t) dma_buffer_2, NO_OF_SENSOR_SAMPLES);
    HAL_ADC_Start(&h_adc1);
}

/***
 * Interrupt Handlers and Callback functions
 */
void DMA2_Stream0_IRQHandler(void) {
    // transfer complete interrupt 
    if (DMA2->LISR & DMA_LISR_TCIF0_Msk) {
        if (DMA2_Stream0->CR & DMA_SxCR_CT_Msk) {// current buffer is M1AR, TC triggered by M0AR 
            //
            DMA2->LIFCR |= DMA_LIFCR_CTCIF0;
            dma2_stream0_TC_callback(&h_dma2);
        }
        else if (!(DMA2_Stream0->CR & DMA_SxCR_CT)) { // current buffer is M0AR, TC trigg by M1AR
            // 
            DMA2->LIFCR |= DMA_LIFCR_CTCIF0;
            dma2_stream0_M1_cplt_callback(&h_dma2);
        }
    }
    // transfer error interrupt
    if (DMA2->LIFCR & DMA_LISR_TEIF0_Msk) {
        DMA2->LIFCR |= DMA_LIFCR_CTEIF0; // clear transfer error interrupt
        dma2_stream0_TE_callaback(&h_dma2);
    }
}

/**
 * @brief stream0 tranfer complete callback API
 * 
 * This callback function is called when the DMA2 peripheral 
 * has completed the transfer of bytes to the first memory
 * buffer (M0AR). 
 */
static void dma2_stream0_TC_callback(DMA_HandleTypeDef *dma) {
    data_ready_flag = DMA_BUF1_RDY;
}

/**
 * @brief DMA Memory1 transfer complete callback API
 * 
 * This API is called when the DMA2 peripheral has filled the 
 * second memory buffer (M1AR). This API should swap the buffers and 
 * return.
 */
static void dma2_stream0_M1_cplt_callback(DMA_HandleTypeDef *dma) {
    data_ready_flag = DMA_BUF2_RDY;
}

/**
 * @brief DMA Transfer error callback function
 * 
 * This API executes on transmission errors. Not sure 
 * exactly what we will do about these but we will see.
 */
static void dma2_stream0_TE_callaback(DMA_HandleTypeDef *dma) {
    tx_err_flag = 1;
}