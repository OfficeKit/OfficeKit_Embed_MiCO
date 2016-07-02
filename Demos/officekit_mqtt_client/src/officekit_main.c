#include "mico.h"
#include "mico_app_define.h"
#include "MQTTClient.h"

#ifdef USE_MiCOKit_EXT
#include "MiCOKit_EXT/micokit_ext.h"
#endif

app_context_t* app_context = NULL;

#define app_log(M, ...) custom_log("APP", M, ##__VA_ARGS__)

#define UART_RECV_TIMEOUT                   500
#define UART_ONE_PACKAGE_LENGTH             1024
#define UART_BUFFER_LENGTH                  2048

static mico_semaphore_t wifi_sem;
/*UART for sensor data recv buffer*/
volatile ring_buffer_t  rx_buffer;
volatile uint8_t        rx_data[UART_BUFFER_LENGTH];

static void mqtt_client_thread(void *arg);
static void user_recv_thread(void *arg);
static void messageArrived(MessageData* md);
static void connectAP( mico_Context_t * const inContext);
static void uartRecv_thread(void *app_ctx);
static void uartSend_thread(void *app_ctx);
static OSStatus mqtt_msg_publish(Client *c, const char* topic, char qos, char retained, 
                         const unsigned char* msg, uint32_t msg_len);

void appNotify_WifiStatusHandler(WiFiEvent status, void* const inContext)
{
  switch (status) {
  case NOTIFY_STATION_UP:
    app_log("Wi-Fi connected.");
    mico_rtos_set_semaphore(&wifi_sem);
    break;
  case NOTIFY_STATION_DOWN:
    app_log("Wi-Fi disconnected.");
    break;
  }
}

/* MICO system callback: Restore default configuration provided by application */
void appRestoreDefault_callback(void * const user_config_data, uint32_t size)
{
  UNUSED_PARAMETER(size);
  application_config_t* appConfig = user_config_data;
  appConfig->configDataVer = CONFIGURATION_VERSION;
}

/* Application entrance */
OSStatus application_start( void *arg )
{
  UNUSED_PARAMETER(arg);
  OSStatus err = kNoErr;
  mico_Context_t* mico_context = NULL;
  mico_uart_config_t uart_config;
  
#ifdef MQTT_CLIENT_SSL_ENABLE
  int mqtt_thread_stack_size = 0x3000;
#else
  int mqtt_thread_stack_size = 0x800;
#endif
  int user_recv_thread_stack_size = 0x800;
  int uart_recv_thread_stack_size = 0x300;
  int uart_send_thread_stack_size = 0x300;

  /* Create application context */
  app_context = ( app_context_t *)calloc(1, sizeof(app_context_t) );
  require_action( app_context, exit, err = kNoMemoryErr );

  /* Create mico system context and read application's config data from flash */
  mico_context = mico_system_context_init( sizeof( application_config_t) );
  app_context->appConfig = mico_system_context_get_user_data( mico_context );
  
  err = mico_rtos_init_semaphore(&wifi_sem, 1);
  require_noerr( err, exit );
  
  /* Register user function for MiCO notification: WiFi status changed */
  err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)appNotify_WifiStatusHandler, NULL );
  require_noerr( err, exit ); 
  
  /* mico system initialize */
  err = mico_system_init( mico_context );
  require_noerr( err, exit );
  connectAP(mico_context);
  /* wait for wifi on */
  mico_rtos_get_semaphore(&wifi_sem, MICO_WAIT_FOREVER);
  
  /* get free memory */
  app_log("Free memory: %d bytes", MicoGetMemoryInfo()->free_memory) ; 
  
  app_context->mqtt_client_connected = false;
  
  /* create msg send/recv queue */
  err = mico_rtos_init_queue(&(app_context->mqtt_msg_recv_queue), "mqtt_msg_recv_queue", 
                             sizeof(p_mqtt_recv_msg_t), MAX_MQTT_RECV_QUEUE_SIZE);
  require_noerr_action( err, exit, app_log("ERROR: create mqtt msg recv queue err=%d.", err) );
  
  err = mico_rtos_init_queue(&(app_context->mqtt_msg_send_queue), "mqtt_msg_send_queue", 
                             sizeof(p_mqtt_send_msg_t), MAX_MQTT_SEND_QUEUE_SIZE);
  require_noerr_action( err, exit, app_log("ERROR: create mqtt msg send queue err=%d.", err) );
    
  /* start mqtt client */
  err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "mqtt_client", 
                                mqtt_client_thread, mqtt_thread_stack_size, app_context );
  require_noerr_string( err, exit, "ERROR: Unable to start the mqtt client thread." );
  
  /* start user recv thread */
  err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "user_recv", 
                                user_recv_thread, user_recv_thread_stack_size, app_context );
  require_noerr_string( err, exit, "ERROR: Unable to start user recv thread." );
  
  /*init UART & start the UART receive thread for get sensor data*/
  uart_config.baud_rate    = 115200;
  uart_config.data_width   = DATA_WIDTH_8BIT;
  uart_config.parity       = NO_PARITY;
  uart_config.stop_bits    = STOP_BITS_1;
  uart_config.flow_control = FLOW_CONTROL_DISABLED;
  if(mico_context->flashContentInRam.micoSystemConfig.mcuPowerSaveEnable == true)
      uart_config.flags = UART_WAKEUP_ENABLE;
  else
      uart_config.flags = UART_WAKEUP_DISABLE;
  ring_buffer_init  ( (ring_buffer_t *)&rx_buffer, (uint8_t *)rx_data, UART_BUFFER_LENGTH );
  MicoUartInitialize( UART_FOR_APP, &uart_config, (ring_buffer_t *)&rx_buffer );
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "uart_recv", uartRecv_thread, uart_recv_thread_stack_size, app_context );
  require_noerr_action( err, exit, app_log("ERROR: Unable to start the uart recv thread.") );
    
  /*Send command to sensor via UART send thread*/
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "uart_send", uartSend_thread, uart_send_thread_stack_size, app_context );
  require_noerr_action( err, exit, app_log("ERROR: Unable to start the uart send thread.") );
  
