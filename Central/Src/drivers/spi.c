#include <stddef.h>
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "../../Inc/drivers/spi.h"
#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"
#include "../../../FreeRTOS_WrkSpace/include/semphr.h"
 
/**************
 * CONFIG
 **************/
/* Task-notification index used for DMA completion.
 * Requires configTASK_NOTIFICATION_ARRAY_ENTRIES > SPI_NFY_IDX. */
#ifndef SPI_NFY_IDX
#define SPI_NFY_IDX             (1U)
#endif
#define SPI_NFY_OK              (0x1U << 0)
#define SPI_NFY_ERR             (0x1U << 1)
 
#define SPI_POLL_TIMEOUT_MS     (500U)
#define SPI_DMA_TIMEOUT_MS      (500U)
#define SPI_IRQ_PRIO            (5U)    /* must be >= configMAX_SYSCALL_INTERRUPT_PRIORITY */
#define SPI_NO_DEV              (-1)
 
/**************
 * TYPES
 **************/
typedef enum {
    SPI_BUS_1 = 0,
    SPI_BUS_2,
    SPI_BUS_4,
    SPI_BUS_COUNT
} spi_bus_e;
 
/* Static, per-bus hardware description. A NULL stream means "no DMA on this bus". */
typedef struct {
    SPI_TypeDef        *spi;
    DMA_Stream_TypeDef *tx_stream;
    uint32_t            tx_channel;
    IRQn_Type           tx_irq;
    DMA_Stream_TypeDef *rx_stream;
    uint32_t            rx_channel;
    IRQn_Type           rx_irq;
    uint32_t            init_prescaler;
} spi_bus_cfg_t;
 
/* Per-device: which bus it is on and what clock it needs. */
typedef struct {
    spi_bus_e bus;
    uint32_t  prescaler;
} spi_dev_cfg_t;
 
typedef struct {
    SPI_HandleTypeDef  hspi;        /* MUST be first: callbacks cast hspi -> bus */
    DMA_HandleTypeDef  hdmatx;
    DMA_HandleTypeDef  hdmarx;
    const spi_bus_cfg_t *cfg;
 
    SemaphoreHandle_t  lock;        /* recursive mutex */
    StaticSemaphore_t  lock_buf;
 
    TaskHandle_t       waiter;      /* task to notify from the DMA ISR */
    volatile uint8_t   dma_pending; /* a DMA transfer is in flight / unreaped */
    int8_t             curr_dev;    /* device the bus clock is currently set for */
} spi_bus_t;
 
_Static_assert(offsetof(spi_bus_t, hspi) == 0, "hspi must be first member of spi_bus_t");
 
/**************
 * TABLES  (the only place bus/device wiring lives)
 **************/
static const spi_bus_cfg_t spi_bus_cfg[SPI_BUS_COUNT] = {
    [SPI_BUS_1] = {
        .spi = SPI1,
        /* No DMA on SPI1 (as in the original). To add it: SPI1_TX = DMA2_Stream3 ch3,
         * SPI1_RX = DMA2_Stream2 ch3 (Stream0 is taken by SPI4_RX), plus IRQ handlers. */
        .tx_stream = NULL, .rx_stream = NULL,
        .init_prescaler = SPI_BAUDRATEPRESCALER_2,
    },
    [SPI_BUS_2] = {
        .spi = SPI2,
        .tx_stream = DMA1_Stream4, .tx_channel = DMA_CHANNEL_0, .tx_irq = DMA1_Stream4_IRQn,
        .rx_stream = DMA1_Stream3, .rx_channel = DMA_CHANNEL_0, .rx_irq = DMA1_Stream3_IRQn,
        .init_prescaler = SPI_BAUDRATEPRESCALER_256,
    },
    [SPI_BUS_4] = {
        .spi = SPI4,
        .tx_stream = DMA2_Stream1, .tx_channel = DMA_CHANNEL_4, .tx_irq = DMA2_Stream1_IRQn,
        .rx_stream = DMA2_Stream0, .rx_channel = DMA_CHANNEL_4, .rx_irq = DMA2_Stream0_IRQn,
        .init_prescaler = SPI_BAUDRATEPRESCALER_2,
    },
};
 
