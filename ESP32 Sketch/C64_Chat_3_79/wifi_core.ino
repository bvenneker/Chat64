#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/message_buffer.h>
#include <WiFi.h>
#include <HTTPUpdate.h>

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
unsigned long lastWifiBegin;
// You do NOT need to change any of these settings!
String ssid = "empty";      // do not change this!
String password = "empty";  // do not change this!
String timeoffset = "empty";
String server = "empty";  // do not change this!
String myLocalIp = "0.0.0.0";
volatile unsigned long messageIds[] = { 0, 0 };
volatile unsigned long tempMessageIds[] = { 0, 0 };
volatile unsigned long lastprivmsg = 0;
String msgtype = "public";  // do not change this!
String users = "";          // a list of all users on this server.
volatile bool updateUserlist = false;
char msgbuffer[500];  // a character buffer for a chat message
volatile int msgbuffersize = 0;
volatile int haveMessage = 0;
volatile bool getMessage = false;
volatile bool pastMatrix = false;
volatile bool aboutToReset = false;
volatile bool sendingMessage = false;
String userPages[8];
String romVersion = "0.0";
String newVersions ="";
MessageBufferHandle_t commandBuffer;
MessageBufferHandle_t responseBuffer;
bool isWifiCoreConnected = false;
int lastp = 0;
String configured = "empty";  // do not change this!
// ***************************************************************
//   get the list of users from the webserver
// ***************************************************************
void fill_userlist() {
  String oldusers = users;
  String serverName = "http://" + server + "/listUsers.php";
  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverName);
  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Prepare your HTTP POST request data
  String httpRequestData = "regid=" + regID + "&call=list";

  // Send HTTP POST request
  unsigned long responseTime = millis();
  int httpResponseCode = http.POST(httpRequestData);
  responseTime = millis() - responseTime;
  if (responseTime > 10000) softReset();
  String result = "0";

  if (httpResponseCode == 200) {
    users = http.getString();
    users.trim();
#ifdef debug
    Serial.println(users);
#endif
  } else {
    result = "communication error";
    users = oldusers;
  }

  // Free resources
  http.end();
  client.stop();
}

// *************************************************
//  void to send a message to the server
// *************************************************
bool SendMessageToServer(String Encoded, String RecipientName, int retryCount, bool heartbeat) {

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
    httpRequestData = "sendername=" + myNickName + "&retryCount=" + retryCount + "&regid=" + regID + "&recipientname=" + RecipientName + "&message=" + Encoded;
  }

  // Send HTTP POST request
  unsigned long responseTime = millis();
  int httpResponseCode = http.POST(httpRequestData);
  responseTime = millis() - responseTime;
  if (responseTime > 10000) softReset();

  // httpResponseCode should be 200
  if (httpResponseCode == 200) {
    result = true;
  }
  // Free resources
  http.end();
  client.stop();
  return result;
}

// *******************************************************
//  String function to get the userlist from the database
// *******************************************************
void get_full_userlist() {
  // this is for the user list in the menu (Who is on line?)
  // The second core calls this webpage so the main thread does not suffer performance
  for (int p = 0; p < 8; p++) {
    userPages[p] = getUserList(p);
    char firstchar = userPages[p].charAt(0);
    if ((firstchar == 156 or firstchar == 149) == false) userPages[p] = "      ";
  }
}