exit:
  if(kNoErr !=  err){
    app_log("ERROR, app thread exit err: %d", err);
  }
  mico_rtos_delete_thread(NULL);
  return err;
}

static void mqtt_client_thread(void *arg)
{
  OSStatus err = kUnknownErr;
  
  uint32_t mqtt_lib_version = 0;
  int rc = -1;
  fd_set readfds;
  struct timeval_t t = {0, MQTT_YIELD_TMIE*1000};
  
  Client c;  // mqtt client object
  Network n;  // socket network for mqtt client
  ssl_opts ssl_settings;
  MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
  
  app_context_t *app_ctx = (app_context_t*)arg;
  p_mqtt_send_msg_t p_send_msg = NULL;
  int msg_send_event_fd = -1;
  bool no_mqtt_msg_exchange = true;
  
  app_log("MQTT client thread started...");
    
  memset(&c, 0, sizeof(c));
  memset(&n, 0, sizeof(n));

  mqtt_lib_version = MQTTClientLibVersion();
  app_log("MQTT client version: [%d.%d.%d]", 0xFF & (mqtt_lib_version >> 16), 
                                             0xFF & (mqtt_lib_version >> 8), 
                                             0xFF &  mqtt_lib_version);
  
  /* create msg send queue event fd */
  msg_send_event_fd = mico_create_event_fd(app_ctx->mqtt_msg_send_queue);
  require_action(msg_send_event_fd >=0, exit, app_log("ERROR: create msg send queue event fd failed!!!"));

MQTT_start:
  app_context->mqtt_client_connected = false;
  
  /* 1. create network connection */
#ifdef MQTT_CLIENT_SSL_ENABLE
  ssl_settings.ssl_enable = true;
  ssl_settings.ssl_debug_enable = false;  // ssl debug log
  ssl_settings.ssl_version = TLS_V1_1_MODE;
  ssl_settings.ca_str_len = strlen(mqtt_server_ssl_cert_str);
  ssl_settings.ca_str = mqtt_server_ssl_cert_str;
#else
  ssl_settings.ssl_enable = false;
#endif
  
network_reconnect:
  rc = NewNetwork(&n, MQTT_SERVER, MQTT_SERVER_PORT, ssl_settings);
  if(rc < 0){
    app_log("ERROR: MQTT network connection err=%d,reconnect after 3s...", rc);
    mico_thread_sleep(3);
    goto network_reconnect;
  }
  else{
    app_log("MQTT network connection success!");
  }
  
  /* 2. init mqtt client */
  //c.heartbeat_retry_max = 2;
  app_log("MQTT client init...");
  rc = MQTTClientInit(&c, &n, MQTT_CMD_TIMEOUT);
  if(MQTT_SUCCESS != rc){
    app_log("ERROR: MQTT client init err=%d.", rc);
    goto MQTT_disconnect;
  }
  else{
    app_log("MQTT client init success!");
  }
  
  /* 3. create mqtt client connection */
  connectData.willFlag = 0;
  connectData.MQTTVersion = 3;
  connectData.clientID.cstring = MQTT_CLIENT_ID;
  connectData.username.cstring = MQTT_CLIENT_USERNAME;
  connectData.password.cstring = MQTT_CLIENT_PASSWORD;
  connectData.keepAliveInterval = MQTT_CLIENT_KEEPALIVE;
  connectData.cleansession = 1;

  app_log("MQTT client connecting...");
  rc = MQTTConnect(&c, &connectData);
  if(MQTT_SUCCESS == rc){
    app_log("MQTT client connect success!");
  }
  else{
    app_log("ERROR: MQTT client connect err=%d.", rc);
    goto MQTT_disconnect;
  }
  
  /* 4. mqtt client subscribe */
  app_log("MQTT client subscribe...");
  rc = MQTTSubscribe(&c, MQTT_CLIENT_SUB_TOPIC, QOS0 , messageArrived);
  if (MQTT_SUCCESS == rc){
    app_log("MQTT client subscribe success! recv_topic=[%s].", MQTT_CLIENT_SUB_TOPIC);
  }
  else{
    app_log("ERROR: MQTT client subscribe err=%d.", rc);
    goto MQTT_disconnect;
  }
  
  app_context->mqtt_client_connected = true;
  
  /* 5. client loop for recv msg && keepalive */
  while(1){
    app_log("MQTT client running...");
    no_mqtt_msg_exchange = true;
    FD_ZERO(&readfds);
    FD_SET(c.ipstack->my_socket, &readfds);
    FD_SET(msg_send_event_fd, &readfds);
    select(msg_send_event_fd + 1, &readfds, NULL, NULL, &t);
    
    /* recv msg from server */
    if (FD_ISSET( c.ipstack->my_socket, &readfds )){
      rc = MQTTYield(&c, (int)MQTT_YIELD_TMIE);
      if (MQTT_SUCCESS != rc) {
        goto MQTT_disconnect;
      }
      no_mqtt_msg_exchange = false;
    }
    
    /* recv msg from user thread to be sent to server */
    if (FD_ISSET( msg_send_event_fd, &readfds )){
      // get msg from send queue
      err = mico_rtos_pop_from_queue(&(app_ctx->mqtt_msg_send_queue), &p_send_msg, 0);
      if(kNoErr == err){
        if(p_send_msg){
          // send message to server
          err = mqtt_msg_publish(&c, p_send_msg->topic, p_send_msg->qos, p_send_msg->retained, 
                         p_send_msg->data, p_send_msg->datalen);
          // release msg mem resource
          free(p_send_msg);
          if(kNoErr != err){
            app_log("ERROR: MQTT publish data err=%d, send_topic=[%s], msg=[%d][%s].", err,
                    p_send_msg->topic, p_send_msg->datalen, p_send_msg->data);
            goto MQTT_disconnect;
          }
          else{
            app_log("MQTT publish data success! send_topic=[%s], msg=[%d][%s].",
                    p_send_msg->topic, p_send_msg->datalen, p_send_msg->data);
            no_mqtt_msg_exchange = false;
          }
        }
      }
      else{
        app_log("MQTT pop user msg from send queue err=%d.", err);
        p_send_msg = NULL;
      }
    }
    
    /* if no msg exchange, we need to check ping msg to keep alive. */
    if(no_mqtt_msg_exchange){
      rc = keepalive(&c);
      if (MQTT_SUCCESS != rc) {
        app_log("ERROR: keepalive err=%d.", rc);
        goto MQTT_disconnect;
      }
    }

    continue;
    
  MQTT_disconnect:
    app_log("MQTT client disconnected, reconnect after 3s...");
    if(c.isconnected) {MQTTDisconnect(&c);}  // send mqtt disconnect msg
    n.disconnect(&n);  // close connection
    rc = MQTTClientDeinit(&c);  // free mqtt client resource
    if(MQTT_SUCCESS != rc){
      app_log("MQTTClientDeinit failed!");
      err = kDeletedErr;
    }
    mico_thread_sleep(5);
    goto MQTT_start;
  }

exit:
  app_context->mqtt_client_connected = false;
  if(c.isconnected) {MQTTDisconnect(&c);}
  n.disconnect(&n);
  rc = MQTTClientDeinit(&c);
  if(MQTT_SUCCESS != rc){
    app_log("MQTTClientDeinit failed!");
    err = kDeletedErr;
  }

  app_log("EXIT: MQTT client exit with err = %d.", err);
  mico_rtos_delete_thread(NULL);
}