/* Runtime-mutable so spi_set_dev_speed() works. */
static spi_dev_cfg_t spi_dev_cfg[DEV_COUNT] = {
    /* SD starts slow for card init; call spi_set_dev_speed(DEV_SD, ...PRESCALER_2) afterwards */
    [DEV_SD]    = { SPI_BUS_2, SPI_BAUDRATEPRESCALER_256 },
    [DEV_FLASH] = { SPI_BUS_1, SPI_BAUDRATEPRESCALER_8   },
    [DEV_ETH]   = { SPI_BUS_4, SPI_BAUDRATEPRESCALER_8   },
    [DEV_DISP]  = { SPI_BUS_4, SPI_BAUDRATEPRESCALER_8   },
    [DEV_TOUCH] = { SPI_BUS_4, SPI_BAUDRATEPRESCALER_32  },
    [DEV_MFRC]  = { SPI_BUS_4, SPI_BAUDRATEPRESCALER_16  },
};
 
static spi_bus_t spi_buses[SPI_BUS_COUNT];
static uint8_t   initialized = 0;
 
/**************
 * STATIC DECS
 **************/
static void     spi_bus_configure(spi_bus_t *bus, const spi_bus_cfg_t *cfg);
static void     spi_dma_setup(DMA_HandleTypeDef *h, DMA_Stream_TypeDef *stream,
                              uint32_t channel, uint32_t direction);
static uint8_t  spi_acquire(spi_dev_e dev, spi_bus_t **bus_out);
static void     spi_release(spi_bus_t *bus);
static void     spi_apply_dev(spi_bus_t *bus, spi_dev_e dev);
static uint8_t  spi_dma_start(spi_dev_e dev, uint8_t *tx, uint8_t *rx, uint32_t len);
static uint8_t  spi_dma_finish(spi_bus_t *bus);
static void     spi_isr_notify(SPI_HandleTypeDef *hspi, uint32_t bits);
 
/**************
 * PUB APIs
 **************/
 
/**
 * @brief Enable clocks, configure every bus, create the bus mutexes.
 */
void spi_init(void) {
    if (initialized)
        return;
 
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI4EN;
 
    for (uint32_t i = 0; i < SPI_BUS_COUNT; i++)
        spi_bus_configure(&spi_buses[i], &spi_bus_cfg[i]);
 
    initialized = 1;
}
 
/**
 * @brief Lock the bus the device is on. Recursive: nest freely.
 *
 * Only takes the mutex; the bus clock is switched to the device's
 * prescaler by the transfer call that follows.
 */
uint8_t spi_lock(spi_dev_e dev) {
    if ((unsigned)dev >= DEV_COUNT)
        return pdFALSE;
    spi_bus_t *bus = &spi_buses[spi_dev_cfg[dev].bus];
    return xSemaphoreTakeRecursive(bus->lock, portMAX_DELAY);
}
 
uint8_t spi_unlock(spi_dev_e dev) {
    if ((unsigned)dev >= DEV_COUNT)
        return pdFALSE;
    spi_bus_t *bus = &spi_buses[spi_dev_cfg[dev].bus];
    return xSemaphoreGiveRecursive(bus->lock);
}
 
uint8_t spi_set_dev_speed(spi_dev_e dev, uint32_t prescaler) {
    spi_bus_t *bus;
    uint8_t stat = spi_acquire(dev, &bus);
    if (stat != HAL_OK)
        return stat;
    spi_dev_cfg[dev].prescaler = prescaler;
    spi_apply_dev(bus, dev);
    spi_release(bus);
    return HAL_OK;
}
 
/* ---------------- polling ---------------- */
 
