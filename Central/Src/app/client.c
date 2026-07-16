/**
 * HTTPS Client implementation for Telegram Bot API
 */

#include "../../Inc/app/client.h"
#include "private.h"
#include "string.h"
#include "ring_buffer.h"
#include "mbedtls/ssl.h"
#include <stdio.h>


#define TELEGRAM_HOST_NAME  ("api.telegram.org")
#define CHAT_ID             (8807953801ULL)
#define MAX_QUEUE_LEN       (10U)
#define MAX_HTTPS_PKT_LEN   (1024U)

typedef struct {
    uint8_t msg_body[MAX_HTTPS_BODY_LEN];
    uint16_t msg_len;
} net_msg_t;

typedef struct {
    uint8_t buf[MAX_HTTPS_PKT_LEN];
    uint16_t len;
} https_pkt_t;

static net_msg_t active_tls_buf;
static https_pkt_t active_pkt;
/************************
 * STATIC DECLARATIONS
 ************************/
static int32_t tls_send_data(void);
static int32_t tls_read_data(void);
static uint8_t tls_parse_data(void);


STATIC_RING_BUFFER(post_req_q, MAX_QUEUE_LEN, net_msg_t);
STATIC_RING_BUFFER(get_req_q, MAX_QUEUE_LEN, net_msg_t);
static client_context_t client;
/***********************
 * PUBLIC APIs
 ***********************/

/**
 * @brief Initialize w5500 module and TLS configurations
 * 
 * 
 * 
 * @return 0 on success; 1 else
 */
uint8_t client_init(void) {
    client_status_e res = CLIENT_OK;
    w5500_init();

    memcpy((void *)&client.token, (void *)API_TOKEN, sizeof(client.token));
    memcpy((void *)&client.host_name, (void *)TELEGRAM_HOST_NAME, strlen((const char *)TELEGRAM_HOST_NAME) + 1);
    client.server_port = HTTPS_SERVER_PORT;
    client.sock_num = TLS_SOCK_NUM;
    client.chat_id_h = (uint32_t)(CHAT_ID / 1000000000ULL);
    client.chat_id_l = (uint32_t)(CHAT_ID % 1000000000ULL);
    client.conn_status = 0;
    client.msgs_avail = 0;
    client.msgs_pending = 0;
    client.update_id = 0;

    res = w5500_configure();
    if (res == CLIENT_OK)
        res = wiz_tls_init(&client.tls_context, &client.sock_num);
    return res;
}


/**
 * @brief Establishes an HTTPS connection with telegram API server
 * 
 * DNS is used to resolve the host IP address from the Host name. A TCP
 * connection is established over TLS version 1.2
 * 
 * @return 0 on success; else 1
 */
uint8_t client_connect(void) {
    uint8_t res = w5500_resolve_hostname(&client.host_name, client.host_ip);

    if (res == CLIENT_OK) {
        res = wiz_tls_connect(&client.tls_context, client.server_port, client.host_ip);
        if (res == CLIENT_OK)
            client.conn_status = 1;
    }

    return res;
}



uint8_t client_post_message(uint8_t *msg, uint16_t len) {
    uint8_t status = CLIENT_OK;
    net_msg_t client_msg;
    memset((void *)&client_msg, 0, sizeof(net_msg_t));

    // we'll go with the approach of dropping messages that are taking long to be processed (avoid blocking)
    memcpy((void *) client_msg.msg_body, (void *)msg, len + 1);
    ring_buffer_push(&post_req_q, (void *)&client_msg);
    client.msgs_pending = ring_buffer_count(&post_req_q);

    return status;
}



uint8_t client_retrieve_message(uint8_t *msg, uint16_t *len) {
    uint8_t status = CLIENT_OK;
    if (ring_buffer_empty(&get_req_q))
        status = CLIENT_IDLE;
    
    if (status == CLIENT_OK) {
        net_msg_t server_msg;
        ring_buffer_pop(&get_req_q, (void *)&server_msg);
        memcpy((void *)msg, (void *)server_msg.msg_body, server_msg.msg_len);
        *len = server_msg.msg_len;

        client.msgs_avail = ring_buffer_count(&get_req_q);
    }

    return status;
}



uint8_t client_send(void) {
    uint8_t status = CLIENT_IDLE;
    if (!ring_buffer_empty(&post_req_q)) {
        net_msg_t curr_msg;
        memset((void *)&curr_msg, 0, sizeof(curr_msg));
        ring_buffer_pop(&post_req_q, (void *)&curr_msg);
        client.msgs_pending = ring_buffer_count(&post_req_q); // COULD CHOOSE TO REQUEUE MESSAGES THAT FAIL TO SEND?

        // FORMAT HTTPS BODY
        snprintf(client.https_req, sizeof(client.https_req), "{\"chat_id\": %u%09u, \"text\": \"%s\"}",
                client.chat_id_h, client.chat_id_l, curr_msg.msg_body);

        // FORMAT HTTPS POST REQUEST
        snprintf(client.out_buf, sizeof(client.out_buf), 
                "POST /bot%s/sendMessage HTTP/1.1\r\n"
                "Host: api.telegram.org\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %u\r\n"
                "Connection: keep-alive\r\n"
                "\r\n"
                "%s",
                client.token, (uint32_t)strlen((const char *)client.https_req), client.https_req);


        if ((status = tls_send_data())) {
            status = tls_read_data();
        }
    }
    
    return status;
}


