#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/message_buffer.h>
#include <WiFi.h>

#include "common.h"
#include "utils.h"
#include "wifi_core.h"

String regID = "";       // String variale for your regID (leave it empty!)
String macaddress = "";  // variable for the mac address (leave it empty!)
String myNickName = "";  // variable for your nickname (leave it empty!)
String ServerConnectResult = "";
byte ResultColor = 144;
int pmCount = 0;       // counter for the number of unread private messages
String pmSender = "";  // name of the personal message sender

// You do NOT need to change any of these settings!
String ssid = "empty";      // do not change this!
String password = "empty";  // do not change this!
String timeoffset = "empty";
String server = "empty";                 // do not change this!
volatile unsigned long lastmessage = 1;  // do not change this!
volatile unsigned long lastprivmsg = 1;  // do not change this!
volatile unsigned long tempMessageID = 0;
String msgtype = "public";  // do not change this!
String users = "";          // a list of all users on this server.
volatile bool updateUserlist = false;
char msgbuffer[500];  // a character buffer for a chat message
volatile int msgbuffersize = 0;
volatile int haveMessage = 0;
volatile bool getMessage = false;
String userPages[6];
String romVersion = "0.0";
volatile bool fullpage = true;
char fullpagetext[3500];

MessageBufferHandle_t commandBuffer;
MessageBufferHandle_t responseBuffer;
bool isWifiCoreConnected = false;

// ***************************************************************
//   get the list of users from the webserver
// ***************************************************************
void fill_userlist() {
  String serverName = "http://" + server + "/listUsers.php";
  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverName);
  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Prepare your HTTP POST request data
  String httpRequestData = "regid=" + regID + "&call=list";

  // Send HTTP POST request
  int httpResponseCode = http.POST(httpRequestData);
  String result = "0";

  if (httpResponseCode == 200) {
    String new_users = http.getString();
    new_users.trim();
    users = new_users;
#ifdef debug
    Serial.println(users);
#endif
  } else {
    result = "communication error";
  }

  // Free resources
  http.end();
}

// *************************************************
//  void to send a message to the server
// *************************************************
bool SendMessageToServer(String Encoded, String RecipientName, bool heartbeat) {
  String serverName = "http://" + server + "/insertMessage.php";
  WiFiClient client;
  HTTPClient http;
  bool result = false;
  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);

  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Prepare your HTTP POST request data
  String httpRequestData = "";
  if (heartbeat) {
    httpRequestData = "regid=" + regID + "&call=heartbeat";
  } else {
    httpRequestData = "sendername=" + myNickName + "&regid=" + regID + "&recipientname=" + RecipientName + "&message=" + Encoded;
  }

  // Send HTTP POST request
  int httpResponseCode = http.POST(httpRequestData);

  // httpResponseCode should be 200
  if (httpResponseCode > 0) {
    String ret = http.getString();
    ret.trim();
#ifdef debug
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    Serial.print("Return code from php page: ");
    Serial.print(ret);
#endif
    if (ret == "0") {
      // no error on database level
      result = true;
#ifdef debug
      Serial.println(" = no error.");
#endif
    } else {
      // some error on database level or php
      result = false;

#ifdef debug
      Serial.println(" = some error!!!");
#endif
    }
  }
  // Free resources
  http.end();
  return result;
}

// *******************************************************
//  String function to get the userlist from the database
// *******************************************************
String getUserList(int page) {
  String serverName = "http://" + server + "/listUsers.php";
  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverName);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String httpRequestData = "regid=" + regID + "&page=" + page + "&version=2";
  http.POST(httpRequestData);
  String result = "0";
  result = http.getString();
  result.trim();
  http.end();
  return result;
}

void get_full_userlist() {
  // this is for the user list in the menu (Who is on line?)
  // The second core calls this webpage so the main thread does not suffer performance
  for (int p = 0; p < 6; p++) {
    userPages[p] = getUserList(p);
    char firstchar = userPages[p].charAt(0);
    if ((firstchar == 156 or firstchar == 149) == false) userPages[p] = "      ";
  }
}

// ****************************************************
//  char function that returns the registration status
// ****************************************************
char getRegistrationStatus() {

  String serverName = "http://" + server + "/getRegistration.php";
  WiFiClient client;
  HTTPClient http;
  // Connect to configured server
  http.begin(client, serverName);
  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  // Prepare your HTTP POST request data
  String httpRequestData = "macaddress=" + macaddress + "&regid=" + regID + "&nickname=" + myNickName + "&version=" + SwVersion;
  int httpResponseCode = http.POST(httpRequestData);
  char result = 'x';

  if (httpResponseCode == 200) {
    String textOutput = http.getString();
    textOutput.trim();

    if (textOutput == "r200") result = 'r';       // registration and nickname are good.
    else if (textOutput == "r105") result = 'n';  // registration is good but nickname is taken by someone else
    else if (textOutput == "r104") result = 'u';  // registration is not good
  }
  return result;
}

