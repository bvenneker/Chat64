#ifndef WIFI_CORE_H
#define WIFI_CORE_H

#include "common.h"

#include <freertos/message_buffer.h>


// shared variables between wifi core and app core

extern String regID;               // String variale for your regID (leave it empty!)
extern String macaddress;          // variable for the mac address (leave it empty!)
extern String myNickName;          // variable for your nickname (leave it empty!)
extern String ServerConnectResult;
extern byte ResultColor;
extern int pmCount;               // counter for the number of unread private messages
extern String pmSender;           // name of the personal message sender
extern String ssid; 
extern String password;
extern String timeoffset;
extern String server;
extern String myLocalIp;
extern volatile unsigned long lastprivmsg;
extern String msgtype;
extern String users;
extern volatile bool updateUserlist;
extern char msgbuffer[500];
extern volatile int msgbuffersize;
extern volatile int haveMessage;
extern volatile bool getMessage;
extern String userPages[8];
extern String romVersion;
extern String newVersions;
extern char multiMessageBufferPub[3500];
extern char multiMessageBufferPriv[3500];
extern volatile unsigned long messageIds[];
extern volatile unsigned long tempMessageIds[];
extern MessageBufferHandle_t commandBuffer;
extern MessageBufferHandle_t responseBuffer;
extern bool isWifiCoreConnected;
extern volatile bool pastMatrix;
extern volatile bool sendingMessage;
extern String configured; 

// list of wifi commands from app core to wifi core

#define WiFiBeginCommand 1
#define ConnectivityCheckCommand 2
#define GetRegistrationStatusCommand 3
#define SendMessageToServerCommand 4
#define GetWiFiMacAddressCommand 5
//#define GetWiFiLocalIpCommand 6
#define DoUpdateCommand 7

struct WiFiCommandMessage{
    byte command;
    union date {
        struct SendMessageToServer {
            char encoded[500];
            char recipientName[20];
            int  retryCount;
        } sendMessageToServer;
    } data;
};

// generic response from wifi core to app core

struct WiFiResponseMessage{
    byte command;
    union response {
        bool boolean;
        char str[20];
    } response;
};

void WifiCoreLoop(void* parameter);

#endif //WIFI_CORE_H
