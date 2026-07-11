/**
 * @author Hector Jardines
 * 
 * This module implements HTTPS client functionality on STM32 
 * controller node. The controller node establishes and HTTPS 
 * connection the specified telegram bot API and sends/receives 
 * notifications and commands to and from storage unit nodes. 
 * The module utilizes DHCP for dynamic IP address assignment, DNS
 * for address resolution, and TLS1.2 for secure communication provided 
 * by the mbedTLS library
 */

#ifndef _CLIENT_H
#define _CLIENT_H

#include "../drivers/w5500_ethernet.h"
#include "SSLInterface.h"

#define IPv4_ADDR_LEN           (4U)
#define MAX_HOST_NAME_LEN       (17U)
#define API_TOKEN_LEN           (47U)

#define MAX_CLIENT_MSG_LEN      (64U)
#define MAX_HTTPS_BODY_LEN      (128U)
#define MAX_HTTPS_REQ_LEN       (256U)
#define MAX_HTTPS_OUTPUT_LEN    (1024U)

#define HTTPS_SERVER_PORT       (443U)
#define TLS_SOCK_NUM            (0U)



typedef struct {
    uint8_t https_req[MAX_HTTPS_REQ_LEN];
    uint8_t out_buf[MAX_HTTPS_OUTPUT_LEN];
    uint8_t message[MAX_CLIENT_MSG_LEN];
    uint8_t host_ip[IPv4_ADDR_LEN];
    uint8_t host_name[MAX_HOST_NAME_LEN];
    uint8_t token[API_TOKEN_LEN];               /* TELEGRAM BOT API TOKEN */

    uint8_t conn_status;
    uint8_t msgs_avail;                 /* NUMBER OF MSGS AVAILABLE FOR READ */
    uint8_t msgs_pending;               /* NUMBER OF MSGS PENDING SEND */
    uint8_t sock_num;                   /* W5500 SOCKET NUMBER IN USE */
    uint16_t server_port;
    uint32_t chat_id_l;                 /* LOWER 32 BITS OF CHAT ID */
    uint32_t chat_id_h;                 /* UPPER 32 BITS OF CHAT ID */
    uint32_t update_id;                 /* CURRENT UPDATE ID VALUE USED AS OFFSET IN MSG RETRIEVAL */

    wiz_tls_context tls_context;
    wiz_NetInfo ethernet_context;
} client_context_t;



typedef enum {
    CLIENT_OK,
    CLIENT_ERR,
    CLIENT_IDLE
} client_status_e;



/**
 * @brief Initialize the underlying Ethernet peripheral, DHCP, DNS, and TLS protocols
 * 
 * 
 */
uint8_t client_init(void);



/**
 * @brief Establish a connection to the Telegram bot API
 * 
 * 
 * @param server domain name of the server to connect to
 */
uint8_t client_connect(void);



/**
 * @brief Send to server
 * 
 * 
 * @param
 * @param
 * @return 0 on success; else 1
 */
uint8_t client_send(void);



/**
 * @brief POST an HTTPS (post) request body into the client's message queue
 * 
 * 
 * 
 * @param msg
 * @param len
 * @return 0 on success; else 1
 */
uint8_t client_post_message(uint8_t *msg, uint16_t len);


/**
 * @brief Receive from server 
 * 
 * 
 * 
 * @param 
 * @param 
 * @return 0 on success; else 1
 */
uint8_t client_receive(void);



/**
 * @brief GETs HTTPS body text if any has been received
 * 
 * 
 * @param
 * @param
 * 
 * @return 0 if success i.e. message has been receieve and buffered recently; else 1
 * no messages have been received recently, internal message buffer is empty
 */
uint8_t client_retrieve_message(uint8_t *msg, uint16_t *len);




/**
 * @brief Returns whether client has conencted to server or not...
 * 
 * 
 */
uint8_t client_connected(void);

#endif /* _CLIENT_H */