// msg received from mqtt server with callback
static void messageArrived(MessageData* md)
{
  OSStatus err = kUnknownErr;
  p_mqtt_recv_msg_t p_recv_msg = NULL;
  MQTTMessage* message = md->message;
  app_log("MQTT messageArrived callback: [%.*s]\t  [%d][%.*s]",
          md->topicName->lenstring.len, md->topicName->lenstring.data,
          (int)message->payloadlen,
          (int)message->payloadlen, (char*)message->payload);
  
  p_recv_msg = (p_mqtt_recv_msg_t)malloc(sizeof(mqtt_recv_msg_t));
  if(NULL !=  p_recv_msg){
    memset(p_recv_msg, 0, sizeof(mqtt_recv_msg_t));
    strncpy(p_recv_msg->topic, md->topicName->lenstring.data, md->topicName->lenstring.len);
    memcpy(p_recv_msg->data, message->payload, message->payloadlen);
    p_recv_msg->datalen = message->payloadlen;
    p_recv_msg->qos = (char)(message->qos);
    p_recv_msg->retained = message->retained;
    err = mico_rtos_push_to_queue(&(app_context->mqtt_msg_recv_queue), &p_recv_msg, 0);
    if(kNoErr != err){
      app_log("push mqtt recv msg into recv queue err=%d.", err);
      free(p_recv_msg);
      p_recv_msg = NULL;
    }
    else{
      app_log("push mqtt recv msg into recv queue success!");
    }
  }
  else{
    app_log("ERROR: create mqtt recv msg failed!!!");
  }
}