uint8_t client_receive(void) {
    uint8_t status = CLIENT_OK;

    uint8_t json_body[64];
    snprintf(json_body, sizeof(json_body), 
             "{\n"
            "\"offset\": %ld,\n" // OFFSET SHOULD BE THE LAST UPDATE_ID + 1
            "\"limit\": 1,\n" // READ ONLY ONE UPDATE AT A TIME
            "\"timeout\": 0,\n" // MAY CHANGE THIS IF IT DOESN'T ACTUALLY BLOCK 
            "}",
            client.update_id + 1);


    // FORMAT HTTPS GET REQUEST
    snprintf(client.out_buf, sizeof(client.out_buf), 
            "GET /bot%s/getUpdates HTTP/1.1\r\n"
            "Host: api.telegram.org\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %u\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "%s",
            client.token, (uint32_t)strlen((const char *)json_body), json_body);


    if ((status = tls_send_data())) {
        status = tls_read_data();
        if (status > 0)
            tls_parse_data();
    }

    return status;
}


uint8_t client_connected(void) {
    return client.conn_status == 1;
}


/***************************
 * STATIC DEFINITIONS
 ***************************/

//  COULD EMPLOY STATE LOGIC WHERE WE BREAK ON MBEDTLS_WANT_WRITE/READ OR WHEN BYTES READ < LEN
//  THIS WOULD ALLOW US TO AVOID BLOCKING IN THE CASE THAT MANY CALLS TO WIZ_TLS_WRITE ARE MADE
static int32_t tls_send_data(void) {
    uint16_t len = 0;
    int32_t status = CLIENT_OK;

    printf(" > WRITE TO SERVER:");

    while ((status = wiz_tls_write(&client.tls_context, client.out_buf, strlen((const char *)client.out_buf))) <= 0) {
        if (status != MBEDTLS_ERR_SSL_WANT_READ && status != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf(" failed \n ! wiz_tls_write returned %d\n\n\r\n", status);
            status = CLIENT_ERR;
            break;
        }
    }

    return status;
}


// AGAIN HERE WE CAN ADOPT A STATE MACHINE APPROACH, ALSO LOOK INTO ASYNC CB FOR MBEDTLS
static int32_t tls_read_data(void) {
    uint8_t retries = 0xFF;
    int32_t status = 0;

    do {
        memset((void *)active_pkt.buf, 0, sizeof(active_pkt.buf));
        status = wiz_tls_read(&client.tls_context, active_pkt.buf, sizeof(active_pkt.buf));

        if (status == MBEDTLS_ERR_SSL_WANT_READ || status == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue; // more bytes to read

        if (status == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) // peer will send no more bytes
            break;

        if (status < 0) {// no bytes read; some other error (there are a few errors where we simply need to retry later)
            printf(" failed \n ! wiz_tls_read returned %d\n\n\r\n", status);
            break;
        }

        if (status == 0) { // connection error; requires that we reset the connection
            client.conn_status = 0;
            break;
        }

        active_pkt.len = status; // bytes read successfully, status indicates how many were read
        printf(" ok\n bytes read: %d\n\n\r\n", active_pkt.len);
        break;
    } while (retries--);

    return status;
}


static uint8_t tls_parse_data(void) {
    uint8_t *buffer = active_pkt.buf;
    uint8_t status = CLIENT_OK;

    memset((void *)&active_tls_buf, 0, sizeof(active_tls_buf));
    while (*buffer != '\0') { // skip to start of JSON body
        if (*buffer != '{')
            buffer++;
        else {
            buffer++;
            break;
        }
    }

    while (strncmp((const char *)buffer, (const char *)"update_id", strlen((const char*)"update_id")) != 0 
            && *buffer != '\0')
        buffer++;
    
    if (*buffer != '\0') {
        buffer += 11; // length of update_id string (9) + 2 bytes for colon and closing quote
        uint8_t update_id_str[11];
        uint8_t i = 0;
        while (*buffer != ',') {
            update_id_str[i] = *buffer++;
            ++i;
        }
        update_id_str[i] = '\0';
        client.update_id = atoi((const char *)update_id_str);


        while (strncmp((const char *)buffer, (const char *)"text\":", strlen((const char*)"text\":")) != 0 
            && *buffer != '\0')
            buffer++;
        
        if (*buffer != '\0') {
            buffer += 7; // skip quotes and colon to text...
            i = 0;
            while (*buffer != '"') {
                active_tls_buf.msg_body[i] = *buffer++;
                ++i;
            }
            active_tls_buf.msg_body[i++] = '\0';
            active_tls_buf.msg_len = i;

            ring_buffer_push(&get_req_q, (void *)&active_tls_buf);
            client.msgs_avail = ring_buffer_count(&get_req_q);
        }
    }
    else
        status = CLIENT_IDLE; // NO UPDATES FOUND


    return status;
}
