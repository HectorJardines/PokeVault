#include "stm32f4xx.h"
#include "../../Inc/drivers/io.h"
#include "../../Inc/drivers/sd_spi.h"
#include "../../Inc/drivers/ili9341.h"
#include "../../Inc/drivers/spi.h"
#include "../../../FreeRTOS_WrkSpace/include/queue.h"
#include "../../../FreeRTOS_WrkSpace/include/semphr.h"
#include "../../../FreeRTOS_WrkSpace/include/event_groups.h"

/**************
 * MACROS
 **************/
#define SPI_TX_GET_IRQn(spix)   ((spix)->Instance == SPI1 ? DMA2_Stream2_IRQn : DMA1_Stream4_IRQn)
#define SPI_RX_GET_IRQn(spix)   ((spix)->Instance == SPI1 ? DMA2_Stream0_IRQn : DMA1_Stream3_IRQn)
#define SPI1_ACT_TSK_DEPTH  (128U)
#define SPI1_ACT_TSK_PRIO   (4U)
#define SPI1_ACT_POST_TIMEOUT   (pdMS_TO_TICKS(500U))

#define SPI1_READY_BIT  (0x1U << 0)
#define SPI1_TASK_NFY_IDX   (1U)
#define SPI1_ERR_Msk    (0x1U << 0)
#define SPI1_OK_Msk     (0x1U << 1)

SemaphoreHandle_t act_cplt_disp;
StaticSemaphore_t _act_cplt_disp;
SemaphoreHandle_t act_cplt_sd;
StaticSemaphore_t _act_cplt_sd;


static volatile uint8_t disp_tsk_cplt = 0;
static volatile uint8_t sd_tsk_cplt = 0;

typedef struct {
    uint8_t curr_dev;
    uint8_t stat;
    SPI_HandleTypeDef hspi;
    DMA_HandleTypeDef hdmatx;
    DMA_HandleTypeDef hdmarx;

    BaseType_t hpt_trigger;
    SemaphoreHandle_t bus_lock;
    StaticSemaphore_t _bus_lock;
    SemaphoreHandle_t dma_signal;
    StaticSemaphore_t _dma_signal;
    SemaphoreHandle_t act_cplt;
    StaticSemaphore_t _act_cplt;
    TaskHandle_t curr_task;
} spi_conf_t;

/*****************
 * STATIC DECS
 ******************/
static void spi_set_freq(spi_dev_e dev);
static void spi1_configure(void);
static void spi2_configure(void);
static void spi1_actor(void *arg);
static uint8_t spi_wait(spi_dev_e dev);


static spi_conf_t spi1;
static spi_conf_t spi2;

static TaskHandle_t spi1_act;
static StaticTask_t _spi1_act;
static StackType_t spi1_act_stk[512];

static QueueHandle_t spi1_req_q;
static StaticQueue_t _spi1_req_q;
static uint8_t spi1_req_buf[sizeof(spi1_req_t) * 10];

static EventGroupHandle_t spi1_rdy;
static StaticEventGroup_t _spi1_rdy;
/*************
 * PUB APIs
 **************/

static uint8_t initialized = 0; 
/**
 * @brief Initialize and configure SPI peripheral
 * 
 * 
 */
void spi_init(void) {
    if (!initialized) {
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

        spi1_configure();
        spi2_configure();

        // spi1.act_cplt = xSemaphoreCreateBinaryStatic(&spi1._act_cplt);
        act_cplt_disp = xSemaphoreCreateBinaryStatic(&_act_cplt_disp);
        act_cplt_sd = xSemaphoreCreateBinaryStatic(&_act_cplt_sd);
        spi1.dma_signal = xSemaphoreCreateBinaryStatic(&spi1._dma_signal);
        spi2.bus_lock = xSemaphoreCreateRecursiveMutexStatic(&spi2._bus_lock);
        spi2.dma_signal = xSemaphoreCreateBinaryStatic(&spi2._dma_signal);

        spi1_act = xTaskCreateStatic(spi1_actor, "TASK SPI1", SPI1_ACT_TSK_DEPTH, NULL, 
                                    SPI1_ACT_TSK_PRIO, spi1_act_stk, &_spi1_act);
        spi1_rdy = xEventGroupCreateStatic(&_spi1_rdy);
        spi1_req_q = xQueueCreateStatic(5, sizeof(spi1_req_t), spi1_req_buf, &_spi1_req_q);

        if (spi1_act == NULL || spi1_rdy == NULL || spi1_req_q == NULL)
            while(1) {}

        initialized = 1;
    }
}


