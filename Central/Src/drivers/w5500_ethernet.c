#include "../../Inc/drivers/spi.h"
#include "../../Inc/drivers/io.h"

#include "../../Inc/drivers/w5500_ethernet.h"
#include "../../../Drivers/w5500_eth/DHCP/dhcp.h"
#include "../../../Drivers/w5500_eth/DNS/dns.h"
#include "../../Inc/common/printf-stdarg.h"
#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"

#define DHCP_DISABLED   (0U)
#define DHCP_ENABLED    (1U)
#define IP_UNASSIGNED   (0U)
#define IP_ASSIGNED     (1U)
#define DHCP_BUFFER_LEN_BYTES   (548U)
#define DHCP_SOCKET (7U)
#define DNS_SOCKET  (6U)
#define TX_SOCK_SZ  (2U)
#define RX_SOCK_SZ  (2U)

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
static uint8_t initialized = 0;
uint8_t w5500_init(void) {
    spi_init();

    // register chip select callback functions
    reg_wizchip_cs_cbfunc(w5500_cs_low, w5500_cs_high);

    // register spi read/write byte/burst callback functions
    reg_wizchip_spi_cbfunc(w5500_spi_read_byte, w5500_spi_write_byte);
    reg_wizchip_spiburst_cbfunc(w5500_spi_burst_read, w5500_spi_burst_write);
    reg_wizchip_cris_cbfunc(vPortEnterCritical, vPortExitCritical);

    initialized = 1;
}

uint8_t w5500_configure(void) {
    // reset wizchip
    ctlwizchip(CW_RESET_WIZCHIP, NULL);
    
    w5500_status_e status = W5500_OK;
    // used to set the TX and RX socket sizes
    uint8_t w5500_mem_size[2][8] = {{2,2,2,2,2,2,2,2}, {2,2,2,2,2,2,2,2}};

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
    // set local mac address
    setSHAR(net_info.mac);
    DHCP_init(DHCP_SOCKET, dhcp_buffer);

    retries = 20;
    do {
        DHCP_run();
    } while(retries-- && !dhcp_ip_assigned);

    // use static IP if DHCP failed to assign addr
    if (!dhcp_ip_assigned) {
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
    printf("IP: %d.%d.%d.%d\r\n", net_info.ip[0],  net_info.ip[1],  net_info.ip[2],  net_info.ip[3]);
    printf("GATEWAY: %d.%d.%d.%d\r\n", net_info.gw[0],  net_info.gw[1],  net_info.gw[2],  net_info.gw[3]);
    printf("DNS: %d.%d.%d.%d\r\n", net_info.dns[0],  net_info.dns[1],  net_info.dns[2],  net_info.dns[3]);
    printf("SUBNET: %d.%d.%d.%d\r\n", net_info.sn[0],  net_info.sn[1],  net_info.sn[2],  net_info.sn[3]);

    if (status == W5500_OK)
        initialized = 1;
    return status;
}

/**
 * @brief Resolves host IP from host domain name
 * 
 * @param hostname
 * @param host_ip
 * 
 * @return 1 on error; 0 on success
 */
uint8_t w5500_resolve_hostname(unsigned char *hostname, uint8_t *host_ip) {
    int8_t ret = 1;
    if (initialized)
        ret = DNS_run(net_info.dns, hostname, host_ip);    
    if (ret <= 0)
        ret = 0;

    return !ret;
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
    spi_receive(DEV_ETH, &byte, 1);
    return byte;
}

static void w5500_spi_write_byte(uint8_t data) {
    spi_transmit(DEV_ETH, &data, 1);
}

static void w5500_spi_burst_read(uint8_t *data, uint16_t len) {
    spi_receive_dma(DEV_ETH, data, (uint32_t)len);
}

static void w5500_spi_burst_write(uint8_t *data, uint16_t len) {
    spi_transmit_dma(DEV_ETH, data, (uint32_t)len);
}

static void w5500_dhcp_ip_set(void) {
    dhcp_ip_assigned = IP_ASSIGNED;
}

static void w5500_dhcp_ip_not_set(void) {
    dhcp_ip_assigned = IP_UNASSIGNED;
}
