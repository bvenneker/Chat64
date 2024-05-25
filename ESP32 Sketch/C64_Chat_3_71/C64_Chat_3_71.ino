#include <Preferences.h>
#include <ArduinoJson.h>

#include "common.h"
#include "utils.h"
#include "wifi_core.h"
#include "prgfile.h"
Preferences settings;

bool invert_reset_signal = false ;  // true for pcb rev 2.0
                                    // false for pcb rev 3.7 
                                    // false for pcb rev 3.8

bool invert_nmi_signal = false;     // false for pcb rev 2.0
                                    // true for pcb rev 3.7, 
                                    // false for rev 3.8

#ifdef VICE_MODE
bool accept_serial_command = true;
#endif

// About the regID (registration id)
// A user needs to register at https://www.chat64.nl
// they will receive a registration_id via email.
// that number needs to be filled in on the account setup page on the commodore 64
// now the cartridge is registered and the administrator has a way to block the user
// the user can not register again with the same email address if they are blocked

// ********************************
// **     Global Variables       **
// ********************************
String configured = "empty";  // do not change this!

String urgentMessage = "";
volatile bool wificonnected = false;
char regStatus = 'u';
volatile bool dataFromC64 = false;
volatile bool io2 = false;
char inbuffer[250];  // a character buffer for incomming data
int inbuffersize = 0;
char outbuffer[250];  // a character buffer for outgoing data
int outbuffersize = 0;
char textsize = 0;
int it = 0;
volatile byte ch = 0;
TaskHandle_t Task1;
volatile bool internalLEDstatus = false;
byte send_error = 0;
int userpageCount = 0;
char multiMessageBufferPub[3500];
char multiMessageBufferPriv[3500];

WiFiCommandMessage commandMessage;
WiFiResponseMessage responseMessage;

// ********************************
// **        OUTPUTS             **
// ********************************
// see http://www.bartvenneker.nl/index.php?art=0030
// for usable io pins!

#define oC64D0 GPIO_NUM_5   // data bit 0 for data from the ESP32 to the C64
#define oC64D1 GPIO_NUM_33  // data bit 1 for data from the ESP32 to the C64
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

#define internalLED GPIO_NUM_2  // Internal LED

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
  internalLEDstatus = true;
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
  reboot();
}

void reboot() {
#ifdef VICE_MODE
  send_serial_reboot();
#endif

  ESP.restart();
}

#ifdef VICE_MODE
void receive_serial_command() {
  static bool receiving_command = false;
  while (Serial2.available() > 0) {
    byte buf = Serial2.read();
    if (!receiving_command && buf == '$') {
      receiving_command = true;
    } else {
      if (buf == 'I') {
        ch = Serial2.parseInt();
        internalLEDstatus = true;
        dataFromC64 = true;
      } else if (buf == 'J') {
        io2 = true;
      }

      receiving_command = false;
    }
  }
}

void send_serial_data(byte b) {
  Serial2.print("$D");
  Serial2.print(b);
  Serial2.write((uint8_t)0);
}

void send_serial_nmi() {
  Serial2.print("$N");
  Serial2.write((uint8_t)0);
}

void send_serial_reboot() {
  Serial2.print("$R");
  Serial2.write((uint8_t)0);
  delay(100);
}
#endif