/**
 * @brief Posts a SPI1 bus request to the SPI1 actor task
 * 
 * Access to the SPI1 bus is arbitrated by the 
 * SPI1 Actor task. Any tasks wanting to access 
 * the SPI1 bus must call this API with the specific
 * request type.
 * 
 */
uint8_t spi1_post_request(spi1_req_t *req) {
    return !xQueueSendToBack(spi1_req_q, req, SPI1_ACT_POST_TIMEOUT); // convention of ret 0 when OK
}


/**
 * @brief Tasks must wait on the initialization of the SD SPI peripheral
 * 
 * The SPI SD card init has some quirks, thus its execution
 * is isolated at SPI1 startup. Tasks utilizing the SPI1 bus must 
 * wait for its completion befor accessing the bus.
 */
uint8_t spi1_wait_init(void) {
    uint32_t ret = xEventGroupWaitBits(spi1_rdy, SPI1_READY_BIT, pdFALSE, pdTRUE, portMAX_DELAY); // convention of ret 0 when OK
    if (ret & SPI1_READY_BIT)
        return HAL_OK;
    else
        return HAL_ERROR;
}


uint8_t spi1_wait_notify(uint8_t dev) {
    uint32_t ret = HAL_ERROR;

    //  if (dev == DEV_SD) { 
    //     while (!disp_tsk_cplt) 
    //         vTaskDelay(1); 
    //     disp_tsk_cplt = 0; 
    //     ret = spi1.stat; 
    // } else if (dev == DEV_SD) { 
    //     while (!sd_tsk_cplt) 
    //         vTaskDelay(1); 
    //     sd_tsk_cplt = 0; 
    //     ret = spi1.stat;
    // }
    if (xTaskNotifyWaitIndexed(SPI1_TASK_NFY_IDX, 0x00, 0xFFFFFFFF, &ret, portMAX_DELAY) == pdTRUE)
    {
        if (ret & SPI1_ERR_Msk)
            ret = HAL_ERROR;
        else if (ret & SPI1_OK_Msk)
            ret = HAL_OK;
    }
    // if (dev == DEV_SD) {
    //     xSemaphoreTake(act_cplt_disp, portMAX_DELAY);
    //     ret = spi1.stat;
    // } else if (dev == DEV_SD) {
    //     xSemaphoreTake(act_cplt_sd, portMAX_DELAY);
    //     ret = spi1.stat;
    // }
    // if (xSemaphoreTake(spi1.act_cplt, portMAX_DELAY) == pdTRUE)
    //     ret = spi1.stat;
    return (uint8_t)ret;
}


/**
 * @brief 
 */
uint8_t spi_transmit(spi_dev_e dev, uint8_t *data, uint32_t len) {
    SPI_HandleTypeDef *spix;
    if (dev == DEV_SD) {
        spix = &spi1.hspi;
        spi1.curr_dev = dev;
    }
    else {
        if (spi_lock(dev) == pdTRUE) {
            spix = &spi2.hspi;
            spi2.curr_dev = dev;
        } else
            return HAL_ERROR;
    }

    if (!(spix->Instance->CR1 & SPI_CR1_SPE))
        __HAL_SPI_ENABLE(spix);

    uint8_t stat = HAL_SPI_Transmit(spix, data, len, 500);
    spi_unlock(dev);
    return stat;
}


/**
 * @brief 
 */
uint8_t spi_receive(spi_dev_e dev, uint8_t *read_data, uint32_t read_len) {
    SPI_HandleTypeDef *spix;
    if (dev == DEV_SD) {
        spix = &spi1.hspi;
        spi1.curr_dev = dev;
    }
    else {
        if (spi_lock(dev) == pdTRUE) {
            spix = &spi2.hspi;
            spi2.curr_dev = dev;
        } else 
            return HAL_ERROR;
    }

    if (!(spix->Instance->CR1 & SPI_CR1_SPE))
        __HAL_SPI_ENABLE(spix);
    uint8_t stat = HAL_SPI_Receive(spix, read_data, read_len, 500);
    spi_unlock(dev);
    return stat;
}


