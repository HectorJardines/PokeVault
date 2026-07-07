#ifndef _W5500_DRIVER_H
#define _W5500_DRIVER_H

#include "./log.h"
#include "../../../Drivers/w5500_eth/W5500/w5500.h"

typedef enum {
    W5500_OK,
    W5500_ERR
} w5500_status_e;

/**
 * @brief Initialize the underlying submodules for the W5500 peripheral 
 * 
 * @return 0 on success; 1 else
 */
uint8_t w5500_init(void);


/**
 * @brief Configures the W5500 ethernet module
 * 
 * This API configures the W5500 settings, e.g. enables/disables
 * DHCP for dynamic IP selection and registers spi chip select functions 
 * and read and write functions.
 * 
 */
uint8_t w5500_configure(void);


/**
 * @brief Resolves host IP from hostname via DNS query
 * 
 * 
 * 
 * @return 0 if success; else 1
 */
uint8_t w5500_resolve_hostname(unsigned char *hostname, uint8_t *host_ip);

#endif