String getUserList(int page) {
  String serverName = "http://" + server + "/listUsers.php";
  WiFiClient client;
  HTTPClient http;
  http.setReuse(1);
  http.begin(client, serverName);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String httpRequestData = "regid=" + regID + "&pagesize=15&page=" + page + "&version=2";
  unsigned long responseTime = millis();
  http.POST(httpRequestData);
  responseTime = millis() - responseTime;
  if (responseTime > 10000) softReset();
  String result = "0";
  result = http.getString();
  result.trim();
  http.end();
  client.stop();
  return result;
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
  unsigned long responseTime = millis();
  int httpResponseCode = http.POST(httpRequestData);
  responseTime = millis() - responseTime;
  if (responseTime > 10000) softReset();
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
  unsigned long responseTime = millis();
  int httpResponseCode = http.POST(httpRequestData);  // httpResponseCode should be "connected"
  responseTime = millis() - responseTime;
  if (responseTime > 10000) softReset();
  if (httpResponseCode > 0) {                // get the response from the php page.
    ServerConnectResult = http.getString();  // Connected: Connected to database
    ServerConnectResult.trim();              // Not connected: Not connected to database
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
  client.stop();
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

    if (ret != 0) {
      switch (commandMessage.command) {
        case WiFiBeginCommand:
          lastWifiBegin=millis();
          Serial.println("Connecting to WiFi");           
          WiFi.mode(WIFI_STA);
          WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
          WiFi.begin(ssid, password);
          break;
        case ConnectivityCheckCommand:
          ConnectivityCheck();
          break;
        case GetRegistrationStatusCommand:
          {
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
                                commandMessage.data.sendMessageToServer.retryCount,
                                false);
          xMessageBufferSend(responseBuffer, &responseMessage, sizeof(responseMessage), portMAX_DELAY);
          break;
        case GetWiFiMacAddressCommand:
          responseMessage.command = GetWiFiMacAddressCommand;
          Network.macAddress().toCharArray(responseMessage.response.str, sizeof(responseMessage.response.str));
          xMessageBufferSend(responseBuffer, &responseMessage, sizeof(responseMessage), portMAX_DELAY);
          break;
        //case GetWiFiLocalIpCommand:
        //  responseMessage.command = GetWiFiLocalIpCommand;
        //  WiFi.localIP().toString().toCharArray(responseMessage.response.str, sizeof(responseMessage.response.str));
        //  xMessageBufferSend(responseBuffer, &responseMessage, sizeof(responseMessage), portMAX_DELAY);
        //  break;
        case DoUpdateCommand:
          doUpdate();
          break;
        default:
          Serial.print("Invalid Command Message: ");
          Serial.println(commandMessage.command);
          break;
      }
    }

    isWifiCoreConnected = WiFi.isConnected();
    if (!isWifiCoreConnected) {
      myLocalIp="0.0.0.0";
      if (millis() > lastWifiBegin + 7000) {
        lastWifiBegin=millis();
        Serial.print("Connecting to WiFi again");         
        WiFi.mode(WIFI_STA);
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
        WiFi.begin(ssid, password);  
      }
      continue;
    } 
    
    myLocalIp=WiFi.localIP().toString();
    
    if (!configured.startsWith("d")){
      continue;
    }
    
    if (!getMessage) {                     // this is a wait loop
      if (millis() > heartbeat + 25000) {  // while we do nothing we send a heartbeat signal to the server
        heartbeat = millis();              // so that the web server knows you are still on line
        if (!sendingMessage) {
          SendMessageToServer("", "", 0, true);  // heartbeat repeats every 25 seconds
        }
        refreshUserPages = true;  // and refresh the user pages (who is online)
      }
      if (millis() > last_up_refresh + 30000 and pastMatrix and !sendingMessage) {
        refreshUserPages = true;

      }
      if (updateUserlist and !getMessage and pastMatrix and !sendingMessage) {
        updateUserlist = false;
        fill_userlist();
      }
      if (refreshUserPages) {
        refreshUserPages = false;
        get_full_userlist();
        last_up_refresh = millis();
      }
      continue;
    }

    // when the getMessage variable goes True, we drop out of the wait loop
    getMessage = false;                                               // first reset the getMessage variable back to false.
    String serverName = "http://" + server + "/readAllMessages.php";  // set up the server and needed web page
    WiFiClient client;
    HTTPClient httpb;

    httpb.setReuse(0);
    httpb.begin(client, serverName);                                       // start the http connection
    httpb.addHeader("Content-Type", "application/x-www-form-urlencoded");  // Specify content-type header

    // Prepare your HTTP POST request data
    String httpRequestData = "mac=" + macaddress + "&regid=" + regID + "&lastmessage=" + messageIds[0] + "&lastprivate=" + messageIds[1] + "&previousPrivate=" + lastprivmsg + "&type=" + msgtype + "&version=" + SwVersion + "&rom=" + romVersion + "&t=" + timeoffset;
#ifdef debug
    Serial.println(serverName);
    Serial.println(httpRequestData);
#endif
    unsigned long responseTime = millis();
    // Send HTTP POST request
    int httpResponseCode = httpb.POST(httpRequestData);
    responseTime = millis() - responseTime;
    if (responseTime > 10000) softReset();
#ifdef debug
    Serial.print("http POST took: ");
    Serial.print(responseTime);
    Serial.println(" ms.");
    Serial.print("Response code=");
    Serial.println(httpResponseCode);
#endif
    if (httpResponseCode == 200) {  // httpResponseCode should be 200
      String textOutput = httpb.getString();  // capture the response from the webpage (it's json)
      textOutput.trim();                      // trim the output

      msgbuffersize = textOutput.length() + 1;
      if (msgtype == "private") {
        textOutput.toCharArray(multiMessageBufferPriv, msgbuffersize);
      }
      if (msgtype == "public") {
        textOutput.toCharArray(multiMessageBufferPub, msgbuffersize);
      }

      textOutput = "";
      heartbeat = millis();  // readAllMessages also updates the 'last seen' timestamp, so no need for a heartbeat for the next 25 seconds.
    }
    // Free resources

    httpb.end();
    client.stop();  // without this, we have a small memory leak
    newVersions = UpdateAvailable();
  }
}