/**
 * @brief Thread-safe spi transmit dma implementation
 * 
 * This function assumes that the SPI bus corresponding
 * to the SPI device has been locked before calling...
 * 
 */
uint8_t spi_transmit_dma(spi_dev_e dev, uint8_t *data, uint32_t len) {
    uint8_t status = HAL_OK;
    SPI_HandleTypeDef *spix;
    
    if (dev == DEV_SD) {
        spix = &spi1.hspi;
        spi1.curr_dev = dev;
        spi1.curr_task = xTaskGetCurrentTaskHandle();
    }
    else {
        if (spi_lock(dev) == pdTRUE) {
            spix = &spi2.hspi;
            spi2.curr_dev = dev;
            spi2.curr_task = xTaskGetCurrentTaskHandle();
        } else
            return HAL_ERROR;
    }

    status = HAL_SPI_Transmit_DMA(spix, data, len);
    if (status == HAL_OK)
        spi_wait(dev);
    spi_unlock(dev);
    return status;
}

/**
 * @brief Thread-safe spi receive dma implementation
 */
uint8_t spi_receive_dma(spi_dev_e dev, uint8_t *read_data, uint32_t read_len) {
    uint8_t status = HAL_OK;
    SPI_HandleTypeDef *spix;
    if (dev == DEV_SD) {
        spix = &spi1.hspi;
        spi1.curr_dev = dev;
        spi1.curr_task = xTaskGetCurrentTaskHandle();
    }
    else {
        status = spi_lock(dev);
        if (status == pdTRUE) {
            spix = &spi2.hspi;
            spi2.curr_dev = dev;
            spi2.curr_task = xTaskGetCurrentTaskHandle();
        } else
            return HAL_ERROR;
    }

    status = HAL_SPI_Receive_DMA(spix, read_data, read_len);
    if (status == HAL_OK)
        spi_wait(dev);
    spi_unlock(dev);
    return status;
}


/**
 * @brief Sleeps the thread until the lock is obtained for the device
 * 
 * 
 * 
 */
uint8_t spi_lock(spi_dev_e dev) {
    BaseType_t lock_obtained = pdFALSE;
    
    if (dev == DEV_SD) {
    }
    else {
        lock_obtained = xSemaphoreTakeRecursive(spi2.bus_lock, portMAX_DELAY);
        spi_set_freq(dev);
        // uint8_t byte = 0xFF;
        // spi_transmit(dev, &byte, 1);
    }
    return lock_obtained;
}


/**
 * @brief Release the lock associated with the SPI device
 * 
 * 
 * 
 */
uint8_t spi_unlock(spi_dev_e dev) {
    BaseType_t lock_release = pdFALSE;
    if (dev == DEV_SD) {
    }
    else
        lock_release = xSemaphoreGiveRecursive(spi2.bus_lock);
    
    return lock_release;
}


/*****************
 * STATIC DEFS
 *****************/