uint8_t spi_transmit(spi_dev_e dev, const uint8_t *data, uint32_t len) {
    spi_bus_t *bus;
    uint8_t stat = spi_acquire(dev, &bus);
    if (stat != HAL_OK)
        return stat;
    stat = HAL_SPI_Transmit(&bus->hspi, (uint8_t *)data, len, SPI_POLL_TIMEOUT_MS);
    spi_release(bus);
    return stat;
}
 
uint8_t spi_receive(spi_dev_e dev, uint8_t *data, uint32_t len) {
    spi_bus_t *bus;
    uint8_t stat = spi_acquire(dev, &bus);
    if (stat != HAL_OK)
        return stat;
    stat = HAL_SPI_Receive(&bus->hspi, data, len, SPI_POLL_TIMEOUT_MS);
    spi_release(bus);
    return stat;
}
 
uint8_t spi_transfer(spi_dev_e dev, const uint8_t *tx, uint8_t *rx, uint32_t len) {
    spi_bus_t *bus;
    uint8_t stat = spi_acquire(dev, &bus);
    if (stat != HAL_OK)
        return stat;
    stat = HAL_SPI_TransmitReceive(&bus->hspi, (uint8_t *)tx, rx, len, SPI_POLL_TIMEOUT_MS);
    spi_release(bus);
    return stat;
}
 
/* ---------------- DMA ---------------- */
 
uint8_t spi_transmit_dma_start(spi_dev_e dev, const uint8_t *data, uint32_t len) {
    return spi_dma_start(dev, (uint8_t *)data, NULL, len);
}
 
uint8_t spi_receive_dma_start(spi_dev_e dev, uint8_t *data, uint32_t len) {
    return spi_dma_start(dev, NULL, data, len);
}
 
/**
 * @brief Sleep until the DMA ISR notifies this task, then unlock the bus.
 *
 * Must be called by the same task that called *_dma_start().
 */
uint8_t spi_dma_wait(spi_dev_e dev) {
    if ((unsigned)dev >= DEV_COUNT)
        return HAL_ERROR;
    spi_bus_t *bus = &spi_buses[spi_dev_cfg[dev].bus];
    if (!bus->dma_pending || bus->waiter != xTaskGetCurrentTaskHandle())
        return HAL_ERROR;
    return spi_dma_finish(bus);
}
 
uint8_t spi_transmit_dma(spi_dev_e dev, const uint8_t *data, uint32_t len) {
    uint8_t stat = spi_transmit_dma_start(dev, data, len);
    if (stat == HAL_OK)
        stat = spi_dma_wait(dev);
    return stat;
}
 
uint8_t spi_receive_dma(spi_dev_e dev, uint8_t *data, uint32_t len) {
    uint8_t stat = spi_receive_dma_start(dev, data, len);
    if (stat == HAL_OK)
        stat = spi_dma_wait(dev);
    return stat;
}
 
/*****************
 * STATIC DEFS
 *****************/
 
/**
 * @brief Take the bus, make sure it is idle, and set the clock for dev.
 *
 * If this task left a DMA transfer pending on the bus, it is completed
 * here first. Any other task is blocked on the mutex until the pending
 * transfer is reaped, so two DMA transfers can never overlap on a bus.
 */
static uint8_t spi_acquire(spi_dev_e dev, spi_bus_t **bus_out) {
    if ((unsigned)dev >= DEV_COUNT)
        return HAL_ERROR;
 
    spi_bus_t *bus = &spi_buses[spi_dev_cfg[dev].bus];
    if (xSemaphoreTakeRecursive(bus->lock, portMAX_DELAY) != pdTRUE)
        return HAL_ERROR;
 
    if (bus->dma_pending)
        spi_dma_finish(bus);        /* releases the level taken by the *_start() */
 
    if (bus->curr_dev != (int8_t)dev)
        spi_apply_dev(bus, dev);
 
    *bus_out = bus;
    return HAL_OK;
}
 
