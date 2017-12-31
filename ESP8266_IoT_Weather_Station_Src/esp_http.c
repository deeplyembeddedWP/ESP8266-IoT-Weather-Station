/*
 * MIT License
Copyright (c) 2017 DeeplyEmbedded

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

/*
 * esp_http.c
 *
 * Created on  : Dec 27, 2017
 * Author      : Vinay Divakar
 * Description : This consists of the sever side related funtionalities that is responsible
                 for interacting with the client and handling the requests.
 * Website     : www.deeplyembedded.org
 * Ref Link    : https://esp8266.vn/nonos-sdk/http-server/http-server/
 */

#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"
#include "esp_http.h"
#include "esp_html.h"
#include "esp8266_dht11.h"

/* # definitions */
#define SERVER_LOCAL_PORT 8000

/* Static objects */
static os_timer_t check_ip_timer;
static esp_tcp esptcp;
static struct espconn esp_conn;

/* Static Functions */
static void ICACHE_FLASH_ATTR esp_check_got_ip(void);
static void ICACHE_FLASH_ATTR tcp_server_recv_cb(void* arg, char* pursdata,
    unsigned short len);
static void ICACHE_FLASH_ATTR tcp_server_discon_cb(void* arg);
static void ICACHE_FLASH_ATTR tcp_server_recon_cb(void* arg, sint8 err);
static void ICACHE_FLASH_ATTR tcp_server_listen(void* arg);
static void ICACHE_FLASH_ATTR esp_tcp_server_init(unsigned long port);
static void ICACHE_FLASH_ATTR display_esp_ip(unsigned long ip);

/****************************************************************
 * Function Name : esp_check_got_ip
 * Description   : Checks if the ESP has got its IP.
 * Returns       : NONE.
 * Params        : NONE.
 ****************************************************************/
void ICACHE_FLASH_ATTR esp_check_got_ip(void)
{
    bool ret = 0;

    /* struct to store the ip details */
    struct ip_info ipconfig;

    /* disarm the timer */
    os_timer_disarm(&check_ip_timer);

    /* Get the ip info of the esp */
    ret = wifi_get_ip_info(STATION_IF, &ipconfig);

    if (ret == TRUE) {
        /* Check if esp got an ip */
        if ((wifi_station_get_connect_status() == STATION_GOT_IP) && (ipconfig.ip.addr != 0)) {
            os_printf("got ip !!! \r\n");
            display_esp_ip(ipconfig.ip.addr);
            esp_tcp_server_init(SERVER_LOCAL_PORT);
        }
        else if ((wifi_station_get_connect_status() == STATION_WRONG_PASSWORD) || 
                 (wifi_station_get_connect_status() == STATION_NO_AP_FOUND) || 
                 (wifi_station_get_connect_status() == STATION_CONNECT_FAIL)) {
            os_printf("connect fail !!! \r\n");
        }
        else {
            /* Re-arm the timer to check the ip */
            os_timer_setfn(&check_ip_timer, (os_timer_func_t*)esp_check_got_ip, NULL);
            os_timer_arm(&check_ip_timer, 100, 0);
        }
    }
}

/****************************************************************
 * Function Name : esp_ap_connect
 * Description   : Connects ESP to the Access Point/Router
 * Returns       : NONE.
 * Params        : NONE.
 ****************************************************************/
void ICACHE_FLASH_ATTR esp_ap_connect(void)
{
    bool ret = 0;

    /* Wifi configuration */
    unsigned char ssid[32] = AP_SSID;
    unsigned char password[64] = AP_PWD;
    struct station_config ap_details;

    /* We dont need the AP's MAC addr */
    ap_details.bssid_set = 0x00;

    /* Clear memory */
    os_memset(ap_details.ssid, 0x00, 32);
    os_memset(ap_details.password, 0x00, 64);

    /* Save AP details to flash */
    os_memcpy(&ap_details.ssid, ssid, 32);
    os_memcpy(&ap_details.password, password, 64);
    ret = wifi_station_set_config(&ap_details);

    if (ret == TRUE) {
        /* Set a timer to check if the esp got an IP from the AP.
		 * It will check every 100ms until the IP is acquired */
        os_timer_disarm(&check_ip_timer);
        os_timer_setfn(&check_ip_timer, (os_timer_func_t*)esp_check_got_ip, NULL);
        os_timer_arm(&check_ip_timer, 100, 0);
    }
}

/****************************************************************
 * Function Name : tcp_server_recv_cb
 * Description   : Interprets the request and responds to it with
                   the web page. This is a call back i.e. Invoked
                   when ESP receives the request.
 * Returns       : NONE.
 * Params        : @arg      - Pointer to the espcon object
                   @pursdata - Client request message
                   @len      - Not used (possibly the len of the 
                               response message)
 ****************************************************************/
