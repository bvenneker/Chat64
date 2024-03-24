#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "ArduinoJson.h"

#define debug

Preferences settings;

String SwVersion = "3.58";

bool invert_reset_signal = true;    // false for pcb version 3.7 and up
bool invert_nmi_signal = false;     // true for pcb version 3.7 and up

// About the regID (registration id)
// A user needs to register at https://www.chat64.nl
// they will receive a registration_id via email.
// that number needs to be filled in on the account setup page on the commodore 64
// now the cartridge is registered and the administrator has a way to block the user
// the user can not register again with the same email address if they are blocked

// ********************************
// **     Global Variables       **
// ********************************
// You do NOT need to change any of these settings!
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
String configured = "empty";             // do not change this!
volatile unsigned long lastmessage = 1;  // do not change this!
volatile unsigned long lastprivmsg = 1;  // do not change this!
volatile unsigned long tempMessageID = 0;
String msgtype = "public";  // do not change this!
String users = "";          // a list of all users on this server.
String tempMessageTp = "";
String urgentMessage = "";
volatile bool wificonnected = false;
char regStatus = 'u';
volatile bool dataFromC64 = false;
volatile bool io2 = false;
volatile bool updateUserlist = false;
char inbuffer[250];  // a character buffer for incomming data
int inbuffersize = 0;
char outbuffer[250];  // a character buffer for outgoing data
int outbuffersize = 0;
char msgbuffer[500];  // a character buffer for a chat message
volatile int msgbuffersize = 0;
char textbuffer[10];  // a small buffer to capture the start of a message
char textsize = 0;
volatile int haveMessage = 0;
int it = 0;
volatile byte ch = 0;
volatile bool getMessage = false;
TaskHandle_t Task1;
String userPages[6];
volatile bool refreshUserPages = true;
volatile unsigned long last_up_refresh = millis() + 5000;
String romVersion = "0.0";
volatile bool mLEDstatus = false;
volatile bool fullpage = true;
char fullpagetext[3500];
byte send_error = 0;
int userpageCount = 0;

// ********************************
// **        OUTPUTS             **
// ********************************
// see http://www.bartvenneker.nl/index.php?art=0030
// for usable io pins!

#define oC64D0 GPIO_NUM_5   // data bit 0 for data from the ESP32 to the C64
#define oC64D1 GPIO_NUM_33  // data bit 1 foswitch data fswitchom the ESP32 to the C64
#define oC64D2 GPIO_NUM_14  // data bit 2 for data from the ESP32 to the C64
#define oC64D3 GPIO_NUM_23  // data bit 3 for data from the ESP32 to the C64
#define oC64D4 GPIO_NUM_13  // data bit 4 for data from the ESP32 to the C64
#define oC64D5 GPIO_NUM_19  // data bit 5 for data from the ESP32 to the C64
#define oC64D6 GPIO_NUM_18  // data bit 6 for data from the ESP32 to the C64
#define oC64D7 GPIO_NUM_26  // data bit 7 for data from the ESP32 to the C64

#define oC64RST GPIO_NUM_21  // reset signal to C64
#define oC64NMI GPIO_NUM_32  // non-maskable interrupt signal to C64
#define CLED GPIO_NUM_4      // led on cartridge
#define sclk GPIO_NUM_25     // serial clock signal to the shift register
#define pload GPIO_NUM_16    // parallel load signal to the shift register

#define mLED GPIO_NUM_2  // Internal LED

// ********************************
// **        INPUTS             **
// ********************************
#define resetSwitch GPIO_NUM_15  // this pin outputs PWM signal at boot
#define C64IO1 GPIO_NUM_22
#define sdata GPIO_NUM_27
#define C64IO2 GPIO_NUM_17

// *************************************************
// Interrupt routine for IO1
// *************************************************
void IRAM_ATTR isr_io1() {

  // This signal goes LOW when the commodore writes to (or reads from) the IO1 address space
  // In our case the Commodore 64 only WRITES the IO1 address space, so ESP32 can read the data.
  digitalWrite(oC64D7, LOW);  // this pin is used for flow controll,
                              // make it low so the C64 will not send the next byte
                              // until we are ready for it
  ch = 0;
  mLEDstatus = true;
  digitalWrite(pload, HIGH);  // stop loading parallel data and enable shifting serial data
  ch = shiftIn(sdata, sclk, MSBFIRST);
  dataFromC64 = true;
  digitalWrite(pload, LOW);
}

// *************************************************
// Interrupt routine for IO2
// *************************************************
void IRAM_ATTR isr_io2() {
  // This signal goes LOW when the commodore reads from (or write to) the IO2 address space
  // In this case the commodore only uses the IO2 address space to read from, so ESP32 can send data.
  io2 = true;
}

// *************************************************
// Interrupt routine, to restart the esp32
// *************************************************
void IRAM_ATTR isr_reset() {
  ESP.restart();
}

