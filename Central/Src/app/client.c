/**
 * HTTPS Client implementation for Telegram Bot API
 */

#include "../../../FreeRTOS_WrkSpace/include/FreeRTOS.h"
#include "../../../FreeRTOS_WrkSpace/include/task.h"
#include "../../../FreeRTOS_WrkSpace/include/queue.h"

#include "../../Inc/app/client.h"
#include "../../Inc/common/private.h"
#include "string.h"
#include "mbedtls/ssl.h"
#include "../../Inc/common/printf-stdarg.h"
// #include <stdio.h>


#define TELEGRAM_HOST_NAME  ("api.telegram.org")
#define CHAT_ID             (8807953801ULL)
#define MAX_QUEUE_LEN       (5U)
#define MAX_HTTPS_PKT_LEN   (1024U)

#define CLI_STACK_DEPTH         (2048U) // 2048 units not bytes
#define CLI_TASK_PRIO           (3U)
#define CLI_POST_REQ_TIMEOUT    (pdMS_TO_TICKS(25))
#define CLI_GET_REQ_PERIOD      (pdMS_TO_TICKS(50))

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
static uint8_t tls_parse_data(net_msg_t *msg);
static uint8_t client_disconnect(void);
static uint8_t client_connect(void);
static uint8_t client_receive(net_msg_t *msg);
static uint8_t client_send(net_msg_t *msg);


// STATIC_RING_BUFFER(post_req_q, MAX_QUEUE_LEN, net_msg_t);
// STATIC_RING_BUFFER(get_req_q, MAX_QUEUE_LEN, net_msg_t);
static client_context_t client;
static tls_members_t tls_info;

static void task_client(void *arg);
static QueueHandle_t request_q;
static StaticQueue_t _request_q;
static uint8_t request_q_buf[MAX_QUEUE_LEN * sizeof(net_msg_t)];

static TaskHandle_t cli_tsk;
static StaticTask_t _cli_tsk;
static StackType_t cli_tsk_stk[CLI_STACK_DEPTH];
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

    // HTTPS CONNECTION INFORMATION
    memcpy((void *)&client.token, (void *)API_TOKEN, sizeof(client.token));
    memcpy((void *)&client.host_name, (void *)TELEGRAM_HOST_NAME, strlen((const char *)TELEGRAM_HOST_NAME) + 1);
    client.server_port = HTTPS_SERVER_PORT;
    client.sock_num = TLS_SOCK_NUM;
    client.chat_id_h = (uint32_t)(CHAT_ID / 1000000000ULL);
    client.chat_id_l = (uint32_t)(CHAT_ID % 1000000000ULL);
    client.flags = 0x00;
    client.update_id = 0;

    // INITIALIZE TLS CONTEXT MEMBER STRUCT POINTERS
    client.tls_context.cacert = &tls_info.cacert;
    client.tls_context.conf = &tls_info.conf;
    client.tls_context.ctr_drbg = &tls_info.ctr_drbg;
    client.tls_context.ssl = &tls_info.ssl;

    request_q = xQueueCreateStatic(MAX_QUEUE_LEN, sizeof(net_msg_t), request_q_buf, &_request_q);
    cli_tsk = xTaskCreateStatic(task_client, "Cli Task", CLI_STACK_DEPTH,
                                    NULL, CLI_TASK_PRIO, cli_tsk_stk, &_cli_tsk);
    if (cli_tsk == NULL) {
        while (1) {}
    }
}



uint8_t client_post_message(uint8_t *msg, uint16_t len) {
    uint8_t status = CLIENT_OK;
    net_msg_t client_msg;

    // we'll go with the approach of dropping messages that are taking long to be processed (avoid blocking)
    memcpy((void *) client_msg.msg_body, (void *)msg, len);
    client_msg.msg_len = len;
    status = !xQueueSendToBack(request_q, &client_msg, CLI_POST_REQ_TIMEOUT);

    return status;
}


/**
 * @brief Checks whether the client is connected
 * 
 * Since read and writes can trigger disconnect events, 
 * the read/write function set flags to indicate those 
 * disconnect events. If any are set, this function will 
 * disconnect and return the updated status
 *  
 * @return 1 if connected; else 0
 */
uint8_t client_connected(void) {
    if ((client.flags & (CLI_CONN_ERR_Msk | CLI_PCN_Msk)))
        client_disconnect();
    return client.flags & CLI_CONN_STAT_Msk;
}


/**
 * @brief Checks if any messages are pending to be sent
 * 
 * 
 */
uint8_t client_messages_pending(void) {
    // return client.msgs_pending;
    return 0;
}

/***************************
 * STATIC DEFINITIONS
 ***************************/

/**
 * @brief Handles send/rcv messages to/from the ethernet controller
 * 
 * 
 * 
 */