static void spi1_actor(void *arg) {
    spi1_req_t request;
    uint8_t stat = 0;
    const uint8_t byte = 0xFF;

    // initialize SD card
    stat = SD_SPI_Init();
    if (stat == HAL_OK) {
        spi_set_freq(DEV_SD);
        xEventGroupSetBits(spi1_rdy, SPI1_READY_BIT);
    } else while(1) {};
    // xEventGroupSetBits(spi1_rdy, SPI1_READY_BIT);
    for(;;) {
        if (xQueueReceive(spi1_req_q, &request, portMAX_DELAY) == pdTRUE) {
            switch(request.req_type) {
            case SD_READ_BLOCKS:
                spi_set_freq(DEV_SD);
                io_set_out(IO_DBG_A, LOW);
                spi1.stat = SD_ReadBlocks(request.sd_io.buff, request.sd_io.sector, request.sd_io.count);
                // xSemaphoreGive(act_cplt_sd);
                sd_tsk_cplt = 1;
                io_set_out(IO_DBG_A, HIGH);
                break;
            case SD_WRITE_BLOCKS:
                io_set_out(IO_DBG_A, LOW);
                spi_set_freq(DEV_SD);
                spi1.stat = SD_WriteBlocks((const uint8_t *)request.sd_io.buff, request.sd_io.sector, request.sd_io.count);
                // xSemaphoreGive(act_cplt_sd);
                sd_tsk_cplt = 1;
                io_set_out(IO_DBG_A, HIGH);
                break;
            case ILI9341_SEND_CMD:
                io_set_out(IO_DBG_B, LOW);
                spi_set_freq(DEV_DISP);
                // spi1.stat = ili9341_spi_send_cmd(request.ili9341_io.cmd, request.ili9341_io.cmd_size, request.ili9341_io.param, request.ili9341_io.param_size);
                // spi_set_freq(DEV_SD_INIT);
                // resets SD card to known state, hacky but works i think
                // spi_transmit(DEV_SD, &byte, 1);
                io_set_out(IO_DBG_B, HIGH);
                // xSemaphoreGive(act_cplt_disp);
                disp_tsk_cplt = 1;
                break;
            case ILI9341_SEND_PIXELS:
                io_set_out(IO_DBG_B, LOW);
                spi_set_freq(DEV_DISP);
                // spi1.stat = ili9341_spi_send_pixels(request.ili9341_io.cmd, request.ili9341_io.cmd_size, request.ili9341_io.param, request.ili9341_io.param_size);         
                // spi_set_freq(DEV_SD_INIT);
                // resets SD card to known state, hacky but works i think
                // spi_transmit(DEV_SD, &byte, 1);
                // xSemaphoreGive(act_cplt_disp);
                disp_tsk_cplt = 1;
                io_set_out(IO_DBG_B, HIGH);
                break;
            }
            xTaskNotifyIndexed(request.req_task, SPI1_TASK_NFY_IDX, stat == 0 ? SPI1_OK_Msk : SPI1_ERR_Msk, eSetBits);
        }
    }
}

/**
 * @brief Sleeps the task until it is notified by DMA interrupt
 * 
 * 
 */
static uint8_t spi_wait(spi_dev_e dev) {
    uint8_t ret = 0;
    if (dev == DEV_SD) {
        ret = xSemaphoreTake(spi1.dma_signal, portMAX_DELAY);
    } else {
        ret = xSemaphoreTake(spi2.dma_signal, portMAX_DELAY);
    }
    if(dev == DEV_SD)
        while(spi1.hspi.Instance->SR & SPI_SR_BSY);
    else
        while(spi2.hspi.Instance->SR & SPI_SR_BSY)
    return ret;
}


/**
 * @brief Configures the SPI CLK frequency for the specific device
 * 
 * 
 */
static void spi_set_freq(spi_dev_e dev) {
    if (dev == DEV_SD) {
        while (__HAL_SPI_GET_FLAG(&spi1.hspi, SPI_SR_BSY));
        spi1.hspi.Instance->CR1 &= ~(SPI_CR1_SPE);
        spi1.hspi.Instance->CR1 &= ~(SPI_CR1_BR); // clear current BR
        if (dev == DEV_SD)
            spi1.hspi.Instance->CR1 |= (SPI_BAUDRATEPRESCALER_4);
        spi1.hspi.Instance->CR1 |= (SPI_CR1_SPE);
    } else {
        while (__HAL_SPI_GET_FLAG(&spi2.hspi, SPI_SR_BSY));
        spi2.hspi.Instance->CR1 &= ~(SPI_CR1_SPE);
        spi2.hspi.Instance->CR1 &= ~(SPI_CR1_BR); // clear current BR
        if (dev == DEV_MFRC)
            spi2.hspi.Instance->CR1 |= (SPI_BAUDRATEPRESCALER_8);
        else if (dev == DEV_ETH)
            spi2.hspi.Instance->CR1 |= (SPI_BAUDRATEPRESCALER_2);
        else if (dev == DEV_TOUCH)
            spi2.hspi.Instance->CR1 |= (SPI_BAUDRATEPRESCALER_32);
        else if (dev == DEV_DISP)
            spi2.hspi.Instance->CR1 |= (SPI_BAUDRATEPRESCALER_2);
        else
            spi2.hspi.Instance->CR1 |= (SPI_BAUDRATEPRESCALER_256);
        spi2.hspi.Instance->CR1 |= (SPI_CR1_SPE);
    }
    HAL_Delay(1);
}
 