// *************************************************
//  SETUP
// *************************************************
void setup() {
  Serial.begin(115200);

  // we create a task for the second (unused) core of the esp32
  // this task will communicate with the web site while the other core
  // is busy talking to the C64
  xTaskCreatePinnedToCore(
    Task1code, /* Function to implement the task */
    "Task1",   /* Name of the task */
    10000,     /* Stack size in words */
    NULL,      /* Task input parameter */
    0,         /* Priority of the task */
    &Task1,    /* Task handle. */
    0);        /* Core where the task should run */

  // get the wifi mac address, this is used to identify the cartridge.
  macaddress = WiFi.macAddress();
  macaddress.replace(":", "");
  macaddress.toLowerCase();
  macaddress = macaddress.substring(4);

  // add a checksum to the mac address.
  byte data[4];
  int i = 0;
  for (unsigned int t = 0; t < macaddress.length(); t = t + 2) {
    String p = macaddress.substring(t, t + 2);
    char n[3];
    p.toCharArray(n, 3);
    byte f = x2i(n);
    data[i++] = f;
  }
  String crc8 = String(checksum(data, 4), HEX);

  if (crc8.length() == 1) crc8 = "0" + crc8;
  macaddress += crc8;

  // init settings object to store settings in the eeprom
  settings.begin("mysettings", false);

  // get the configured status from the eeprom
  configured = settings.getString("configured", "empty");

  // get the registration id from the eeprom
  regID = settings.getString("regID", "unregistered!");

  // get the nick name from the eeprom
  myNickName = settings.getString("myNickName", "empty");

  // get the last known message id
  lastmessage = settings.getULong("lastmessage", 1);
  lastprivmsg = settings.getULong("lastprivmsg", 1);

  //lastmessage = 1 ;                            // for debugging and testing  //  <<--------------------
  //lastprivmsg = 1 ;                            // for debugging and testing  //  <<--------------------

  // get Chatserver ip/fqdn from eeprom
  server = settings.getString("server", "www.chat64.nl");

  ssid = settings.getString("ssid", "empty");  // get WiFi credentials and Chatserver ip/fqdn from eeprom
  password = settings.getString("password", "empty");
  timeoffset = settings.getString("timeoffset", "+0");  // get the time offset from the eeprom

  settings.end();

  // define inputs
  pinMode(sdata, INPUT);
  pinMode(C64IO1, INPUT_PULLDOWN);
  pinMode(C64IO2, INPUT_PULLUP);
  pinMode(resetSwitch, INPUT_PULLUP);

  // define interrupts
  attachInterrupt(C64IO1, isr_io1, RISING);          // interrupt for io1, C64 writes data to io1 address space
  attachInterrupt(C64IO2, isr_io2, FALLING);         // interrupt for io2, c64 reads
  attachInterrupt(resetSwitch, isr_reset, FALLING);  // interrupt for reset button

  // define outputs
  pinMode(mLED, OUTPUT);
  digitalWrite(mLED, LOW);
  pinMode(CLED, OUTPUT);
  digitalWrite(CLED, LOW);
  pinMode(oC64D0, OUTPUT);
  pinMode(oC64D1, OUTPUT);
  pinMode(oC64D2, OUTPUT);
  pinMode(oC64D3, OUTPUT);
  pinMode(oC64D4, OUTPUT);
  pinMode(oC64D5, OUTPUT);
  pinMode(oC64D6, OUTPUT);
  pinMode(oC64D7, OUTPUT);
  digitalWrite(oC64D7, LOW);
  pinMode(oC64RST, OUTPUT);
  pinMode(oC64NMI, OUTPUT);
  digitalWrite(oC64RST, invert_reset_signal);
  digitalWrite(oC64NMI, invert_nmi_signal);

  digitalWrite(oC64NMI, LOW);
  pinMode(pload, OUTPUT);
  digitalWrite(pload, LOW);  // must be low to load parallel data
  pinMode(sclk, OUTPUT);
  digitalWrite(sclk, LOW);  //data shifts to serial data output on the transition from low to high.

  // Reset the C64, toggle the output pin
  uint32_t new_state = 1 - (GPIO.out >> oC64RST & 0x1);
  digitalWrite(oC64RST, new_state);
  delay(250);
  digitalWrite(oC64RST, !new_state);

  // try to connect to wifi for 5 seconds
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  for (int d = 0; d < 10; d++) {
    if (WiFi.isConnected()) {
      wificonnected = true;
      break;
    }
    delay(500);
  }

  // check if we are connected to wifi
  if (WiFi.isConnected()) {
    // light the LED when connection was succesful
    digitalWrite(CLED, HIGH);
    wificonnected = true;

    // check the connection with the server.
    ConnectivityCheck();

#ifdef debug
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Name of the server (from eeprom): ");
    Serial.println(server);
#endif


  } else {
// if there is no wifi, the user can change the credentials in cartridge menu
#ifdef debug
    Serial.print("NO Wifi connection! : ");
    Serial.println(WiFi.localIP());
#endif
  }

#ifdef debug
  Serial.print("last public message = ");
  Serial.println(lastmessage);
  Serial.print("last private message = ");
  Serial.println(lastprivmsg);
#endif

}  // end of setup