// publish msg to mqtt server
static OSStatus mqtt_msg_publish(Client *c, const char* topic, char qos, char retained, 
                         const unsigned char* msg, uint32_t msg_len)
{
  OSStatus err = kUnknownErr;
  int ret = 0;

  MQTTMessage publishData =  MQTTMessage_publishData_initializer;
  
  if(topic == NULL || msg_len <= 0)
    return kParamErr;
    
  // upload data qos0
  publishData.qos = (enum QoS)qos;
  publishData.retained = retained;
  publishData.payload = (void*)msg;
  publishData.payloadlen = msg_len;
  
  ret = MQTTPublish(c, topic, &publishData);
  
  if (MQTT_SUCCESS == ret){
    err = kNoErr;
  }
  else if(MQTT_SOCKET_ERR == ret){
    err = kConnectionErr;
  }
  else{
    err = kUnknownErr;
  }
  
  return err;
}

static void user_recv_thread(void *arg)
{
  OSStatus err = kUnknownErr;
  app_context_t *app_ctx = (app_context_t*)arg;
  p_mqtt_recv_msg_t p_recv_msg = NULL;
  
  app_log("user recv thread started...");
  
  while(1){
    // get msg from recv queue
    err = mico_rtos_pop_from_queue(&(app_ctx->mqtt_msg_recv_queue), &p_recv_msg, MICO_NEVER_TIMEOUT);
    if(kNoErr == err){
      if(p_recv_msg){
        app_log("user get data success! from_topic=[%s], msg=[%d][%s].",
                p_recv_msg->topic, p_recv_msg->datalen, p_recv_msg->data);
        // release msg mem resource
        free(p_recv_msg);
        p_recv_msg = NULL;
      }
    }
  }
}