/**
 * @brief Configures the SPI peripheral used by ETH/DISP
 */
static void spi1_configure(void) {
    /************* CONFIGURE SPI TX DMA *******************/
    spi1.hdmatx.Instance = DMA2_Stream2;
    spi1.hdmatx.Init.Channel = DMA_CHANNEL_2;
    spi1.hdmatx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    spi1.hdmatx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    spi1.hdmatx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    spi1.hdmatx.Init.MemInc = DMA_MINC_ENABLE;
    spi1.hdmatx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi1.hdmatx.Init.PeriphInc = DMA_PINC_DISABLE;
    spi1.hdmatx.Init.Mode = DMA_NORMAL;

    /************* CONFIGURE SPI RX DMA *******************/
    spi1.hdmarx.Instance = DMA2_Stream0;
    spi1.hdmarx.Init.Channel = DMA_CHANNEL_3;
    spi1.hdmarx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    spi1.hdmarx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    spi1.hdmarx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    spi1.hdmarx.Init.MemInc = DMA_MINC_ENABLE;
    spi1.hdmarx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi1.hdmarx.Init.PeriphInc = DMA_PINC_DISABLE;
    spi1.hdmarx.Init.Mode = DMA_NORMAL;

    /************** CONFIGURE SPI PERIPH ***************/
    spi1.hspi.Instance = SPI1;
    spi1.hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
    spi1.hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
    spi1.hspi.Init.Mode = SPI_MODE_MASTER;
    spi1.hspi.Init.DataSize = SPI_DATASIZE_8BIT;
    spi1.hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    spi1.hspi.Init.NSS = SPI_NSS_SOFT;
    spi1.hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    spi1.hspi.Init.Direction = SPI_DIRECTION_2LINES;
    spi1.hspi.Init.CRCPolynomial = SPI_CRCCALCULATION_DISABLE;
    spi1.hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;

    HAL_DMA_Init(&spi1.hdmatx);
    HAL_DMA_Init(&spi1.hdmarx);
    HAL_SPI_Init(&spi1.hspi);

    __HAL_LINKDMA(&spi1.hspi, hdmatx, spi1.hdmatx);
    __HAL_LINKDMA(&spi1.hspi, hdmarx, spi1.hdmarx);
    NVIC_SetPriority(SPI_RX_GET_IRQn(&spi1.hspi), 5);
    NVIC_SetPriority(SPI_TX_GET_IRQn(&spi1.hspi), 5);
    NVIC_EnableIRQ(SPI_RX_GET_IRQn(&spi1.hspi));
    NVIC_EnableIRQ(SPI_TX_GET_IRQn(&spi1.hspi));
}



/**
 * @brief Config SPI periph used by MFRC/SD reader
 */
