#include "../Inc/drivers/spi.h"

#define SPI_TX_RX_TIMEOUT_MS   (500U)

/*************************
 * STATIC DECLARATIONS
 *************************/
SPI_HandleTypeDef h_spi1;

static void spi_configure_mfrc(void);

/********************
 * USER APIs
 ********************/


void spi_init(spi_device_e spi_dev) {
    if (spi_dev == SPI_DEVICE_MFRC522) {
        RCC->APB2ENR |= (RCC_APB2ENR_SPI1EN); // enable peripheral clock
        spi_configure_mfrc();
    }
}


int8_t spi_transmit_mfrc(uint8_t data) {
    uint8_t stat = HAL_SPI_Transmit(&h_spi1, &data, 1, SPI_TX_RX_TIMEOUT_MS);
    if (stat == HAL_OK)
        stat = data;
    else
        stat = -1;
    return stat;
}


int8_t spi_receive_mfrc(void) {
    uint8_t data = 0x00;
    uint8_t stat = HAL_SPI_Receive(&h_spi1, &data, 1, SPI_TX_RX_TIMEOUT_MS);
    if (stat == HAL_OK)
        stat = data;
    else
        stat = -1;
    return stat;
}


uint8_t TM_SPI_Send(SPI_TypeDef *spix, uint8_t byte) {
    while(((spix)->SR & (SPI_SR_TXE | SPI_SR_RXNE)) == 0 || spix->SR & SPI_SR_BSY);
    spix->DR = byte;
    while(((spix)->SR & (SPI_SR_TXE | SPI_SR_RXNE)) == 0 || spix->SR & SPI_SR_BSY);
    return (uint8_t)spix->DR;
}


/*************************
 * STATIC DECLARATIONS
 *************************/

/**
 * @brief Configure the SPI1 peripheral
 * 
 * Configures the SPI1 peripheral in SPI mode 0 (CPOL=0/CPHA=0),
 * with MSB first TX/RX, full-duplex 8-bit word communication. 
 * Peripheral baudrate set to 4.6Mhz, with SSM.
 * 
 * @return void
 */
static void spi_configure_mfrc(void) {
    h_spi1.Instance = SPI1;
    h_spi1.Init.Mode = SPI_MODE_MASTER;
    h_spi1.Init.FirstBit = SPI_FIRSTBIT_MSB;

    // Configure SPI mode 0 (sample on rising first edge)
    h_spi1.Init.CLKPhase = SPI_PHASE_1EDGE; // sample on first edge
    h_spi1.Init.CLKPolarity = SPI_POLARITY_LOW; // SCK IDLE LOW

    // configure data size and full-duplex tx
    h_spi1.Init.DataSize = SPI_DATASIZE_8BIT;
    h_spi1.Init.Direction = SPI_DIRECTION_2LINES;
    h_spi1.Init.TIMode = SPI_TIMODE_DISABLE;

    // software chip select
    h_spi1.Init.NSS = SPI_NSS_SOFT;
    h_spi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32; // 75MHz apb2 peripheral clock / 16 = 4.6MHz
    h_spi1.State = HAL_SPI_STATE_READY; // NOT USING MSP INIT FUNCTIONS

    h_spi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;

    HAL_SPI_Init(&h_spi1);
    __HAL_SPI_ENABLE(&h_spi1);
}