void sendMsg(app_context_t *app_ctx, u8* msg, u32 datalen)
{
    OSStatus err = kUnknownErr;
    p_mqtt_send_msg_t p_send_msg = NULL;
    
    if(app_ctx->mqtt_client_connected){
      app_log("user send msg...");
      p_send_msg = (p_mqtt_send_msg_t)malloc(sizeof(mqtt_send_msg_t));
      if(NULL !=  p_send_msg){
        memset(p_send_msg, 0, sizeof(mqtt_send_msg_t));
        strncpy(p_send_msg->topic, MQTT_CLIENT_PUB_TOPIC, strlen(MQTT_CLIENT_PUB_TOPIC));
        p_send_msg->datalen = datalen;
        memcpy(p_send_msg->data, msg, p_send_msg->datalen);
        p_send_msg->qos = 0;
        p_send_msg->retained = 0;
        
        err = mico_rtos_push_to_queue(&(app_ctx->mqtt_msg_send_queue), &p_send_msg, 0);
        if(kNoErr != err){
          app_log("push user msg into send queue err=%d.", err);
          free(p_send_msg);
          p_send_msg = NULL;
        }
        else{
          app_log("push user msg into send queue success!");
        }
      }
      else{
        app_log("ERROR: create user msg failed!!!");
      }
      
      mico_thread_sleep(10);
    }
}

static void connectAP( mico_Context_t * const inContext)
{
  network_InitTypeDef_st wNetConfig;
	  IPStatusTypedef para;
  memset(&wNetConfig, 0x0, sizeof(network_InitTypeDef_st));
  
  mico_rtos_lock_mutex(&inContext->flashContentInRam_mutex);

  //strncpy((char*)wNetConfig.wifi_ssid, inContext->flashContentInRam.micoSystemConfig.ssid, maxSsidLen);
  //strncpy((char*)wNetConfig.wifi_key, inContext->flashContentInRam.micoSystemConfig.user_key, maxKeyLen);
        
  strncpy((char*)wNetConfig.wifi_ssid, "PandoraBox_706DCF", maxSsidLen);
  strncpy((char*)wNetConfig.wifi_key, "jmdlbl88", maxKeyLen);
        
  wNetConfig.dhcpMode = DHCP_Client;
  wNetConfig.wifi_mode = Station;
  micoWlanGetIPStatus(&para, Station);
  
  strncpy((char*)wNetConfig.local_ip_addr, (char *)&para.ip, maxIpLen);
  strncpy((char*)wNetConfig.net_mask, (char *)&para.mask, maxIpLen);
  strncpy((char*)wNetConfig.gateway_ip_addr, (char *)&para.dns, maxIpLen);
  strncpy((char*)wNetConfig.dnsServer_ip_addr, (char *)&para.ip, maxIpLen);
  wNetConfig.wifi_retry_interval = 500;
	
  mico_rtos_unlock_mutex(&inContext->flashContentInRam_mutex);

  micoWlanStart(&wNetConfig);
  ps_enable();
}

size_t _uart_get_one_packet(uint8_t* inBuf, int inBufLen)
{
  uint8_t *p;
  OSStatus err = kNoErr;

  uint8_t datalen;
  
  while(1) {
      p = inBuf;
      err = MicoUartRecv(UART_FOR_APP, p, 1, MICO_WAIT_FOREVER);
      require_noerr(err, exit);
      
      p = p+1;
      if(p[0] == 0x5a){
        require_noerr(err, exit);
      }
      err = MicoUartRecv(UART_FOR_APP, p, 1, 1000);
      require_noerr(err, exit);
      datalen = *p;
      
      p++;   
      err = MicoUartRecv(UART_FOR_APP, p, datalen-2, 1000);
      
      return datalen;   
  }
  exit:
  return -1;
}

static void uartSend_thread(void* app_ctx)
{
  u8 command[5] = {0x5a,0x05,0xa1,0x01,0x5b};
  while(1){
     MicoUartSend(UART_FOR_APP, command, 5);
     mico_thread_msleep(UART_SEND_INTERVAL);
  }
}

static void uartRecv_thread(void *app_ctx)
{
  int recvlen;
  uint8_t *inDataBuffer;
  
  inDataBuffer = malloc(UART_ONE_PACKAGE_LENGTH);
  require(inDataBuffer, exit);
  
  while(1) {
    recvlen = _uart_get_one_packet(inDataBuffer, UART_ONE_PACKAGE_LENGTH);
    if (recvlen <= 0)
      continue; 
    app_log("DATA From UART %s",inDataBuffer);
    //push data to mqtt server
    sendMsg((app_context_t*)app_ctx,inDataBuffer,recvlen);
  }
  
exit:
  if(inDataBuffer) free(inDataBuffer);
}