static void spi_release(spi_bus_t *bus) {
    xSemaphoreGiveRecursive(bus->lock);
}
 
/**
 * @brief Set the bus clock to the device's prescaler (bus must be locked).
 */
static void spi_apply_dev(spi_bus_t *bus, spi_dev_e dev) {
    SPI_TypeDef *spi = bus->hspi.Instance;
    while (spi->SR & SPI_SR_BSY);
    CLEAR_BIT(spi->CR1, SPI_CR1_SPE);
    MODIFY_REG(spi->CR1, SPI_CR1_BR, spi_dev_cfg[dev].prescaler);
    bus->curr_dev = (int8_t)dev;    /* HAL_SPI_* re-enables SPE on entry */
}
 
/**
 * @brief Lock the bus, start a DMA transfer, and leave the bus locked.
 *
 * Exactly one of tx / rx is non-NULL.
 */
static uint8_t spi_dma_start(spi_dev_e dev, uint8_t *tx, uint8_t *rx, uint32_t len) {
    if ((unsigned)dev >= DEV_COUNT)
        return HAL_ERROR;
    if (spi_buses[spi_dev_cfg[dev].bus].hspi.hdmatx == NULL)
        return HAL_ERROR;           /* bus has no DMA configured */
 
    spi_bus_t *bus;
    uint8_t stat = spi_acquire(dev, &bus);
    if (stat != HAL_OK)
        return stat;
 
    /* discard any stale notification before arming the ISR */
    xTaskNotifyStateClearIndexed(NULL, SPI_NFY_IDX);
    ulTaskNotifyValueClearIndexed(NULL, SPI_NFY_IDX, 0xFFFFFFFFU);
 
    bus->waiter      = xTaskGetCurrentTaskHandle();
    bus->dma_pending = 1;
 
    stat = (tx != NULL) ? HAL_SPI_Transmit_DMA(&bus->hspi, tx, (uint16_t)len)
                        : HAL_SPI_Receive_DMA(&bus->hspi, rx, (uint16_t)len);
 
    if (stat != HAL_OK) {
        bus->dma_pending = 0;
        bus->waiter      = NULL;
        spi_release(bus);
    }
    return stat;                    /* on HAL_OK the lock is intentionally still held */
}
 
/**
 * @brief Wait for the ISR notification, clean up, release the lock level
 *        taken by spi_dma_start(). Caller must own the bus.
 */
static uint8_t spi_dma_finish(spi_bus_t *bus) {
    uint32_t bits = 0;
    uint8_t  stat = HAL_TIMEOUT;
 
    if (xTaskNotifyWaitIndexed(SPI_NFY_IDX, 0x00, 0xFFFFFFFFU, &bits,
                               pdMS_TO_TICKS(SPI_DMA_TIMEOUT_MS)) == pdTRUE) {
        stat = (bits & SPI_NFY_ERR) ? HAL_ERROR : HAL_OK;
    } else {
        HAL_SPI_Abort(&bus->hspi);
    }
 
    while (bus->hspi.Instance->SR & SPI_SR_BSY);
    __HAL_SPI_CLEAR_OVRFLAG(&bus->hspi);
 
    bus->waiter      = NULL;
    bus->dma_pending = 0;
    spi_release(bus);
    return stat;
}
 
static void spi_dma_setup(DMA_HandleTypeDef *h, DMA_Stream_TypeDef *stream,
                          uint32_t channel, uint32_t direction) {
    h->Instance                 = stream;
    h->Init.Channel             = channel;
    h->Init.Direction           = direction;
    h->Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    h->Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    h->Init.MemInc              = DMA_MINC_ENABLE;
    h->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    h->Init.PeriphInc           = DMA_PINC_DISABLE;
    h->Init.Mode                = DMA_NORMAL;
    configASSERT(HAL_DMA_Init(h) == HAL_OK);
}
 