static void spi2_configure(void) {
    /************* CONFIGURE SPI TX DMA *******************/
    spi2.hdmatx.Instance = DMA1_Stream4;
    spi2.hdmatx.Init.Channel = DMA_CHANNEL_0;
    spi2.hdmatx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    spi2.hdmatx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    spi2.hdmatx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    spi2.hdmatx.Init.MemInc = DMA_MINC_ENABLE;
    spi2.hdmatx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi2.hdmatx.Init.PeriphInc = DMA_PINC_DISABLE;
    spi2.hdmatx.Init.Mode = DMA_NORMAL;

    /************* CONFIGURE SPI RX DMA *******************/
    spi2.hdmarx.Instance = DMA1_Stream3;
    spi2.hdmarx.Init.Channel = DMA_CHANNEL_0;
    spi2.hdmarx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    spi2.hdmarx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    spi2.hdmarx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    spi2.hdmarx.Init.MemInc = DMA_MINC_ENABLE;
    spi2.hdmarx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    spi2.hdmarx.Init.PeriphInc = DMA_PINC_DISABLE;
    spi2.hdmarx.Init.Mode = DMA_NORMAL;

    /************** CONFIGURE SPI PERIPH ***************/
    spi2.hspi.Instance = SPI2;
    spi2.hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
    spi2.hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
    spi2.hspi.Init.Mode = SPI_MODE_MASTER;
    spi2.hspi.Init.DataSize = SPI_DATASIZE_8BIT;
    spi2.hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    spi2.hspi.Init.NSS = SPI_NSS_SOFT;
    spi2.hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    spi2.hspi.Init.Direction = SPI_DIRECTION_2LINES;
    spi2.hspi.Init.CRCPolynomial = SPI_CRCCALCULATION_DISABLE;
    spi2.hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;

    HAL_DMA_Init(&spi2.hdmatx);
    HAL_DMA_Init(&spi2.hdmarx);
    HAL_SPI_Init(&spi2.hspi);

    __HAL_LINKDMA(&spi2.hspi, hdmatx, spi2.hdmatx);
    __HAL_LINKDMA(&spi2.hspi, hdmarx, spi2.hdmarx);
    NVIC_SetPriority(SPI_RX_GET_IRQn(&spi2.hspi), 5);
    NVIC_SetPriority(SPI_TX_GET_IRQn(&spi2.hspi), 5);
    NVIC_EnableIRQ(SPI_RX_GET_IRQn(&spi2.hspi));
    NVIC_EnableIRQ(SPI_TX_GET_IRQn(&spi2.hspi));
}


/**
 * @brief 
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        spi1.hpt_trigger = pdFALSE;
        while (!(hspi->Instance->SR & SPI_SR_TXE));
        while(hspi->Instance->SR & SPI_SR_BSY);
        __HAL_SPI_CLEAR_OVRFLAG(hspi);

        xSemaphoreGiveFromISR(spi1.dma_signal, &spi1.hpt_trigger);
        portYIELD_FROM_ISR(spi1.hpt_trigger);
    }
    else if (hspi->Instance == SPI2) {
        spi2.hpt_trigger = pdFALSE;
        while (!(hspi->Instance->SR & SPI_SR_TXE));
        while(hspi->Instance->SR & SPI_SR_BSY);
        __HAL_SPI_CLEAR_OVRFLAG(hspi);

        xSemaphoreGiveFromISR(spi2.dma_signal, &spi2.hpt_trigger);
        portYIELD_FROM_ISR(spi2.hpt_trigger);
    }
}


/**
 * @brief 
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        spi1.hpt_trigger = pdFALSE;
        
        while ((hspi->Instance->SR & SPI_SR_RXNE));
        while (!(hspi->Instance->SR & SPI_SR_TXE));
        while(hspi->Instance->SR & SPI_SR_BSY);
        __HAL_SPI_CLEAR_OVRFLAG(hspi);
        
        xSemaphoreGiveFromISR(spi1.dma_signal, &spi1.hpt_trigger);
        portYIELD_FROM_ISR(spi1.hpt_trigger);
    }
    else if (hspi->Instance == SPI2) {
        spi2.hpt_trigger = pdFALSE;
        __HAL_SPI_CLEAR_OVRFLAG(hspi);
        while ((hspi->Instance->SR & SPI_SR_RXNE));
        while (!(hspi->Instance->SR & SPI_SR_TXE));
        while(hspi->Instance->SR & SPI_SR_BSY);
        
        xSemaphoreGiveFromISR(spi2.dma_signal, &spi2.hpt_trigger);
        portYIELD_FROM_ISR(spi2.hpt_trigger);
    }
}


void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    HAL_SPI_TxRxCpltCallback(hspi);
}

/**
 * @brief Handler for SPI1 TX (ethernet/display)
 */
void DMA2_Stream2_IRQHandler(void) {
    HAL_DMA_IRQHandler(&spi1.hdmatx);
}


/**
 * @brief Handler for SPI1 RX
 */
void DMA2_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&spi1.hdmarx);
}


/**
 * @brief Handler for SPI2 TX (tag/sd card)
 */
void DMA1_Stream4_IRQHandler(void) {
    HAL_DMA_IRQHandler(&spi2.hdmatx);
}


/**
 * @brief Handler for SPI2 RX
 */
void DMA1_Stream3_IRQHandler(void) {
    HAL_DMA_IRQHandler(&spi2.hdmarx);
}