void softReset() {
  settings.begin("mysettings", false);
  settings.putInt("doReset", 157);
  settings.putULong("lastPubMessage", messageIds[0]);
  settings.putULong("lastPrivMessage", messageIds[1]);
  settings.end();
  delay(100);
  ESP.restart();
}

String UpdateAvailable(){
  Serial.println("*****************  CHECK UPDATES");
  String serverName = "http://" + server + "/checkUpdateForC64.php";
  
  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverName);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String httpRequestData = "regid=" + regID ;
  http.POST(httpRequestData);
  String result = "0";
  result = http.getString();
  result.trim();
  
  http.end();
  client.stop();
  String thisVersion = String(uromVersion) + " " + String(SwVersion);
  if (result != thisVersion) {
    
    return result;
  }
  return "";
}

void doUpdate(){
    updateProgress(1);
     
    NetworkClient client;
    httpUpdate.onStart(update_started);
    httpUpdate.onEnd(update_finished);
    httpUpdate.onProgress(update_progress);
    httpUpdate.onError(update_error);
    httpUpdate.update(client, "http://www.chat64.nl/update/C64_Chat.bin");
    
}

void update_started() {
  updateProgress(1);
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
  int p = map(cur,1,total,1,28);
  updateProgress(p);
  if (p==28) delay(500);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}


void updateProgress(int p){   
  if (lastp == p) return;
  lastp = p;
  digitalWrite(oC64D0, bool(p & B00000001));
  digitalWrite(oC64D1, bool(p & B00000010));
  digitalWrite(oC64D2, bool(p & B00000100));
  digitalWrite(oC64D3, bool(p & B00001000));
  digitalWrite(oC64D4, bool(p & B00010000));
  digitalWrite(oC64D5, bool(p & B00100000));
  digitalWrite(oC64D6, bool(p & B01000000));
  digitalWrite(oC64D7, bool(p & B10000000));
  digitalWrite(oC64NMI, !invert_nmi_signal);
  delayMicroseconds(175);  // minimal 100 microseconds delay
  digitalWrite(oC64NMI, invert_nmi_signal);
  delayMicroseconds(175);  // minimal 100 microseconds delay
}