// *************************************************
//  SETUP
// *************************************************
void setup() {
  Serial.begin(115200);

#ifdef VICE_MODE
  Serial2.begin(115200);
#endif

  commandBuffer = xMessageBufferCreate(sizeof(commandMessage) + sizeof(size_t));
  responseBuffer = xMessageBufferCreate(sizeof(responseMessage) + sizeof(size_t));

  // we create a task for the second (unused) core of the esp32
  // this task will communicate with the web site while the other core
  // is busy talking to the C64
  xTaskCreatePinnedToCore(
    WifiCoreLoop, /* Function to implement the task */
    "Task1",      /* Name of the task */
    10000,        /* Stack size in words */
    NULL,         /* Task input parameter */
    0,            /* Priority of the task */
    &Task1,       /* Task handle. */
    0);           /* Core where the task should run */


  // get the wifi mac address, this is used to identify the cartridge.
  commandMessage.command = GetWiFiMacAddressCommand;
  xMessageBufferSend(commandBuffer, &commandMessage, sizeof(commandMessage), portMAX_DELAY);
  xMessageBufferReceive(responseBuffer, &responseMessage, sizeof(responseMessage), portMAX_DELAY);
  macaddress = responseMessage.response.str;

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

  // get the last known message id (only the private is stored in eeprom)
  lastprivmsg = settings.getULong("lastprivmsg", 1);

  // get Chatserver ip/fqdn from eeprom
  server = settings.getString("server", "www.chat64.nl");

  ssid = settings.getString("ssid", "empty");  // get WiFi credentials and Chatserver ip/fqdn from eeprom
  password = settings.getString("password", "empty");
  timeoffset = settings.getString("timeoffset", "+0");  // get the time offset from the eeprom

  settings.putInt("invRST",(int)invert_reset_signal); // for future fuctionality
  settings.putInt("invNMI",(int)invert_nmi_signal);   // for future fuctionality
  settings.end();

  // define inputs
#ifndef VICE_MODE
  pinMode(sdata, INPUT);
  pinMode(C64IO1, INPUT_PULLDOWN);
  pinMode(C64IO2, INPUT_PULLUP);
  pinMode(resetSwitch, INPUT_PULLUP);

  // define interrupts
  attachInterrupt(C64IO1, isr_io1, RISING);          // interrupt for io1, C64 writes data to io1 address space
  attachInterrupt(C64IO2, isr_io2, FALLING);         // interrupt for io2, c64 reads
  attachInterrupt(resetSwitch, isr_reset, FALLING);  // interrupt for reset button

  // define outputs
  pinMode(internalLED, OUTPUT);
  digitalWrite(internalLED, LOW);
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

  pinMode(pload, OUTPUT);
  digitalWrite(pload, LOW);  // must be low to load parallel data
  pinMode(sclk, OUTPUT);
  digitalWrite(sclk, LOW);  //data shifts to serial data output on the transition from low to high.

  // Reset the C64, toggle the output pin
  uint32_t new_state = 1 - (GPIO.out >> oC64RST & 0x1);
  digitalWrite(oC64RST, new_state);
  delay(250);
  digitalWrite(oC64RST, !new_state);


#endif

  // start wifi
  commandMessage.command = WiFiBeginCommand;
  xMessageBufferSend(commandBuffer, &commandMessage, sizeof(commandMessage), portMAX_DELAY);

  // load the prg file
  loadPrgfile();

  commandMessage.command = GetWiFiLocalIpCommand;
  xMessageBufferSend(commandBuffer, &commandMessage, sizeof(commandMessage), portMAX_DELAY);
  xMessageBufferReceive(responseBuffer, &responseMessage, sizeof(responseMessage), portMAX_DELAY);
  String localIp = responseMessage.response.str;

  // check if we are connected to wifi
  if (isWifiCoreConnected) {
    // check the connection with the server.
    // ConnectivityCheck();

#ifdef debug
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(localIp);
    Serial.print("Name of the server (from eeprom): ");
    Serial.println(server);
#endif

  } else {
// if there is no wifi, the user can change the credentials in cartridge menu
#ifdef debug
    Serial.print("NO Wifi connection! : ");
    Serial.println(localIp);
#endif
  }

}  // end of setup