// ***************************************************************
//   get the list of users from the webserver
// ***************************************************************

void fill_userlist() {
  String oldusers = users;
  String serverName = "http://" + server + "/list_users.php";
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
}

// **************************************************
//  Task1 runs on the second core of the esp32
//  it receives messages from the web site
//  this process can be a bit slow so we run it on
//  the second core and the main program can continue
// **************************************************
void Task1code(void* parameter) {

  for (;;) {  // this is an endless loop

    unsigned long heartbeat = millis();
    while (getMessage == false) {  // this is a wait loop
      delay(10);                   // this task does nothing until the variable getMessage becomes true
      if (millis() > heartbeat + 25000
          and WiFi.isConnected()) {         // while we do nothing we send a heartbeat signal to the server
        heartbeat = millis();               // so that the web server knows you are still on line
        SendMessageToServer("", "", true);  // heartbeat repeats every 25 seconds
        refreshUserPages = true;            // and refresh the user pages (who is online)
      }
      if (updateUserlist and WiFi.isConnected()) {
        updateUserlist = false;
        fill_userlist();
      }
      if (refreshUserPages and WiFi.isConnected()) {
        refreshUserPages = false;
        get_full_userlist();
      }
      if (fullpage == true and WiFi.isConnected()) getMessage = true;
    }
    // when the getMessage variable goes True, we drop out of the wait loop
    getMessage = false;  // first reset the getMessage variable back to false.

    String serverName = "http://" + server + "/readMessage.php";  // set up the server and needed web page
    WiFiClient client;
    HTTPClient http;
    http.setReuse(true);
    http.begin(client, serverName);                                       // start the http connection
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  // Specify content-type header

    String lp = "&lp=0";
    if (fullpage == true) {
      msgtype = "public";
      lp = "&lp=1";
    }

    // Prepare your HTTP POST request data
    String httpRequestData = "sendername=" + myNickName + "&regid=" + regID + lp + "&lastmessage=" + lastmessage + "&lastprivate=" + lastprivmsg + "&type=" + msgtype + "&version=" + SwVersion + "&rom=" + romVersion + "&t=" + timeoffset;

    // Send HTTP POST request
    int httpResponseCode = http.POST(httpRequestData);
    if (httpResponseCode == 200) {           // httpResponseCode should be 200
      String textOutput = http.getString();  // capture the response from the webpage (it's json)
      textOutput.trim();                     // trim the output

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
          tempMessageTp = msgtype;
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
    http.end();
  }
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
void get_full_userlist() {
  // this is for the user list in the menu (Who is on line?)
  // The second core calls this webpage so the main thread does not suffer performance
  for (int p = 0; p < 6; p++) {
    userPages[p] = getUserList(p);
    char firstchar = userPages[p].charAt(0);
    if ((firstchar == 156 or firstchar == 149) == false) userPages[p] = "      ";
  }
  last_up_refresh = millis();
}

String getUserList(int page) {
  String serverName = "http://" + server + "/list_users.php";
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

// ******************************************************************************
// Main loop
// ******************************************************************************
unsigned long ledtimer = 0;
void loop() {

  digitalWrite(CLED, WiFi.isConnected());

  if (mLEDstatus) {
    digitalWrite(mLED, HIGH);
    ledtimer = millis() + 100;
    mLEDstatus = false;
  }

  if (ledtimer < millis()) {
    digitalWrite(mLED, LOW);
  }

  digitalWrite(oC64D7, HIGH);
  if (dataFromC64) {
    dataFromC64 = false;
    digitalWrite(oC64D7, LOW);  // flow control
#ifdef debug
    Serial.print("incomming command: ");
    Serial.println(ch);
#endif

    // generate an error if wifi connection drops
    if (wificonnected and !WiFi.isConnected()) {
      digitalWrite(CLED, LOW);
      wificonnected = false;
      urgentMessage = "   Error in WiFi connection, please reset";
    }

    // 254 = C64 triggers call to the website for new public message
    // 253 = new chat message from c64 to database
    // 252 = C64 sends the new wifi network name (ssid) AND password AND time offset
    // 251 = C64 ask for the current wifi ssid,password and time offset
    // 250 = C64 ask for the first full page of messages (during startup)
    // 249 = get result of last send action (253)
    // 248 = C64 ask for the wifi connection status
    // 247 = C64 triggers call to the website for new private message
    // 246 = set chatserver ip/fqdn
    // 245 = check if the esp is connected at all, or are we running in simulation mode?
    // 244 = reset to factory defaults
    // 243 = C64 ask for the mac address, registration id, nickname and regstatus
    // 242 = get senders nickname of last private message.
    // 241 = get the number of unread private messages
    // 240 = C64 sends the new registration id and nickname to ESP32
    // 238 = C64 triggers call to the chatserver to test connectivity
    // 237 = get chatserver connectivity status
    // 236 = C64 asks for the server configuration status and servername
    // 235 = C64 sends server configuration status
    // 234 = get user list first page
    // 233 = get user list next page


    //
    // 228 = debug purposes
    // 128 = end marker, ignore

    switch (ch) {
      case 254:
        {
          // ------------------------------------------------------------------------------
          // start byte 254 = C64 triggers call to the website for new public message
          // ------------------------------------------------------------------------------

          // if the user list is empty, get the list
          // also refresh the userlist when we switch from public to private messaging and vice versa
          if (users.length() < 1 or msgtype != "public") updateUserlist = true;

          msgtype = "public";
          if (haveMessage == 1 or haveMessage == 3) {
            // copy the msgbuffer to the outbuffer
            for (int x = 0; x < msgbuffersize; x++) { outbuffer[x] = msgbuffer[x]; }

            // copy the buffer size also
            outbuffersize = msgbuffersize;

            // and send the outbuffer
            send_out_buffer_to_C64();
            // store the new message id
            if (haveMessage == 1) {
              lastmessage = tempMessageID;
              settings.begin("mysettings", false);
              settings.putULong("lastmessage", lastmessage);
              settings.end();
            }
            haveMessage = 0;
          } else {  // No message for now, just send byte 128.
            sendByte(128);
          }
          getMessage = true;
          break;
        }

      case 253:
        {
          // ------------------------------------------------------------------------------
          // start byte 253 = new chat message from c64 to database
          // ------------------------------------------------------------------------------

          // we expect a chat message from the C64
          receive_buffer_from_C64(1);
          String toEncode = "";
          String RecipientName = "";
          for (int x = 0; x < inbuffersize; x++) {
            inbuffer[x] = screenCode_to_Ascii(inbuffer[x]);
            byte b = inbuffer[x];
            if (b > 128) {
              toEncode = (toEncode + "[" + int(inbuffer[x]) + "]");
            } else {
              textbuffer[x] = inbuffer[x];
              toEncode = (toEncode + inbuffer[x]);
            }
          }
          // see if the message starts with '@'
          byte b = textbuffer[1];
          if (b == 64) {
            for (int x = 2; x < 10; x++) {
              byte b = textbuffer[x];
              if (b != 32) {
                RecipientName = (RecipientName + textbuffer[x]);
              } else {
                break;
              }
            }
            // is this a valid username?
            String test_name = RecipientName;
            test_name.toLowerCase();
            if (users.indexOf(test_name + ';') >= 0) {
              // user exists
              msgtype = "private";
            } else {
              // user does not exist
              urgentMessage = " System:  Unknown user:" + RecipientName;
              send_error = 1;
              break;
            }
          } else {msgtype = "public";}

          int buflen = toEncode.length() + 1;
          char buff[buflen];
          toEncode.toCharArray(buff, buflen);
          String Encoded = my_base64_encode(buff, buflen);

          // Now send it with retry!
          bool sc = false;
          int retry = 0;
          while (sc == false and retry < 5) {
            sc = SendMessageToServer(Encoded, RecipientName, false);
            // sending the message fails, take a short break and try again
            if (!sc) {
              delay(1000);
              retry = retry + 1;
            }
          }
          // if it still fails after a few retries, give us an error.
          if (!sc) {
            urgentMessage = " System:        ERROR sending the message";
            send_error = 1;
          } else {
            // No error, read the message back from the database to show it on screen
            getMessage = true;
          }
          break;
        }

      case 252:
        {
          // ------------------------------------------------------------------------------
          // 252 = C64 sends the new wifi network name (ssid) AND password AND time offset
          // ------------------------------------------------------------------------------
          receive_buffer_from_C64(3);

          for (int x = 0; x < inbuffersize; x++) {
            inbuffer[x] = screenCode_to_Ascii(inbuffer[x]);
          }

          // inbuffer now contains "SSID password timeoffset"
          char bns[inbuffersize + 1];
          strncpy(bns, inbuffer, inbuffersize + 1);
          String ns = bns;

          ssid = getValue(ns, 32, 0);
          Serial.println(ssid);
          password = getValue(ns, 32, 1);
          Serial.println(password);
          timeoffset = getValue(ns, 32, 2);
          Serial.println(timeoffset);

          settings.begin("mysettings", false);
          settings.putString("ssid", ssid);
          settings.putString("password", password);
          settings.putString("timeoffset", timeoffset);
          settings.end();
          WiFi.mode(WIFI_STA);
          WiFi.begin(ssid.c_str(), password.c_str());
          break;
        }

      case 251:
        {
          // ------------------------------------------------------------------------------
          // start byte 251 = C64 ask for the current wifi ssid,password and time offset
          // ------------------------------------------------------------------------------
          send_String_to_c64(ssid + " " + password + " " + timeoffset);
          break;
        }

      case 250:
        {
          // ------------------------------------------------------------------------------
          // start byte 250 = C64 ask for the first full page of messages
          // ------------------------------------------------------------------------------

          // if the fullpage variable is true, that means the other core has not
          // collected the data yet. Give it 3 seconds..
          unsigned long oldlastmessage = lastmessage;
          unsigned long timeOut = millis() + 3000;
          while (fullpage == true and millis() < timeOut) { delay(1); }

          int i = 0;
          while (i >= 0) {
            int p = 0;
            char cc = 0;

            // find first {
            while (cc != '{' and p < 10) {
              cc = fullpagetext[i++];
              msgbuffer[0] = cc;
              p++;
            }
            // fill buffer until we find '}'
            if (cc == '{') {
              p = 1;
              while (cc != '}') {
                cc = fullpagetext[i++];
                // put this line into the msgbuffer buffer
                if (cc != 10) msgbuffer[p++] = cc;
              }
            }
            DynamicJsonDocument doc(512);                                     // next we want to analyse the json data
            DeserializationError docerror = deserializeJson(doc, msgbuffer);  // deserialize the json document
            if (!docerror) {
              lastmessage = doc["rowid"];
              String a = doc["message"];
              String decoded_message = ' ' + my_base64_decode(a);
              int lines = doc["lines"];
              int msize = decoded_message.length() + 1;
              decoded_message.toCharArray(msgbuffer, msize);
              int outputLength = decoded_message.length();
              msgbuffersize = (unsigned long)outputLength;
              msgbuffer[0] = lines;
              msgbuffersize += 1;
              // copy the msgbuffer to the outbuffer
              for (int x = 0; x < msgbuffersize; x++) { outbuffer[x] = msgbuffer[x]; }
              // copy the buffer size also
              outbuffersize = msgbuffersize;
              // and send the outbuffer
              send_out_buffer_to_C64();

            } else {
              i = -1;
              sendByte(128);
#ifdef debug
              Serial.println("Json deserialize error");
#endif
            }
            // wait for next 250 command
            ch = 0;
            timeOut = millis() + 500;
            while (ch == 0) {
              delayMicroseconds(2);
              if (millis() > timeOut) {
                lastmessage = oldlastmessage;
                ch = 1;
#ifdef debug
                Serial.println("Timeout in fullpage routine");
#endif
              }
            }
            if (ch != 250) i = -1;
          }
          break;
        }

      case 249:
        {
          // ------------------------------------------------------------------------------
          // start byte 249 = C64 asks if this is an existing user (for private chat)
          // ------------------------------------------------------------------------------
          sendByte(send_error);
          sendByte(128);
          send_error = 0;
          break;
        }

      case 248:
        {
          // ------------------------------------------------------------------------------
          // start byte 248 = C64 ask for the wifi connection status
          // ------------------------------------------------------------------------------

          if (!WiFi.isConnected()) {
            digitalWrite(CLED, LOW);
            sendByte(146);
            send_String_to_c64("Not Connected to Wifi");
          } else {
            digitalWrite(CLED, HIGH);
            sendByte(149);
            String wifi_status = "Connected with ip " + WiFi.localIP().toString();
            send_String_to_c64(wifi_status);
          }
          break;
        }

      case 247:
        {
          // ------------------------------------------------------------------------------
          // start byte 247 = C64 triggers call to the website for new private message
          // ------------------------------------------------------------------------------

          // if the user list is empty, get the list
          // also refresh the userlist when we switch from public to private messaging and vice versa
          if (users.length() < 1 or msgtype != "private") updateUserlist = true;

          msgtype = "private";
          pmCount = 0;
          if (haveMessage == 2 or haveMessage == 3) {
            // copy the msgbuffer to the outbuffer
            for (int x = 0; x < msgbuffersize; x++) { outbuffer[x] = msgbuffer[x]; }

            // copy the buffer size also
            outbuffersize = msgbuffersize;

            // and send the outbuffer
            send_out_buffer_to_C64();
            if (haveMessage == 2) {
              // store the new message id
              lastprivmsg = tempMessageID;
              settings.begin("mysettings", false);
              settings.putULong("lastprivmsg", lastprivmsg);
              settings.end();
            }
            haveMessage = 0;
          } else {  // no private messages :-(
            sendByte(128);
          }
          getMessage = true;
          break;
        }

      case 246:
        {
          // ------------------------------------------------------------------------------
          // start byte 246 = C64 sends a new chat server ip/fqdn
          // ------------------------------------------------------------------------------

          receive_buffer_from_C64(1);
          for (int x = 0; x < inbuffersize; x++) {
            inbuffer[x] = screenCode_to_Ascii(inbuffer[x]);
          }

          char bns[inbuffersize + 1];
          strncpy(bns, inbuffer, inbuffersize + 1);
          String ns = bns;
          server = ns;
          settings.begin("mysettings", false);
          settings.putString("server", ns);     // store the new server name in the eeprom settings
          settings.putULong("lastmessage", 1);  // when connecting to a new server, we must also reset the message id's
          settings.putULong("lastprivmsg", 1);
          settings.end();

          lastmessage = 1;
          lastprivmsg = 1;

          // we should also refresh the userlist
          users = "";
          break;
        }
      case 245:
        {
          // ------------------------------------------------------------------------------
          // start byte 245 = C64 checks if the esp is connected at all.. or are we running in a simulator?
          // ------------------------------------------------------------------------------
          // receive the ROM version number
          receive_buffer_from_C64(1);
          char bns[inbuffersize + 1];
          strncpy(bns, inbuffer, inbuffersize + 1);
          String ns = bns;
          romVersion = ns;
          Serial.print("ROM Version=");
          Serial.println(romVersion);
          sendByte(128);
          refreshUserPages = true;
#ifdef debug
          Serial.println("are we in the Matrix?");
#endif
          break;
        }
      case 244:
        {
          // ------------------------------------------------------------------------------
          // start byte 244 = C64 sends the command to reset the cartridge to factory defaults
          // ------------------------------------------------------------------------------
          // this will reset all settings
          receive_buffer_from_C64(1);
          char bns[inbuffersize + 1];
          strncpy(bns, inbuffer, inbuffersize + 1);
          String ns = bns;
          if (ns == "RESET!") {
            settings.begin("mysettings", false);
            settings.putString("regID", "unregistered!");
            settings.putString("myNickName", "empty");
            settings.putString("ssid", "empty");
            settings.putString("password", "empty");
            settings.putString("server", "empty");
            settings.putString("configured", "empty");
            settings.putULong("lastmessage", 1);
            settings.putULong("lastprivmsg", 1);
            settings.end();
            // now reset the esp
            ESP.restart();
          }
          break;
        }

      case 243:
        {
          // ------------------------------------------------------------------------------
          // start byte 243 = C64 ask for the mac address, registration id, nickname and regstatus
          // ------------------------------------------------------------------------------
          regStatus = getRegistrationStatus();
          send_String_to_c64(macaddress + " " + regID + " " + myNickName + " " + regStatus);
          break;
        }
      case 242:
        {
          // ------------------------------------------------------------------------------
          // start byte 242 = C64 ask for the sender of the last private message
          // ------------------------------------------------------------------------------
          send_String_to_c64(pmSender);
          break;
        }
      case 241:
        {
          // ------------------------------------------------------------------------------
          // start byte 241 = C64 asks for the number of unread private messages
          // ------------------------------------------------------------------------------
          if (pmCount > 99) {
            sendByte(156);  // send color code for gray
            send_String_to_c64("[pm:99+]");
          } else {
            String pm = String(pmCount);
            if (pmCount < 10) { pm = "0" + pm; }
            pm = "[pm: " + pm + " (F5)]";
            if (pmCount == 0) pm = "~~~~~~~~~~";
            sendByte(156);           // send color code for gray
            send_String_to_c64(pm);  // then send the number of messages as a string
          }
          break;
        }
      case 240:
        {
          // ------------------------------------------------------------------------------
          // start byte 240 = C64 sends the new registration id and nickname to ESP32
          // ------------------------------------------------------------------------------
          receive_buffer_from_C64(2);
          for (int x = 0; x < inbuffersize; x++) {
            inbuffer[x] = screenCode_to_Ascii(inbuffer[x]);
          }
          // inbuffer now contains "registrationid nickname"
          char bns[inbuffersize + 1];
          strncpy(bns, inbuffer, inbuffersize + 1);
          String ns = bns;

          regID = getValue(ns, 32, 0);

          Serial.println(regID);
          if (regID.length() != 16) {
#ifdef debug
            Serial.println("bad form");
#endif
            break;
          }
          myNickName = getValue(ns, 32, 1);
          Serial.println(myNickName);

          settings.begin("mysettings", false);
          settings.putString("regID", regID);
          settings.putString("myNickName", myNickName);
          settings.end();
          break;
        }



      case 238:
        {
          // ------------------------------------------------------------------------------
          // start byte 238 = C64 triggers call to the chatserver to test connectivity
          // ------------------------------------------------------------------------------
          ConnectivityCheck();
          break;
        }

      case 237:
        {
          // ------------------------------------------------------------------------------
          // start byte 237 = C64 triggers call to receive connection status
          // ------------------------------------------------------------------------------
          sendByte(ResultColor);  // send color code for green
          send_String_to_c64(ServerConnectResult);
          break;
        }


      case 236:
        {
          // ------------------------------------------------------------------------------
          // start byte 236 = C64 asks for the server configuration status and servername
          // ------------------------------------------------------------------------------
          send_String_to_c64(configured + " " + server + " " + SwVersion);
#ifdef debug
          Serial.println("response 236 = " + configured + " " + server + " " + SwVersion);
#endif
          break;
        }

      case 235:
        {
          // ------------------------------------------------------------------------------
          // start byte 235 = C64 sends server configuration status
          // ------------------------------------------------------------------------------

          receive_buffer_from_C64(1);
          for (int x = 0; x < inbuffersize; x++) {
            inbuffer[x] = screenCode_to_Ascii(inbuffer[x]);
          }

          char bns[inbuffersize + 1];
          strncpy(bns, inbuffer, inbuffersize + 1);
          String ns = bns;
          configured = ns;
          settings.begin("mysettings", false);
          settings.putString("configured", ns);
          settings.end();
          break;
        }
      case 234:
        {
          // c64 asks for user list, first page.
          // we send a max of 20 users in one long string
          userpageCount = 0;
          String ul1 = userPages[userpageCount];
          send_String_to_c64(ul1);
          userpageCount++;
          break;
        }
      case 233:
        {
          // c64 asks for user list, second or third page.
          // we send a max of 20 users in one long string
          String ul1 = userPages[userpageCount];
          send_String_to_c64(ul1);
          userpageCount++;
          break;
        }
      case 228:
        {
          // ------------------------------------------------------------------------------
          // start byte 228 = Debug purposes
          // ------------------------------------------------------------------------------
          Serial.println("The code gets triggered");
          // your code here :-)
          sendByte(128);
          break;
        }
      default:
        {
          sendByte(128);
          break;
        }
    }  // end of case statements

    // ------------------------------------------------------------------------------
    // Urgent message
    // ------------------------------------------------------------------------------
    if (urgentMessage != "" and haveMessage == 0) {
      urgentMessage = "  " + urgentMessage;
      msgbuffersize = urgentMessage.length() + 1;
      urgentMessage.toCharArray(msgbuffer, msgbuffersize);
      msgbuffer[0] = 1;
      msgbuffer[1] = 143;
      msgbuffer[2] = 146;
      urgentMessage = "";
      haveMessage = 3;
    }



  }  // end of "if (dataFromC64)"

  if (millis() > last_up_refresh + 30000) refreshUserPages = true;

}  // end of main loop


// ******************************************************************************
// void to set a byte in the 74ls244 buffer
// ******************************************************************************
void outByte(byte c) {
  gpio_set_level(oC64D0, bool(c & B00000001));
  gpio_set_level(oC64D1, bool(c & B00000010));
  gpio_set_level(oC64D2, bool(c & B00000100));
  gpio_set_level(oC64D3, bool(c & B00001000));
  gpio_set_level(oC64D4, bool(c & B00010000));
  gpio_set_level(oC64D5, bool(c & B00100000));
  gpio_set_level(oC64D6, bool(c & B01000000));
  gpio_set_level(oC64D7, bool(c & B10000000));
}

// ******************************************************************************
// void: send a string to the C64
// ******************************************************************************
void send_String_to_c64(String s) {
  outbuffersize = s.length() + 1;           // set outbuffer size
  s.toCharArray(outbuffer, outbuffersize);  // place the ssid in the output buffer
  send_out_buffer_to_C64();                 // and send the buffer
}

// ******************************************************************************
// void: for debugging
// ******************************************************************************
void debug_print_inbuffer() {
  for (int x = 0; x < inbuffersize; x++) {
    char sw = screenCode_to_Ascii(inbuffer[x]);
    Serial.print(sw);
  }
}

// ******************************************************************************
//  void to receive characters from the C64 and store them in a buffer
// ******************************************************************************
void receive_buffer_from_C64(int cnt) {

  // cnt is the number of transmissions we put into this buffer
  // This number is 1 most of the time
  // but in the configuration screens the C64 will send multiple items at once (like ssid and password)

  int i = 0;

  while (cnt > 0) {
    digitalWrite(oC64D7, HIGH);  // ready for next byte
    unsigned long timeOut = millis() + 500;
    while (dataFromC64 == false) {
      delayMicroseconds(2);  // wait for next byte
      if (millis() > timeOut) {
        ch = 128;
        dataFromC64 = true;
#ifdef debug
        Serial.println("Timeout in receive buffer");
#endif
      }
    }
    digitalWrite(oC64D7, LOW);
    dataFromC64 = false;
    inbuffer[i] = ch;
    i++;
    if (i > 248) {  //this should never happen
#ifdef debug
      Serial.print("Error: inbuffer is about to flow over!");
#endif
      ch = 128;
      cnt = 0;
      break;
    }
    if (ch == 128) cnt--;
  }
  i--;
  inbuffer[i] = 0;  // close the buffer
  inbuffersize = i;
}


// ******************************************************************************
// Send the content of the outbuffer to the C64
// ******************************************************************************
void send_out_buffer_to_C64() {
  // send the content of the outbuffer to the C64
  for (int x = 0; x < outbuffersize - 1; x++) {
    sendByte(Ascii_to_screenCode(outbuffer[x]));
  }
  // all done, send end byte
  sendByte(128);
  outbuffersize = 0;
}


// ******************************************************************************
// pull the NMI line low for a few microseconds
// ******************************************************************************
void triggerNMI() {
  // toggle NMI
  uint32_t new_state = 1 - (GPIO.out >> oC64NMI & 0x1);
  digitalWrite(oC64NMI, new_state);
  delayMicroseconds(125);  // minimal 100 microseconds delay
  // And toggle back
  digitalWrite(oC64NMI, !new_state);
}


// ******************************************************************************
// send a single byte to the C64
// ******************************************************************************
void sendByte(byte b) {
  outByte(b);
  io2 = false;
  triggerNMI();
  // wait for io2 interupt
  unsigned long timeOut = millis() + 500;
  while (io2 == false) {
    delayMicroseconds(2);
    if (millis() > timeOut) {
      io2 = true;
#ifdef debug
      Serial.println("Timeout in sendByte");
#endif
    }
  }
  io2 = false;
}

// ******************************************************************************
// translate screen codes to ascii
// ******************************************************************************
char screenCode_to_Ascii(byte screenCode) {

  byte screentoascii[] = { 64, 97, 98, 99, 100, 101, 102, 103, 104, 105,
                           106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
                           116, 117, 118, 119, 120, 121, 122, 91, 92, 93,
                           94, 95, 32, 33, 34, 125, 36, 37, 38, 39,
                           40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
                           50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
                           60, 61, 62, 63, 95, 65, 66, 67, 68, 69,
                           70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
                           80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
                           90, 43, 32, 124, 32, 32, 32, 32, 32, 32,
                           95, 32, 32, 32, 32, 32, 32, 32, 32, 32,
                           32, 95, 32, 32, 32, 32, 32, 32, 32, 32,
                           32, 32, 32, 32, 32, 32, 32, 32, 32 };

  char result = char(screenCode);
  if (screenCode < 129) result = char(screentoascii[screenCode]);
  return result;
}


// ******************************************************************************
// translate ascii to c64 screen codes
// ******************************************************************************
byte Ascii_to_screenCode(char ascii) {

  byte asciitoscreen[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                           11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                           22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
                           33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
                           44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54,
                           55, 56, 57, 58, 59, 60, 61, 62, 63, 0, 65,
                           66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
                           77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,
                           88, 89, 90, 27, 92, 29, 30, 100, 39, 1, 2,
                           3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                           14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                           25, 26, 27, 93, 35, 64, 32, 32 };
  byte result = ascii;
  if (int(ascii) < 129) result = byte(asciitoscreen[int(ascii)]);
  return result;
}

// ************************************************************************************
// BASE64 encode / decode functions.
// based on https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c
// ************************************************************************************
String base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

String my_base64_encode(char* buf, int bufLen) {
  String ret;
  int i = 0;
  int j = 0;

  unsigned char char_array_4[4], char_array_3[3];
  while (bufLen--) {
    char_array_3[i++] = *(buf++);

    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (i = 0; (i < 4); i++)
        ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      ret += base64_chars[char_array_4[j]];

    while ((i++ < 3))
      ret += '=';
  }
  return ret;
}

String my_base64_decode(String const& encoded_string) {
  int inlen = encoded_string.length();
  int i = 0;
  int j = 0;
  int k = 0;
  unsigned char char_array_4[4], char_array_3[3];
  String ret;

  while (inlen-- && (encoded_string[k] != '=') && is_base64(encoded_string[k])) {
    char_array_4[i++] = encoded_string[k];
    k++;
    if (i == 4) {
      for (i = 0; i < 4; i++) {
        char_array_4[i] = (char)base64_chars.indexOf(char_array_4[i]);
      }

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
      for (i = 0; (i < 3); i++) {
        ret += (char)char_array_3[i];
      }
      i = 0;
    }
  }

  if (i) {
    for (j = 0; j < i; j++) {
      char_array_4[j] = (char)base64_chars.indexOf(char_array_4[j]);
    }

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    for (j = 0; (j < i - 1); j++) {
      ret += (char)char_array_3[j];
    }
  }
  return ret;
}

byte checksum(byte data[], int datasize) {
  byte sum = 0;
  for (int i = 0; i < datasize; i++) {
    sum += data[i];
  }
  return -sum;
}

int x2i(char* s) {
  int x = 0;
  for (;;) {
    char c = *s;
    if (c >= '0' && c <= '9') {
      x *= 16;
      x += c - '0';
    } else if (c >= 'a' && c <= 'f') {
      x *= 16;
      x += (c - 'a') + 10;
    } else break;
    s++;
  }
  return x;
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