// *************************************************
//  void to check connectivity to the server
// *************************************************
void ConnectivityCheck() {

  String serverName = "http://" + server + "/connectivity.php";
#ifdef debug
  Serial.print("Current configured Server: ");
  Serial.println(server);
#endif
  WiFiClient client;
  HTTPClient http;

  http.begin(client, serverName);                                       // Connect to configured server
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");  // Specify content-type header
                                                                        // Prepare your HTTP POST request data
  String httpRequestData = "checkcon=1";                                // Send HTTP POST request
  int httpResponseCode = http.POST(httpRequestData);                    // httpResponseCode should be "connected"
  if (httpResponseCode > 0) {                                           // get the response from the php page.
    ServerConnectResult = http.getString();                             // Connected: Connected to database
    ServerConnectResult.trim();                                         // Not connected: Not connected to database
#ifdef debug
    Serial.println("server response: " + ServerConnectResult);
#endif
    if (ServerConnectResult == "Connected") {
      ResultColor = 149;  // color is green
      ServerConnectResult = "Connected to chat server!";
    } else if (ServerConnectResult == "Not connected") {
      ResultColor = 146;  // color is red
      ServerConnectResult = "Server found but failed to connect";
    } else {
      ResultColor = 146;  // color is red
      ServerConnectResult = "No chatserver here!";
    }
  } else {
    ResultColor = 146;  // color is red
    ServerConnectResult = "Error, check server name!";
#ifdef debug
    Serial.print("Error code in ConnectivityCheck: ");
    Serial.println(httpResponseCode);
#endif
  }
  http.end();
}

