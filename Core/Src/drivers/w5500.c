#include "./spi.h"
#include "../../Inc/drivers/w5500.h"
#include "../../../Drivers/w5500_eth/wizchip_conf.h"
#include "../../../Drivers/w5500_eth/DHCP/dhcp.h"
#include "../../../Drivers/w5500_eth/DNS/dns.h"

#define DHCP_DISABLED   (0U)
#define DHCP_ENABLED    (1U)
#define IP_UNASSIGNED   (0U)
#define IP_ASSIGNED     (1U)
#define DHCP_BUFFER_LEN_BYTES   (512U)
#define DHCP_SOCKET (7U)
#define DNS_SOCKET  (6U)

#define USE_DHCP 1
wiz_NetInfo net_info = {
    .mac = {6,0,7,6,4,5}, 
    .ip = {10,0,0,7}, 
    .sn = {255,255,255,0}, 
    .gw = {10,0,0,1}, 
    .dns = {8,8,8,8}, 
#ifdef USE_DHCP
    .dhcp = DHCP_ENABLED
#else
    .dhcp = DHCP_DISABLED
#endif
};

/*********************
 * STATIC DECLARATIONS
 **********************/
static void w5500_cs_low(void);
static void w5500_cs_high(void);
static uint8_t w5500_spi_read_byte(void);
static void w5500_spi_write_byte(uint8_t data);
static void w5500_spi_burst_read(uint8_t *data, uint16_t len);
static void w5500_spi_burst_write(uint8_t *data, uint16_t len);
static void w5500_dhcp_ip_set(void);
static void w5500_dhcp_ip_not_set(void);


static volatile uint8_t dhcp_ip_assigned = IP_UNASSIGNED;
static uint8_t dhcp_buffer[DHCP_BUFFER_LEN_BYTES];
static uint8_t dns_buffer[MAX_DNS_BUF_SIZE];
/*************************
 * PUBLIC APIs
 **************************/


/**
 * @brief
 */
uint8_t w5500_init(void) {
    w5500_status_e status = W5500_OK;
    uint8_t w5500_mem_size[2][8] = {{2,2,2,2,2,2,2,2}, {2,2,2,2,2,2,2,2}};

    // register chip select callback functions
    reg_wizchip_cs_cbfunc(w5500_cs_low, w5500_cs_high);

    // register spi read/write byte/burst callback functions
    reg_wizchip_spi_cbfunc(w5500_spi_read_byte, w5500_spi_write_byte);
    reg_wizchip_spiburst_cbfunc(w5500_spi_burst_read, w5500_spi_burst_write);

    // reset wizchip
    ctlwizchip(CW_RESET_WIZCHIP, NULL);

    // Initialize chip
    if (ctlwizchip(CW_INIT_WIZCHIP, (void *)w5500_mem_size) == -1)
        status = W5500_ERR;

    // Retrieve chip version/verify communication
    uint8_t version = getVERSIONR();
    if (version != 0x04)
        status = W5500_ERR;

    // CHECK IF PHY LINK IS UP
    uint8_t phy_link = PHY_LINK_OFF;
    uint8_t retries = 10;
    do {
        ctlwizchip(CW_GET_PHYLINK, &phy_link);
        HAL_Delay(500);
    } while(retries-- && phy_link == PHY_LINK_OFF);

    if (phy_link != PHY_LINK_ON)
        status = W5500_ERR;

/********************* DHCP CONFIGURATIONS *************************/    
#ifdef USE_DHCP
    // register dhcp_ip_assigned callback functions
    reg_dhcp_cbfunc(w5500_dhcp_ip_set, w5500_dhcp_ip_set, w5500_dhcp_ip_not_set);
    setSHAR(net_info.mac);
    DHCP_init(DHCP_SOCKET, dhcp_buffer);

    retries = 20;
    do {
        DHCP_run();
    } while(retries-- && !dhcp_ip_assigned);

    if (!dhcp_ip_assigned) {
        // USE static ip instead
        ctlnetwork(CN_SET_NETINFO, (void *)&net_info);
    }
    else {
        getIPfromDHCP(net_info.ip);
        getDNSfromDHCP(net_info.dns);
        getGWfromDHCP(net_info.gw);
        getSNfromDHCP(net_info.sn);

        // retrieved configuration are not auto applied, apply here
        ctlnetwork(CN_SET_NETINFO, (void *)&net_info);
    }
#endif
    HAL_Delay(500);
    DNS_init(DNS_SOCKET, dns_buffer);
    // TODO: log all NET INFO HERE
    return status;
}


/***********************
 * STATIC DEFINITIONS
 ***********************/

static void w5500_cs_low(void) {
    io_set_out(IO_SPI_CS_W5500, LOW);
}

static void w5500_cs_high(void) {
    io_set_out(IO_SPI_CS_W5500, HIGH);
}

static uint8_t w5500_spi_read_byte(void) {
    uint8_t byte = 0x00;
    spi_receive(&byte, 1);
    return byte;
}

static void w5500_spi_write_byte(uint8_t data) {
    spi_transmit(&data, 1);
}

static void w5500_spi_burst_read(uint8_t *data, uint16_t len) {
    spi_receive(data, (uint32_t) len);
}

static void w5500_spi_burst_write(uint8_t *data, uint16_t len) {
    spi_transmit(data, (uint32_t) len);
}

static void w5500_dhcp_ip_set(void) {
    dhcp_ip_assigned = IP_ASSIGNED;
}

static void w5500_dhcp_ip_not_set(void) {
    dhcp_ip_assigned = IP_UNASSIGNED;
}
