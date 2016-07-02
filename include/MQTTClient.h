/**
******************************************************************************
* @file    MQTTClient.h 
* @author  Eshen Wang
* @version V1.0.0
* @date    31-May-2016
* @brief   This file provide APIs for MiCO mqtt client library.
******************************************************************************
*
*  The MIT License
*  Copyright (c) 2015 MXCHIP Inc.
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy 
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights 
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is furnished
*  to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
*  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR 
*  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************
*/

#ifndef __MQTT_CLIENT_H_
#define __MQTT_CLIENT_H_

// lib version 
#define MQTT_MAIN_VERSION       (0x00)
#define MQTT_SUB_VERSION        (0x01)
#define MQTT_REV_VERSION        (0x05)
#define MQTT_LIB_VERSION        ((MQTT_MAIN_VERSION << 16) | (MQTT_SUB_VERSION << 8 ) | (MQTT_REV_VERSION))

#define MAX_PACKET_ID   (65535)
#define MAX_MESSAGE_HANDLERS    (5)
#define DEFAULT_READBUF_SIZE  (512)
#define DEFAULT_SENDBUF_SIZE  (512)
#define MAX_SIZE_CLIENT_ID  (23+1)
#define MAX_SIZE_USERNAME  (12+1)
#define MAX_SIZE_PASSWORD  (12+1)

typedef struct Timer Timer;
struct Timer {
  unsigned long systick_period;
  unsigned long end_time;
  bool over_flow;
};

typedef struct Network Network;
struct Network
{
  int my_socket;
  int (*mqttread) (Network*, unsigned char*, int, int);
  int (*mqttwrite) (Network*, unsigned char*, int, int);
  void (*disconnect) (Network*);
  void *ssl;
  uint16_t ssl_flag;  // bit0: ssl_enable, bit1: ssl_debug_enable, bit2~4: ssl_version
};

/*
enum {
  SSL_V3_MODE = 1,
  TLS_V1_0_MODE = 2,
  TLS_V1_1_MODE = 3,
  TLS_V1_2_MODE = 4,
};
*/

typedef struct _ssl_opts_t {
  bool ssl_enable;
  bool ssl_debug_enable;
  int ssl_version;  // SSL_V3_MODE=1, TLS_V1_0_MODE=2, TLS_V1_1_MODE=3, TLS_V1_2_MODE=4
  int ca_str_len;  // CA string len
  char* ca_str;  // CA string
} ssl_opts;

// all failure return codes must be negative
enum returnCode { MQTT_SOCKET_ERR = -3, MQTT_BUFFER_OVERFLOW = -2, MQTT_FAILURE = -1, MQTT_SUCCESS = 0 };
enum QoS { QOS0, QOS1, QOS2 };

typedef struct
{
	int len;
	char* data;
} MQTTLenString;

typedef struct
{
	char* cstring;
	MQTTLenString lenstring;
} MQTTString;

struct MQTTMessage
{
    enum QoS qos;
    char retained;
    char dup;
    unsigned short id;
    void *payload;
    size_t payloadlen;
};
typedef struct MQTTMessage MQTTMessage;

struct MessageData
{
    MQTTMessage* message;
    MQTTString* topicName;
};
typedef struct MessageData MessageData;

typedef void (*messageHandler)(MessageData*);

/**
 * Defines the MQTT "Last Will and Testament" (LWT) settings for
 * the connect packet.
 */
typedef struct
{
	/** The eyecatcher for this structure.  must be MQTW. */
	char struct_id[4];
	/** The version number of this structure.  Must be 0 */
	int struct_version;
	/** The LWT topic to which the LWT message will be published. */
	MQTTString topicName;
	/** The LWT payload. */
	MQTTString message;
	/**
      * The retained flag for the LWT message (see MQTTAsync_message.retained).
      */
	unsigned char retained;
	/**
      * The quality of service setting for the LWT message (see
      * MQTTAsync_message.qos and @ref qos).
      */
	char qos;
} MQTTPacket_willOptions;
#define MQTTPacket_willOptions_initializer { {'M', 'Q', 'T', 'W'}, 0, {NULL, {0, NULL}}, {NULL, {0, NULL}}, 0, 0 }

typedef struct
{
	/** The eyecatcher for this structure.  must be MQTC. */
	char struct_id[4];
	/** The version number of this structure.  Must be 0 */
	int struct_version;
	/** Version of MQTT to be used.  3 = 3.1 4 = 3.1.1
	  */
	unsigned char MQTTVersion;
	MQTTString clientID;
	unsigned short keepAliveInterval;
	unsigned char cleansession;
	unsigned char willFlag;
	MQTTPacket_willOptions will;
	MQTTString username;
	MQTTString password;
} MQTTPacket_connectData;
#define MQTTPacket_connectData_initializer { {'M', 'Q', 'T', 'C'}, 0, 4, {NULL, {0, NULL}}, 60, 1, 0, \
        MQTTPacket_willOptions_initializer, {NULL, {0, NULL}}, {NULL, {0, NULL}} }

// mqtt client object
struct Client {
    unsigned int next_packetid;
    unsigned int command_timeout_ms;
    size_t buf_size, readbuf_size;
    unsigned char *buf;  
    unsigned char *readbuf; 
    unsigned int keepAliveInterval;
    char ping_outstanding;
    int isconnected;
    int heartbeat_retry_max;  // heartbeat max retry cnt
    int heartbeat_retry_cnt;  // heartbeat retry cnt

    struct MessageHandlers
    {
        const char* topicFilter;
        void (*fp) (MessageData*);
    } messageHandlers[MAX_MESSAGE_HANDLERS];      // Message handlers are indexed by subscription topic
    
    void (*defaultMessageHandler) (MessageData*);
    
    Network* ipstack;
    Timer ping_timer;
};
typedef struct Client Client;
#define DefaultClient {0, 0, 0, 0, NULL, NULL, 0, 0, 0}

#define MQTTMessage_publishData_initializer {QOS0, 0, 0, 1, "default_payload", strlen("default_payload") }

//-------------------------------- USER API ------------------------------------
int NewNetwork(Network* n, char* addr, int port, ssl_opts ssl_settings);
int MQTTClientInit(Client*, Network*, unsigned int);
int MQTTClientDeinit(Client*);

int MQTTConnect (Client*, MQTTPacket_connectData*);
int MQTTPublish (Client*, const char*, MQTTMessage*);
int MQTTSubscribe (Client*, const char*, enum QoS, messageHandler);  // NOTE: subtopic memory must not be freed
int MQTTUnsubscribe (Client*, const char*);
int MQTTDisconnect (Client*);
int MQTTYield (Client*, int);
int keepalive(Client* c);

uint32_t MQTTClientLibVersion(void);
//------------------------------------------------------------------------------


#endif