// **************************************************
//  Task1 runs on the second core of the esp32
//  it receives messages from the web site
//  this process can be a bit slow so we run it on
//  the second core and the main program can continue
// **************************************************
void WifiCoreLoop(void* parameter) {
  WiFiCommandMessage commandMessage;
  WiFiResponseMessage responseMessage;
  unsigned long last_up_refresh = millis() + 5000;
  unsigned long heartbeat = millis();
  bool refreshUserPages = true;

  for (;;) {  // this is an endless loop

    // check for any command comming from app core for at most 1 sec.
    size_t ret = xMessageBufferReceive(commandBuffer, &commandMessage, sizeof(commandMessage), pdMS_TO_TICKS(1000));
    
    if (ret != 0)
    {
      switch(commandMessage.command){
        case WiFiBeginCommand: 
          WiFi.mode(WIFI_STA);
          WiFi.begin(ssid, password);
          break;
        case ConnectivityCheckCommand:
          ConnectivityCheck();
          break;
        case GetRegistrationStatusCommand: {
            responseMessage.command = GetRegistrationStatusCommand;
            char regStatus = getRegistrationStatus();
            responseMessage.response.str[0] = regStatus;
            xMessageBufferSend(responseBuffer, &responseMessage, sizeof(responseMessage), portMAX_DELAY);
          }
          break;         
        case SendMessageToServerCommand:
          responseMessage.command = SendMessageToServerCommand;
          responseMessage.response.boolean = 
          SendMessageToServer(commandMessage.data.sendMessageToServer.encoded,
                              commandMessage.data.sendMessageToServer.recipientName,
                              false);
          xMessageBufferSend(responseBuffer, &responseMessage, sizeof(responseMessage), portMAX_DELAY);
          break;     
        case GetWiFiMacAddressCommand:
          responseMessage.command = GetWiFiMacAddressCommand;
          WiFi.macAddress().toCharArray(responseMessage.response.str, sizeof(responseMessage.response.str));
          xMessageBufferSend(responseBuffer, &responseMessage, sizeof(responseMessage), portMAX_DELAY);
          break;                        
        case GetWiFiLocalIpCommand:
          responseMessage.command = GetWiFiLocalIpCommand;
          WiFi.localIP().toString().toCharArray(responseMessage.response.str, sizeof(responseMessage.response.str));
          xMessageBufferSend(responseBuffer, &responseMessage, sizeof(responseMessage), portMAX_DELAY);
          break;                        
        default:
          Serial.print("Invalid Command Message: ");
          Serial.println(commandMessage.command);
          break;
      }
    }

    isWifiCoreConnected = WiFi.isConnected();
    if (!isWifiCoreConnected) {
      continue;
    }

    if (getMessage == false) {  // this is a wait loop
      if (millis() > heartbeat + 25000) { // while we do nothing we send a heartbeat signal to the server
        heartbeat = millis();               // so that the web server knows you are still on line
        SendMessageToServer("", "", true);  // heartbeat repeats every 25 seconds
        refreshUserPages = true;            // and refresh the user pages (who is online)
      }
      if (millis() > last_up_refresh + 30000) {
        refreshUserPages = true;
      }
      if (updateUserlist and !fullpage) {
        updateUserlist = false;
        fill_userlist();
      }
      if (refreshUserPages and !fullpage) {
        refreshUserPages = false;
        get_full_userlist();
        last_up_refresh = millis();
      }
      if (fullpage == true) 
        getMessage = true;
      else
        continue;
    }
    
    // when the getMessage variable goes True, we drop out of the wait loop
    getMessage = false;                                           // first reset the getMessage variable back to false.
    String serverName = "http://" + server + "/readMessage.php";  // set up the server and needed web page
    WiFiClient client;
    HTTPClient httpb;
    httpb.setReuse(true);
    httpb.begin(client, serverName);                                       // start the http connection
    httpb.addHeader("Content-Type", "application/x-www-form-urlencoded");  // Specify content-type header

    String lp = "&lp=0";
    if (fullpage == true) {
      msgtype = "public";
      lp = "&lp=1";
      Serial.println("Get full page");
    }

    // Prepare your HTTP POST request data
    String httpRequestData = "sendername=" + myNickName + "&regid=" + regID + lp + "&lastmessage=" + lastmessage + "&lastprivate=" + lastprivmsg + "&type=" + msgtype + "&version=" + SwVersion + "&rom=" + romVersion + "&t=" + timeoffset;
    #ifdef debug
    Serial.println(httpRequestData);
    #endif
    // Send HTTP POST request
    int httpResponseCode = httpb.POST(httpRequestData);    
    if (httpResponseCode == 200) {  // httpResponseCode should be 200

      String textOutput = httpb.getString();  // capture the response from the webpage (it's json)
      textOutput.trim();                      // trim the output

      if (fullpage == true) {
        msgbuffersize = textOutput.length() + 1;
        textOutput.toCharArray(fullpagetext, msgbuffersize);  // copy the json string to full page buffer
        textOutput = "";
        fullpage = false;
      }

      msgbuffersize = textOutput.length() + 1;  //
      if (msgbuffersize > 498) {                // that should never happen
        msgbuffersize = 498;
#ifdef debug
        Serial.println("Error: msgbuffer is too large!");
#endif
      }
      textOutput.toCharArray(msgbuffer, msgbuffersize);  // copy the json string to the message buffer

      DynamicJsonDocument doc(512);                                  // next we want to analyse the json data
      DeserializationError error = deserializeJson(doc, msgbuffer);  // deserialize the json document
      if (!error) {
        unsigned long newMessageId = doc["rowid"];
        // if we get a new message id back from the database, that means we have a new message
        // if the database returns the same message id, there is no new message for us..
        bool newid = false;
        if ((msgtype == "private") and (newMessageId != lastprivmsg)) {
          newid = true;
          String nickname = doc["nickname"];
          if (nickname != myNickName) { pmSender = '@' + nickname; }
        }

        if (msgtype == "public") {
          pmCount = doc["pm"];
          if (newMessageId != lastmessage) {
            newid = true;
          }
        }

        if (newid) {
#ifdef debug
          Serial.print("new lastmessage id=");
          Serial.println(tempMessageID);
#endif
          tempMessageID = newMessageId;
          String message = doc["message"];
          String decoded_message = ' ' + my_base64_decode(message);
          int lines = doc["lines"];
          int msize = decoded_message.length() + 1;
          decoded_message.toCharArray(msgbuffer, msize);
          int outputLength = decoded_message.length();
          msgbuffersize = (int)outputLength;
          msgbuffer[0] = lines;
          msgbuffersize += 1;
          haveMessage = 1;
          if (msgtype == "private") haveMessage = 2;
        } else {
          // we got the same message id back, so no new messages:
          msgbuffersize = 0;
          haveMessage = 0;
        }
      } else {
#ifdef debug
        if (textOutput != "") Serial.println("Error: Json deserialize error in getmessage");
#endif
      }
    } else {
#ifdef debug
      Serial.println("Error: Readmessage response = " + httpResponseCode);
#endif
    }
    // Free resources
    httpb.end();
  }
}
