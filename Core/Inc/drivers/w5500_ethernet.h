#ifndef _W5500_DRIVER_H
#define _W5500_DRIVER_H

#include "./log.h"

typedef enum {
    W5500_OK,
    W5500_ERR
} w5500_status_e;

/**
 * @brief Initializes the W5500 ethernet module
 * 
 * This API configures the W5500 settings, e.g. enables/disables
 * DHCP for dynamic IP selection and registers spi chip select functions 
 * and read and write functions. Additionally, 
 * 
 */
uint8_t w5500_init(void);


/**
 * @brief Resolves host IP from hostname via DNS query
 */
int8_t w5500_resolve_hostname(unsigned char *hostname, uint8_t *host_ip);

#endif