// ******************************************************************************
// Main loop
// ******************************************************************************
unsigned long ledtimer = 0;
int pos1 = 0;
int pos0 = 0;
void loop() {
  digitalWrite(CLED, isWifiCoreConnected);

  if (internalLEDstatus) {
    digitalWrite(internalLED, HIGH);
    ledtimer = millis() + 100;
    internalLEDstatus = false;
  }

  if (ledtimer < millis()) {
    digitalWrite(internalLED, LOW);
  }

  digitalWrite(oC64D7, HIGH);

#ifdef VICE_MODE
  if (accept_serial_command) {
    accept_serial_command = false;
    send_serial_data(0x80);
  }

  receive_serial_command();
#endif

  if (dataFromC64) {
    dataFromC64 = false;
    digitalWrite(oC64D7, LOW);  // flow control
#ifdef debug
    Serial.print("incomming command: ");
    Serial.println(ch);
#endif

    // generate an error if wifi connection drops
    if (wificonnected && !isWifiCoreConnected) {
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
    // 228 = debug purposes
    // 128 = end marker, ignore

    switch (ch) {
      case 254:
        {
          // ------------------------------------------------------------------------------
          // start byte 254 = C64 triggers call to the website for new public message
          // ------------------------------------------------------------------------------
          
          // send urgent messages first
          doUrgentMessage(); 

          msgtype = "public";

          // do we have any messages in the page buffer?
          // find the first '{' in the page buffer
          int p = 0;
          char cc = 0;
          bool found = false;
          // find first {
          while (cc != '{' and p < 10) {
            cc = multiMessageBufferPub[pos0++];

            p++;
          }
          // fill buffer until we find '}'
          if (cc == '{') {
            msgbuffer[0] = cc;
            found = true;
            getMessage = false;
            p = 1;
            while (cc != '}') {
              cc = multiMessageBufferPub[pos0++];
              // put this line into the msgbuffer buffer
              if (cc != 10) msgbuffer[p++] = cc;
            }
          }
          if (found) {
            found = false;
            Deserialize();
          } else {
            // clear the buffer
            for (int y = 0; y < 3500; y++) {
              multiMessageBufferPub[y] = 0;
            }
            pos0 = 0;
            getMessage = true;
          }



          if (haveMessage == 1) {
            // copy the msgbuffer to the outbuffer
            for (int x = 0; x < msgbuffersize; x++) { outbuffer[x] = msgbuffer[x]; }

            // copy the buffer size also
            outbuffersize = msgbuffersize;

            // and send the outbuffer
            send_out_buffer_to_C64();
            // store the new message id
            if (haveMessage == 1) {
              // store the new message id
              messageIds[0] = tempMessageIds[0];
            }
            haveMessage = 0;
          } else {  // no public messages :-(
            sendByte(128);
          }
          // if the user list is empty, get the list
          // also refresh the userlist when we switch from public to private messaging and vice versa
          if (users.length() < 1 or msgtype != "public") updateUserlist = true;
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
          int mstart = 0;
          String colorCode = "[145]";
          // Get the RecipientName
          // see if the message starts with '@'
          byte b = inbuffer[1];
          if (b == 0) {
            toEncode = "[" + String(int(inbuffer[0])) + "]";
            for (int x = 2; x < 15; x++) {
              byte b = inbuffer[x];
              if (b != 32) {
                if (b < 127) {
                  RecipientName = (RecipientName + screenCode_to_Ascii(b));
                  Serial.print(screenCode_to_Ascii(b));
                } else {
                  colorCode = "[" + String(int(b)) + "]";
                  Serial.print(colorCode);
                }
              } else {
                mstart = x + 1;
                toEncode = toEncode + "@" + RecipientName + " " + colorCode;
                break;
              }
            }
          }

          for (int x = mstart; x < inbuffersize; x++) {
            inbuffer[x] = screenCode_to_Ascii(inbuffer[x]);
            byte b = inbuffer[x];
            if (b > 128) {
              toEncode = (toEncode + "[" + int(inbuffer[x]) + "]");
            } else {
              toEncode = (toEncode + inbuffer[x]);
            }
          }
          Serial.println("toEncode = " + toEncode);

          if (RecipientName != "") {
            // is this a valid username?

            String test_name = RecipientName;
            test_name.toLowerCase();
#ifdef debug
            Serial.print("known users: ");
            Serial.println(users);
            Serial.print("Name under test: ");
            Serial.println(test_name);
#endif
            if (users.indexOf(test_name + ';') >= 0) {
              // user exists
              msgtype = "private";
              pmSender = '@' + RecipientName;
            } else {
              // user does not exist
#ifdef debug
              Serial.println("Username not found in list");
#endif
              urgentMessage = "System:  Unknown user:" + RecipientName;
              send_error = 1;
              break;
            }
          } else {
            msgtype = "public";
          }

          int buflen = toEncode.length() + 1;
          char buff[buflen];
          toEncode.toCharArray(buff, buflen);
          String Encoded = my_base64_encode(buff, buflen);

          // Now send it with retry!
          bool sc = false;
          int retry = 0;
          while (sc == false and retry < 2) {
            commandMessage.command = SendMessageToServerCommand;
            Encoded.toCharArray(commandMessage.data.sendMessageToServer.encoded, sizeof(commandMessage.data.sendMessageToServer.encoded));
            RecipientName.toCharArray(commandMessage.data.sendMessageToServer.recipientName, sizeof(commandMessage.data.sendMessageToServer.recipientName));
            commandMessage.data.sendMessageToServer.retryCount=retry;
            xMessageBufferSend(commandBuffer, &commandMessage, sizeof(commandMessage), portMAX_DELAY);
            xMessageBufferReceive(responseBuffer, &responseMessage, sizeof(responseMessage), portMAX_DELAY);
            sc = responseMessage.response.boolean;
            // sending the message fails, take a short break and try again             
            if (!sc) {
              delay(1000);
              retry = retry + 1;
            }
          }
          // if it still fails after a few retries, give us an error.
          if (!sc) {
            urgentMessage = "  System:     ERROR sending the message";
            send_error = 1;
          } else {
            // No error, read the message back from the database to show it on screen
            getMessage = true;  // get the message we just inserted
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

          ssid = getValue(ns, 129, 0);
          ssid.trim();
          Serial.println(ssid);
          password = getValue(ns, 129, 1);
          password.trim();
          Serial.println(password);
          timeoffset = getValue(ns, 129, 2);
          timeoffset.trim();
          Serial.println(timeoffset);

          settings.begin("mysettings", false);
          settings.putString("ssid", ssid);
          settings.putString("password", password);
          settings.putString("timeoffset", timeoffset);
          settings.end();
          commandMessage.command = WiFiBeginCommand;
          xMessageBufferSend(commandBuffer, &commandMessage, sizeof(commandMessage), portMAX_DELAY);
          break;
        }

      case 251:
        {
          // ------------------------------------------------------------------------------
          // start byte 251 = C64 ask for the current wifi ssid,password and time offset
          // ------------------------------------------------------------------------------
          send_String_to_c64(ssid + char(129) + password + char(129) + timeoffset);
          break;
        }

      case 249:
        {
          // ------------------------------------------------------------------------------
          // start byte 249 = C64 asks if this is an existing user (for private chat)
          // ------------------------------------------------------------------------------
          sendByte(send_error);
          sendByte(128);          
          Serial.print("Send error = ");
          Serial.println(send_error);
          send_error = 0;
          break;
        }

      case 248:
        {
          // ------------------------------------------------------------------------------
          // start byte 248 = C64 ask for the wifi connection status
          // ------------------------------------------------------------------------------

          if (!isWifiCoreConnected) {
            digitalWrite(CLED, LOW);
            sendByte(146);
            send_String_to_c64("Not Connected to Wifi");
          } else {
            commandMessage.command = GetWiFiLocalIpCommand;
            xMessageBufferSend(commandBuffer, &commandMessage, sizeof(commandMessage), portMAX_DELAY);
            xMessageBufferReceive(responseBuffer, &responseMessage, sizeof(responseMessage), portMAX_DELAY);
            String localIp = responseMessage.response.str;

            digitalWrite(CLED, HIGH);
            sendByte(149);
            String wifi_status = "Connected with ip " + localIp;
            send_String_to_c64(wifi_status);
          }
          break;
        }

      case 247:
        {
          // ------------------------------------------------------------------------------
          // start byte 247 = C64 triggers call to the website for new private message
          // ------------------------------------------------------------------------------          
          // send urgent messages first
          doUrgentMessage();          

          msgtype = "private";
          pmCount = 0;
          // do we have any messages in the page buffer?
          // find the first '{' in the page buffer
          int p = 0;
          char cc = 0;
          bool found = false;
          // find first {
          while (cc != '{' and p < 10) {
            cc = multiMessageBufferPriv[pos1++];
            p++;
          }
          // fill buffer until we find '}'
          if (cc == '{') {
            msgbuffer[0] = cc;
            found = true;
            getMessage = false;
            p = 1;
            while (cc != '}') {
              cc = multiMessageBufferPriv[pos1++];
              // put this line into the msgbuffer buffer
              if (cc != 10) msgbuffer[p++] = cc;
            }
          }
          if (found) {
            found = false;
            Deserialize();
          } else {
            // clear the buffer
            for (int y = 0; y < 3500; y++) {
              multiMessageBufferPriv[y] = 0;
            }
            pos1 = 0;
            getMessage = true;
          }
          if (haveMessage == 2) {
            // copy the msgbuffer to the outbuffer
            for (int x = 0; x < msgbuffersize; x++) { outbuffer[x] = msgbuffer[x]; }

            // copy the buffer size also
            outbuffersize = msgbuffersize;

            // and send the outbuffer
            send_out_buffer_to_C64();
            if (haveMessage == 2) {
              // store the new message id
              messageIds[1] = tempMessageIds[1];
              lastprivmsg = tempMessageIds[1];
              settings.begin("mysettings", false);
              settings.putULong("lastprivmsg", lastprivmsg);
              settings.end();
            }
            haveMessage = 0;
          } else {  // no private messages :-(
            sendByte(128);
          }
          // if the user list is empty, get the list
          // also refresh the userlist when we switch from public to private messaging and vice versa
          if (users.length() < 1 or msgtype != "private") updateUserlist = true;
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
          ns.trim();
          server = ns;
          settings.begin("mysettings", false);
          settings.putString("server", ns);  // store the new server name in the eeprom settings
          settings.end();

          messageIds[0] = 0;
          messageIds[1] = 0;

          // we should also refresh the userlist
          users = "";
          break;
        }
      case 245:
        {
          // -----------------------------------------------------------------------------------------------------
          // start byte 245 = C64 checks if the Cartrdidge is connected at all.. or are we running in a simulator?
          // -----------------------------------------------------------------------------------------------------
          // receive the ROM version number
          receive_buffer_from_C64(1);
          char bns[inbuffersize + 1];
          // filter out any unwanted bytes, keep only ./01234567890
          for (int k=0; k < inbuffersize ; k++){            
            if (inbuffer[k] < 45 or inbuffer[k] > 57) inbuffer[k] = 32;                            
          } 
          strncpy(bns, inbuffer, inbuffersize + 1);
          String ns = bns;
          ns.replace(" ","");
          romVersion = ns;
          // respond with byte 128 to tell the commodore the cartridge is present
          sendByte(128);
          pastMatrix = true;
          getMessage = true;
#ifdef debug
          Serial.print("ROM Version=");
          Serial.println(romVersion);
          Serial.println("are we in the Matrix?");
#endif
          break;
        }
      case 244:
        {
          // ---------------------------------------------------------------------------------
          // start byte 244 = C64 sends the command to reset the cartridge to factory defaults
          // ---------------------------------------------------------------------------------
          // this will reset all settings
          receive_buffer_from_C64(1);
          char bns[inbuffersize + 1];
          strncpy(bns, inbuffer, inbuffersize + 1);
          String ns = bns;
          Serial.println(ns);
          if (ns.startsWith("RESET!")) {
            settings.begin("mysettings", false);
            settings.putString("regID", "unregistered!");
            settings.putString("myNickName", "empty");
            settings.putString("ssid", "empty");
            settings.putString("password", "empty");
            settings.putString("server", "empty");
            settings.putString("configured", "empty");
            settings.putString("timeoffset", "+0");
            settings.end();
            // now reset the esp
            reboot();
          }
          break;
        }

      case 243:
        {
          // ------------------------------------------------------------------------------
          // start byte 243 = C64 ask for the mac address, registration id, nickname and regstatus
          // ------------------------------------------------------------------------------
          commandMessage.command = GetRegistrationStatusCommand;
          xMessageBufferSend(commandBuffer, &commandMessage, sizeof(commandMessage), portMAX_DELAY);
          xMessageBufferReceive(responseBuffer, &responseMessage, sizeof(responseMessage), portMAX_DELAY);
          regStatus = responseMessage.response.str[0];
          send_String_to_c64(macaddress + char(129) + regID + char(129) + myNickName + char(129) + regStatus);
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
          if (pmCount > 10) pmCount = 10;
          String pm = String(pmCount);
          if (pmCount < 10) { pm = "0" + pm; }
          pm = "[pm:" + pm + " (F5)]";
          if (pmCount == 0) pm = "~~~~~~~~~~~~";
          sendByte(156);           // send color code for gray
          send_String_to_c64(pm);  // then send the number of messages as a string
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

          regID = getValue(ns, 129, 0);
          regID.trim();
#ifdef debug
          Serial.println(regID);
#endif
          if (regID.length() != 16) {
#ifdef debug
            Serial.println("Registration code length should be 16");
#endif
            break;
          }
          myNickName = getValue(ns, 129, 1);
          myNickName.trim();
          myNickName.replace(' ','_');
#ifdef debug
          Serial.println(myNickName);
#endif

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
          commandMessage.command = ConnectivityCheckCommand;
          xMessageBufferSend(commandBuffer, &commandMessage, sizeof(commandMessage), portMAX_DELAY);
          break;
        }

      case 237:
        {
          // ------------------------------------------------------------------------------
          // start byte 237 = C64 triggers call to receive connection status
          // ------------------------------------------------------------------------------
          sendByte(ResultColor);  // send color code for green if connected
          send_String_to_c64(ServerConnectResult);
          break;
        }

      case 236:
        {
          // ------------------------------------------------------------------------------
          // start byte 236 = C64 asks for the server configuration status and servername
          // ------------------------------------------------------------------------------
          send_String_to_c64(configured + char(129) + server + char(129) + SwVersion);
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
 
#ifdef VICE_MODE
    accept_serial_command = true;
#endif

  }  // end of "if (dataFromC64)"

}  // end of main loop

// ******************************************************************************
// void to set a byte in the 74ls244 buffer
// ******************************************************************************
void outByte(byte c) {
#ifdef VICE_MODE
  send_serial_data(c);
#else
  digitalWrite(oC64D0, bool(c & B00000001));
  digitalWrite(oC64D1, bool(c & B00000010));
  digitalWrite(oC64D2, bool(c & B00000100));
  digitalWrite(oC64D3, bool(c & B00001000));
  digitalWrite(oC64D4, bool(c & B00010000));
  digitalWrite(oC64D5, bool(c & B00100000));
  digitalWrite(oC64D6, bool(c & B01000000));
  digitalWrite(oC64D7, bool(c & B10000000));
#endif
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

#ifdef VICE_MODE
    send_serial_data(0x80);
#endif

    while (dataFromC64 == false) {
      delayMicroseconds(2);  // wait for next byte
      if (millis() > timeOut) {
        ch = 128;
        dataFromC64 = true;
#ifdef debug
        Serial.println("Timeout in receive buffer");
#endif
      }

#ifdef VICE_MODE
      receive_serial_command();
#endif
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
    if (ch == 128) {
      cnt--;
      inbuffer[i] = 129;
      i++;
    }
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

#ifdef VICE_MODE
  send_serial_nmi();
#else

  // toggle NMI
  uint32_t new_state = 1 - (GPIO.out >> oC64NMI & 0x1);
  digitalWrite(oC64NMI, new_state);
  delayMicroseconds(175);  // minimal 100 microseconds delay
  // And toggle back
  digitalWrite(oC64NMI, !new_state);

#endif
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

#ifdef VICE_MODE
    receive_serial_command();
#endif
  }
  io2 = false;
}

// ******************************************************************************
// Deserialize the json encoded messages
// ******************************************************************************
void Deserialize() {

  DynamicJsonDocument doc(512);                                  // next we want to analyse the json data
  DeserializationError error = deserializeJson(doc, msgbuffer);  // deserialize the json document

  if (!error) {
    unsigned long newMessageId = doc["rowid"];
    // if we get a new message id back from the database, that means we have a new message
    // if the database returns the same message id, there is no new message for us..
    bool newid = false;
    String channel = doc["channel"];
    if ((channel == "private") and (newMessageId != messageIds[1])) {
      newid = true;
      tempMessageIds[1] = newMessageId;
      String nickname = doc["nickname"];
    }

    if ((channel == "public") and (newMessageId != messageIds[0])) {
      newid = true;
      tempMessageIds[0] = newMessageId;
    }
    if (newid) {
      String message = doc["message"];
      String decoded_message = ' ' + my_base64_decode(message);
      int lines = doc["lines"];
      int msize = decoded_message.length() + 1;
      decoded_message.toCharArray(msgbuffer, msize);
      int outputLength = decoded_message.length();
      msgbuffersize = (int)outputLength;
      msgbuffer[0] = lines;
      msgbuffersize += 1;

      pmCount = doc["pm"];
      haveMessage = 1;
      if (msgtype == "private") haveMessage = 2;

    } else {

      pmCount = doc["pm"];

      // we got the same message id back, so no new messages:
      msgbuffersize = 0;
      haveMessage = 0;
    }
  }
}

// ******************************************************************************
// Send out urgent message if available (error messages)
// ******************************************************************************
void doUrgentMessage(){
  if (urgentMessage != ""){
    Serial.println(urgentMessage);
    urgentMessage = "   " + urgentMessage;       
    outbuffersize = urgentMessage.length() + 1;     
    urgentMessage.toCharArray(outbuffer, msgbuffersize);
    outbuffer[0] = 1;
    outbuffer[1] = 143;
    outbuffer[2] = 146;
    send_out_buffer_to_C64();            
    urgentMessage = "";
  }
}

void loadPrgfile() {

  int startaddress = (prgfile[1] * 0x100) + prgfile[0];  // get the start address from the prg file ($0801)
  int endaddress = startaddress + sizeof(prgfile) - 2;   // calculate the end address

  delay(2000);  // een delay om de C64 tijd te geven om op te starten

  Serial.println("Wait for c64 to send 100");
  // wait for the c64 to send byte 100
  while (ch != 100) {
#ifdef VICE_MODE
      receive_serial_command();
#endif
    // do nothing
  }
  delay(10);
  // Okay, lets start
  Serial.println("------ LOAD PRG FILE ------");

  sendByte(20);  // first send the border color during loading 0-15, default = 14, 20 is blink (loading bars)
  sendByte(0);   // send the screen color during loading 0-15, default = 6, 20 is blink (loading bars)


  sendByte(prgfile[0]);  // send the start address (low byte = 01)
  sendByte(prgfile[1]);  // send the start address (high byte = 08)

  sendByte(lowByte(endaddress));   // send the END address (low byte)
  sendByte(highByte(endaddress));  // send the END address (high byte)

  // Now send all the rest of the bytes
  for (int x = 2; x < sizeof(prgfile); x++) {
    sendByte(prgfile[x]);
  }
  sendByte(0);
  sendByte(0);
  Serial.println("------ PRG FILE DONE ------");
}