/**
 * @brief The single generic bus configuration routine.
 */
static void spi_bus_configure(spi_bus_t *bus, const spi_bus_cfg_t *cfg) {
    bus->cfg      = cfg;
    bus->curr_dev = SPI_NO_DEV;
    bus->waiter   = NULL;
    bus->lock     = xSemaphoreCreateRecursiveMutexStatic(&bus->lock_buf);
    configASSERT(bus->lock != NULL);
 
    const uint8_t has_dma = (cfg->tx_stream != NULL) && (cfg->rx_stream != NULL);
 
    if (has_dma) {
        spi_dma_setup(&bus->hdmatx, cfg->tx_stream, cfg->tx_channel, DMA_MEMORY_TO_PERIPH);
        spi_dma_setup(&bus->hdmarx, cfg->rx_stream, cfg->rx_channel, DMA_PERIPH_TO_MEMORY);
    }
 
    bus->hspi.Instance               = cfg->spi;
    bus->hspi.Init.Mode              = SPI_MODE_MASTER;
    bus->hspi.Init.Direction         = SPI_DIRECTION_2LINES;
    bus->hspi.Init.DataSize          = SPI_DATASIZE_8BIT;
    bus->hspi.Init.CLKPolarity       = SPI_POLARITY_LOW;
    bus->hspi.Init.CLKPhase          = SPI_PHASE_1EDGE;
    bus->hspi.Init.NSS               = SPI_NSS_SOFT;
    bus->hspi.Init.TIMode            = SPI_TIMODE_DISABLE;
    bus->hspi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    bus->hspi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    bus->hspi.Init.CRCPolynomial     = 10;
    bus->hspi.Init.BaudRatePrescaler = cfg->init_prescaler;
    configASSERT(HAL_SPI_Init(&bus->hspi) == HAL_OK);
 
    if (has_dma) {
        __HAL_LINKDMA(&bus->hspi, hdmatx, bus->hdmatx);
        __HAL_LINKDMA(&bus->hspi, hdmarx, bus->hdmarx);
 
        NVIC_SetPriority(cfg->tx_irq, SPI_IRQ_PRIO);
        NVIC_SetPriority(cfg->rx_irq, SPI_IRQ_PRIO);
        NVIC_EnableIRQ(cfg->tx_irq);
        NVIC_EnableIRQ(cfg->rx_irq);
    }
}
 
/*****************
 * ISR / CALLBACKS
 *****************/
 
/* Wake the task that started the transfer; hspi is the first member of spi_bus_t. */
static void spi_isr_notify(SPI_HandleTypeDef *hspi, uint32_t bits) {
    spi_bus_t *bus = (spi_bus_t *)hspi;
    BaseType_t woken = pdFALSE;
 
    if (bus->waiter != NULL)
        xTaskNotifyIndexedFromISR(bus->waiter, SPI_NFY_IDX, bits, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}
 
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)    { spi_isr_notify(hspi, SPI_NFY_OK);  }
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)    { spi_isr_notify(hspi, SPI_NFY_OK);  }
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)  { spi_isr_notify(hspi, SPI_NFY_OK);  }
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)     { spi_isr_notify(hspi, SPI_NFY_ERR); }
 
/* One handler per DMA stream in use. Add SPI1's here if you give it DMA. */
void DMA1_Stream4_IRQHandler(void) { HAL_DMA_IRQHandler(&spi_buses[SPI_BUS_2].hdmatx); }
void DMA1_Stream3_IRQHandler(void) { HAL_DMA_IRQHandler(&spi_buses[SPI_BUS_2].hdmarx); }
void DMA2_Stream1_IRQHandler(void) { HAL_DMA_IRQHandler(&spi_buses[SPI_BUS_4].hdmatx); }
void DMA2_Stream0_IRQHandler(void) { HAL_DMA_IRQHandler(&spi_buses[SPI_BUS_4].hdmarx); }