void ICACHE_FLASH_ATTR tcp_server_recv_cb(void* arg, char* pursdata,
    unsigned short len)
{
    char* ptr = NULL;
    struct espconn* esp_conn = arg;

    /* Check for CRLF in the received request */
    ptr = (char*)os_strstr(pursdata, "\r\n");
    ptr[0] = '\0';
    if (os_strcmp(pursdata, "GET / HTTP/1.1") == 0) {
        os_printf("Message Sent !!!\r\n");
        http_response(esp_conn, 200);
    }
}

/****************************************************************
 * Function Name : tcp_server_discon_cb
 * Description   : This is a call back i.e. Invoked
                   when tcp connection is disconnected
 * Returns       : NONE.
 * Params        : @arg      - Pointer to the espcon object
 ****************************************************************/
void tcp_server_discon_cb(void* arg)
{
    os_printf("tcp disconnect succeeded !!!\r\n");
}

/****************************************************************
 * Function Name : tcp_server_recon_cb
 * Description   : This is a callback invoked when tcp connect 
                   fails.
 * Returns       : NONE.
 * Params        : @arg - Pointer to the espcon object
                   @err - Disconnect error code
 ****************************************************************/
void ICACHE_FLASH_ATTR tcp_server_recon_cb(void* arg, sint8 err)
{
    switch (err) {

    /* Not sure if these integers match the respective cases,
It needs to be checked and confirmed. As there exists no
definitions or the app is unable to find them.--------*/
    case 0:
        os_printf("ERROR TIMEOUT[%d]\r\n", err);
        break;
    case 1:
        os_printf("ESPCONN_ABRT[%d]\r\n", err);
        break;
    case 2:
        os_printf("ESPCONN_RST[%d]\r\n", err);
        break;
    case 3:
        os_printf("ESPCONN_CLSD[%d]\r\n", err);
        break;
    case 4:
        os_printf("ESPCONN_CONN[%d]\r\n", err);
        break;
    case 5:
        os_printf("ESPCONN_HANDSHAKE[%d]\r\n", err);
        break;
    case 6:
        os_printf("ESPCONN_PROTO_MSG[%d]\r\n", err);
        break;
    default:
        os_printf("DEFAULT[%d]", err);
        break;
    }
}

/****************************************************************
 * Function Name : tc_server_sent_cb
 * Description   : This is a callback invoked when the http 
                   message has been sent.
 * Returns       : NONE.
 * Params        : @arg - Pointer to the espcon object
 ****************************************************************/
void ICACHE_FLASH_ATTR tc_server_sent_cb(void* arg)
{
    os_printf("tcp sent call back \r\n");
}

/****************************************************************
 * Function Name : tcp_server_listen
 * Description   : This is a callback invoked when the client
                   establishes a tcp connection with the ESP.
 * Returns       : NONE.
 * Params        : @arg - Pointer to the espcon object
 ****************************************************************/
void ICACHE_FLASH_ATTR tcp_server_listen(void* arg)
{
    struct espconn* esp_conn = arg;
    os_printf("tcp_server_listern !!!\r\n");

    /* register tcp connection event call-backs */
    espconn_regist_recvcb(esp_conn, tcp_server_recv_cb);
    espconn_regist_reconcb(esp_conn, tcp_server_recon_cb);
    espconn_regist_disconcb(esp_conn, tcp_server_discon_cb);
    espconn_regist_sentcb(esp_conn, tc_server_sent_cb);
}

/****************************************************************
 * Function Name : tc_server_sent_cb
 * Description   : This is a callback invoked when the http 
                   message has been sent.
 * Returns       : NONE.
 * Params        : @port - TCP Port No.
 ****************************************************************/
void ICACHE_FLASH_ATTR esp_tcp_server_init(unsigned long port)
{
    sint8 ret1 = ESPCONN_ARG, ret2 = ESPCONN_ARG;
    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    esp_conn.proto.tcp->local_port = port;
    ret1 = espconn_regist_connectcb(&esp_conn, tcp_server_listen);

    if (ret1 == 0) {
        ret2 = espconn_accept(&esp_conn);

        if (ret2 == 0)
            os_printf("esp connection accepted[%d] !!! \r\n", ret1);
        else {
            if (ret2 == ESPCONN_MEM)
                os_printf("esp out of memory[%d] !!!\r\n", ret2);
            else if (ret2 == ESPCONN_ISCONN)
                os_printf("esp already connected[%d] !!!\r\n", ret2);
            else
                os_printf("esp connection failed[%d] !!!\r\n", ret2);
        }
    }
}

/****************************************************************
 * Function Name : display_esp_ip
 * Description   : Dsiplays the IP addr of the ESP
 * Returns       : NONE.
 * Params        : @ip - IP addr in raw format.
 ****************************************************************/
void ICACHE_FLASH_ATTR display_esp_ip(unsigned long ip)
{
    unsigned char a = 0x00, b = 0x00, c = 0x00, d = 0x00;
    a = (ip & 0xFF000000) >> 24;
    b = (ip & 0x00FF0000) >> 16;
    c = (ip & 0x0000FF00) >> 8;
    d = ip & 0x000000FF;

    os_printf("ESP-IP = %d.%d.%d.%d\r\n", d, c, b, a);
}
