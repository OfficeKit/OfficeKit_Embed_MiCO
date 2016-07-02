/**
  ******************************************************************************
  * @file    mico_app_define.h 
  * @author  Eshen Wang
  * @version V1.0.0
  * @date    16-Nov-2015
  * @brief   This file provide constant definition and type declaration for MiCO
  *          applications.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, MXCHIP Inc. SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2014 MXCHIP Inc.</center></h2>
  ******************************************************************************
  */

#pragma once

#include "mico.h"

#ifdef __cplusplus
extern "C" {
#endif

/*User provided configurations*/
#define CONFIGURATION_VERSION               0x20000001 // if default configuration is changed, update this number

#define MQTT_CLIENT_ID          "MiCO_MQTT_Client"
#define MQTT_CLIENT_USERNAME    NULL
#define MQTT_CLIENT_PASSWORD    NULL
#define MQTT_CLIENT_KEEPALIVE   30
#define MQTT_CLIENT_SUB_TOPIC   "mico/test/send"  // loop msg
#define MQTT_CLIENT_PUB_TOPIC   "mico/test/send"
#define MQTT_CMD_TIMEOUT        5000  // 5s
#define MQTT_YIELD_TMIE         5000  // 5s
#define UART_SEND_INTERVAL      20000  //20s
//#define MQTT_CLIENT_SSL_ENABLE  // ssl


#ifdef MQTT_CLIENT_SSL_ENABLE

char* mqtt_server_ssl_cert_str =
"-----BEGIN CERTIFICATE-----\r\n\
MIIC8DCCAlmgAwIBAgIJAOD63PlXjJi8MA0GCSqGSIb3DQEBBQUAMIGQMQswCQYD\r\n\
VQQGEwJHQjEXMBUGA1UECAwOVW5pdGVkIEtpbmdkb20xDjAMBgNVBAcMBURlcmJ5\r\n\
MRIwEAYDVQQKDAlNb3NxdWl0dG8xCzAJBgNVBAsMAkNBMRYwFAYDVQQDDA1tb3Nx\r\n\
dWl0dG8ub3JnMR8wHQYJKoZIhvcNAQkBFhByb2dlckBhdGNob28ub3JnMB4XDTEy\r\n\
MDYyOTIyMTE1OVoXDTIyMDYyNzIyMTE1OVowgZAxCzAJBgNVBAYTAkdCMRcwFQYD\r\n\
VQQIDA5Vbml0ZWQgS2luZ2RvbTEOMAwGA1UEBwwFRGVyYnkxEjAQBgNVBAoMCU1v\r\n\
c3F1aXR0bzELMAkGA1UECwwCQ0ExFjAUBgNVBAMMDW1vc3F1aXR0by5vcmcxHzAd\r\n\
BgkqhkiG9w0BCQEWEHJvZ2VyQGF0Y2hvby5vcmcwgZ8wDQYJKoZIhvcNAQEBBQAD\r\n\
gY0AMIGJAoGBAMYkLmX7SqOT/jJCZoQ1NWdCrr/pq47m3xxyXcI+FLEmwbE3R9vM\r\n\
rE6sRbP2S89pfrCt7iuITXPKycpUcIU0mtcT1OqxGBV2lb6RaOT2gC5pxyGaFJ+h\r\n\
A+GIbdYKO3JprPxSBoRponZJvDGEZuM3N7p3S/lRoi7G5wG5mvUmaE5RAgMBAAGj\r\n\
UDBOMB0GA1UdDgQWBBTad2QneVztIPQzRRGj6ZHKqJTv5jAfBgNVHSMEGDAWgBTa\r\n\
d2QneVztIPQzRRGj6ZHKqJTv5jAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBBQUA\r\n\
A4GBAAqw1rK4NlRUCUBLhEFUQasjP7xfFqlVbE2cRy0Rs4o3KS0JwzQVBwG85xge\r\n\
REyPOFdGdhBY2P1FNRy0MDr6xr+D2ZOwxs63dG1nnAnWZg7qwoLgpZ4fESPD3PkA\r\n\
1ZgKJc2zbSQ9fCPxt2W3mdVav66c6fsb7els2W2Iz7gERJSX\r\n\
-----END CERTIFICATE-----";

#else  // ! MQTT_CLIENT_SSL_ENABLE

#define MQTT_SERVER             "121.42.201.80"
#define MQTT_SERVER_PORT        1883

#endif // MQTT_CLIENT_SSL_ENABLE

#define MAX_MQTT_TOPIC_SIZE  (256)
#define MAX_MQTT_DATA_SIZE    (1024)
#define MAX_MQTT_RECV_QUEUE_SIZE  (5)
#define MAX_MQTT_SEND_QUEUE_SIZE  (5)

/*Application's configuration stores in flash, and loaded to ram when system boots up*/
typedef struct
{
  uint32_t                      configDataVer;

} application_config_t;

typedef struct _app_context_t
{
  /*Flash content*/
  application_config_t         *appConfig;

  /*Running status*/
  mico_queue_t                 mqtt_msg_recv_queue;
  mico_queue_t                 mqtt_msg_send_queue;
  bool                         mqtt_client_connected;
  
} app_context_t;

typedef struct _mqtt_recv_msg_t{
  char topic[MAX_MQTT_TOPIC_SIZE];
  char qos;
  char retained;
  
  uint8_t data[MAX_MQTT_DATA_SIZE];
  uint32_t datalen;
}mqtt_recv_msg_t, *p_mqtt_recv_msg_t, mqtt_send_msg_t, *p_mqtt_send_msg_t;

#ifdef __cplusplus
} /*extern "C" */
#endif