static void task_client(void *arg) {
    net_msg_t msg_post;
    net_msg_t msg_get;
    uint8_t ret = 0;

    TickType_t prev_getreq_tick = 0;
    TickType_t curr_tick = 0;


    ret = w5500_configure();
    // if (ret)
    //     LOG_ERR("FAILED TO INIT W5500 ETH MODULE\n\r");
    ret = wiz_tls_init(&client.tls_context, &client.sock_num);
    // if (ret)
    //     LOG_ERR("FAILED TO INIT TLS MODULE\n\r");
    // task body
    for (;;) {
        while (!client_connected())
            client_connect();
        
        if (xQueueReceive(request_q, &msg_post, CLI_POST_REQ_TIMEOUT) == pdTRUE)
            ret = client_send(&msg_post);

        curr_tick = xTaskGetTickCount();
        if (curr_tick - prev_getreq_tick >= CLI_GET_REQ_PERIOD) {
            ret = client_receive(&msg_get);
            if (ret == 0) {
                // SEND MESSAGE FOR PROCESSING
            }
            prev_getreq_tick = curr_tick;
        }   
    }
}



/**
 * @brief Establishes an HTTPS connection with telegram API server
 * 
 * DNS is used to resolve the host IP address from the Host name. A TCP
 * connection is established over TLS version 1.2
 * 
 * @return 0 on success; else 1
 */
static uint8_t client_connect(void) {
    uint8_t res = w5500_resolve_hostname(client.host_name, client.host_ip);

    if (res == CLIENT_OK) {
        res = wiz_tls_connect(&client.tls_context, client.server_port, client.host_ip);
        if (res == CLIENT_OK)
            client.flags |= CLI_CONN_STAT_Msk; // SET CONN STATUS OK
    }

    return res;
}



/**
 * @brief closes the connection with the client
 * 
 * 
 * The client connection may need to be closed for a number 
 * of reason. Most notable are the peer close notify and error 
 * events. In the former the node should send a close_notify 
 * message before closing the connection, in the latter
 * simply close connection and free resource and reconnect
 * 
 *
 */
static uint8_t client_disconnect(void) {
    int32_t status = 0;
    if (client.flags & CLI_PCN_Msk)
        status = wiz_tls_close_notify(client.tls_context.ssl);
    
    close(client.sock_num);
    status = mbedtls_ssl_session_reset(client.tls_context.ssl);
    client.flags = 0; // RESETS ALL CLIENT FLAGS

    return status;
}


static uint8_t client_send(net_msg_t *msg) {
    uint8_t status = CLIENT_IDLE;
    // FORMAT HTTPS BODY
    snprintf(client.https_req, sizeof(client.https_req), "{\"chat_id\": %u%09u, \"text\": \"%s\"}",
            client.chat_id_h, client.chat_id_l, msg->msg_body);

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

    return status;
}


static uint8_t client_receive(net_msg_t *msg) {
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
            status = tls_parse_data(msg);
    }

    return status;
}



//  COULD EMPLOY STATE LOGIC WHERE WE BREAK ON MBEDTLS_WANT_WRITE/READ OR WHEN BYTES READ < LEN
//  THIS WOULD ALLOW US TO AVOID BLOCKING IN THE CASE THAT MANY CALLS TO WIZ_TLS_WRITE ARE MADE
static int32_t tls_send_data(void) {
    uint16_t len = 0;
    int32_t status = CLIENT_OK;

    printf(" > WRITE TO SERVER:");

    while ((status = wiz_tls_write(&client.tls_context, client.out_buf, strlen((const char *)client.out_buf))) <= 0) {
        if (status != MBEDTLS_ERR_SSL_WANT_READ && status != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf(" failed \n ! wiz_tls_write returned %d\n\n\r\n", status);
            client.flags |= CLI_CONN_ERR_Msk;
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

        if (status == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) { // peer will send no more bytes need to reset context
            client.flags |= CLI_PCN_Msk;
            break;
        }

        if (status < 0) {// no bytes read; some other error (there are a few errors where we simply need to retry later)
            printf(" failed \n ! wiz_tls_read returned %d\n\n\r\n", status);
            break;
        }

        if (status == 0) { // connection error; requires that we reset the connection
            client.flags |= CLI_CONN_ERR_Msk;
            break;
        }

        active_pkt.len = status; // bytes read successfully, status indicates how many were read
        printf(" ok\n bytes read: %d\n\n\r\n", active_pkt.len);
        break;
    } while (retries--);

    return status;
}


static uint8_t tls_parse_data(net_msg_t *msg) {
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

            // ring_buffer_push(&get_req_q, (void *)&active_tls_buf);
            // client.msgs_avail = ring_buffer_count(&get_req_q);
            memcpy((void *)msg, (const void *)&active_tls_buf, sizeof(net_msg_t));
        }
    }
    else
        status = CLIENT_IDLE; // NO UPDATES FOUND


    return status;
}
