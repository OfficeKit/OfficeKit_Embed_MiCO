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
#define MQTT_CLIENT_SSL_ENABLE  // ssl


#ifdef MQTT_CLIENT_SSL_ENABLE

char* mqtt_server_ssl_cert_str =
"-----BEGIN CERTIFICATE-----\r\n\
MIIC3jCCAkegAwIBAgIJAIDl+I0p1LMdMA0GCSqGSIb3DQEBCwUAMIGHMQswCQYD\r\n\
VQQGEwJDTjEQMA4GA1UECAwHYmVpamluZzEQMA4GA1UEBwwHYmVpamluZzERMA8G\r\n\
A1UECgwIY29yYWxzZWMxEjAQBgNVBAsMCW9mZmljZWtpdDEOMAwGA1UEAwwFbGl6\r\n\
aGkxHTAbBgkqhkiG9w0BCQEWDmx4eDEyMzNAcXEuY29tMB4XDTE2MDcwNDEwMDM0\r\n\
NloXDTE5MDcwNDEwMDM0NlowgYcxCzAJBgNVBAYTAkNOMRAwDgYDVQQIDAdiZWlq\r\n\
aW5nMRAwDgYDVQQHDAdiZWlqaW5nMREwDwYDVQQKDAhjb3JhbHNlYzESMBAGA1UE\r\n\
CwwJb2ZmaWNla2l0MQ4wDAYDVQQDDAVsaXpoaTEdMBsGCSqGSIb3DQEJARYObHh4\r\n\
MTIzM0BxcS5jb20wgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBANzI0jlT6MEP\r\n\
BMvHxtiDdRV9o+PvYzzDlaOpdS1JB53/x06zWcpnVTQafSA0pqO2wylyvSemqU54\r\n\
mh/3XW902zNNWEYzs7/oB5UijtxLx9HmhnStkCn12PhM1Uqp5QXEvjxaZWXUXJtR\r\n\
n8ue74nRInaJeG/chWtcJ/jEfxJDQr8FAgMBAAGjUDBOMB0GA1UdDgQWBBSuJawo\r\n\
xCWR48J+TVF8NLmzhXFu8zAfBgNVHSMEGDAWgBSuJawoxCWR48J+TVF8NLmzhXFu\r\n\
8zAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBCwUAA4GBADG1qy7oPp3vmp7CdLA+\r\n\
lfaEldvUf9SWMFA3lqomxtxFS8hkuTXV27U7Tv92qaHGs5X1hngX7wFSTVlLr7G7\r\n\
lCWh9Bj5m/pxBpEHhjAbc8npENQyA5f3lGGrq7YK78WaGyXYBu0lWlPVwY2eXfM+\r\n\
YSnfZddabf0rFHxlp7wXSUDh\r\n\
-----END CERTIFICATE-----";

#define MQTT_SERVER_PORT        8883

#else

#define MQTT_SERVER_PORT        1883

#endif // MQTT_CLIENT_SSL_ENABLE

#define MQTT_SERVER             "121.42.201.80"


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