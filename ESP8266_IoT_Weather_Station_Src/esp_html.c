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
 * esp_html.c
 *
 * Created on  : Dec 27, 2017
 * Author      : Vinay Divakar
 * Description : This consists of functions for managing the HTML page, preparing
                 and sending the http response message.
 * Website     : www.deeplyembedded.org
 */

#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"

/* Custom Libs */
#include "esp_html.h"
#include "esp8266_dht11.h"

/* Externs - If you can re-architect the app 
   to get rid of this, that would be great */
extern DHT11_T dht11_obj;

/* Static Global Variables */
static char* http_message = "<!DOCTYPE html>\r\n<html><h1>IoT-Weather Station</h1>"
                            "<h2>www.deeplyembedded.org</h2>"
                            "<p>\r\nTemperature = %uC\r\n<br>"
                            "Humidity           = %u%\r\n</p>"
                            "</html>\r\n";

/* Static Functions */
static char* ICACHE_FLASH_ATTR format_html_str(char* html_str,
    unsigned char temp, unsigned char rh);

/****************************************************************
 * Function Name : format_html_str
 * Description   : Prepares the html message.
 * Returns       : Pointer to the html message array else NULL.
 * Params        : @html_str - Pointer to the html message
 *                 @temp     - Temperature data
 *                 @rh       - Humidity data
 ****************************************************************/
char* ICACHE_FLASH_ATTR
format_html_str(char* html_str,
    unsigned char temp, unsigned char rh)
{
    char* buffer = NULL;
    int ret = -1, html_len = 0x00;

    /* Prepare the http message */
    html_len = os_strlen(html_str) + 1;
    buffer = (char*)os_malloc(html_len * sizeof(char));

    if (buffer != NULL)
        ret = os_sprintf(buffer, html_str, temp, rh);
#ifdef HTML_DBG
    os_printf("(format_html_str):ret      = %d\r\n", ret);
    os_printf("(format_html_str):html_len = %d\r\n", html_len);
    os_printf("(format_html_str): %s\r\n", buffer);
#endif

    if ((ret >= 0) && (ret <= html_len))
        return (buffer);
    else
        return (NULL);
}

/****************************************************************
 * Function Name : http_response
 * Description   : Prepares the html response message and 
                   sends it on request.
 * Returns       : NONE.
 * Params        : @espconn  - Pointer to the espcon object
 *                 @error    - Error code
 * REF           : This function is a slight modification of the
                   actual function stolen from the below link:
 * REF LINK: https://esp8266.vn/nonos-sdk/http-server/http-server/
 
 ****************************************************************/
void ICACHE_FLASH_ATTR
http_response(struct espconn* pespconn, int error)
{
    char *buffer = NULL, *html_msg_ptr = NULL;
    int html_length = 0;

    html_msg_ptr = format_html_str(http_message,
        dht11_obj.temperature, dht11_obj.rh);

    buffer = (char*)os_malloc(256 * sizeof(char));
    if (buffer != NULL) {
        if (html_msg_ptr != NULL) {
            html_length = os_strlen(html_msg_ptr);
        }
        else {
            html_length = 0;
        }

        os_sprintf(buffer, "HTTP/1.1 %d OK\r\n"
                           "Content-Length: %d\r\n"
                           "Content-Type: text/html\r\n"
                           "Connection: Closed\r\n"
                           "Refresh: 15\r\n"
                           "\r\n",
            error, html_length);

        if (html_length > 0) {
            buffer = (char*)os_realloc(buffer, (256 + html_length) * sizeof(char));
            os_strcat(buffer, html_msg_ptr);
        }
#ifdef HTML_DBG
        os_printf("%s\r\n", buffer);
#endif
        espconn_sent(pespconn, buffer, strlen(buffer));

        /* Free the memory after use */
        os_free(buffer);
        os_free(html_msg_ptr);
    }
}
