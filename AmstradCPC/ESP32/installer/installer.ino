// Installer for Chat64, Amstrad CPC Version

#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>


const char* WIFI_SSID = "YOUR_WIFI_SSID";          //    <------------   CHANGE THIS
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  //    <------------   CHANGE THIS



const char* FIRMWARE_URL = "https://raw.githubusercontent.com/bvenneker/CHAT64_C64/main/AmstradCPC/firmware/CPC_Chat.bin";

Preferences settings;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Connecting to WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting firmware installation...");

  String ssid = WIFI_SSID;
  String password = WIFI_PASSWORD;
  settings.begin("mysettings", false);
  settings.putString("ssid", ssid);
  settings.putString("password", password);
  settings.end();

  WiFiClientSecure client;
  client.setInsecure();
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  
  httpUpdate.onProgress(updateProgress);
  t_httpUpdate_return result = httpUpdate.update(client, FIRMWARE_URL);

  switch (result) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("Update failed (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("No firmware available.");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("Installation successful!");
      Serial.println("Wait for reboot");
      Serial.println();
      Serial.println();
      break;
  }
}

void updateProgress(int current, int total) {
  static int lastPrinted = -5;
  if (total > 0) {
    int percent = (current * 100) / total;
    int progress = (percent / 10) * 10;
    if (progress != lastPrinted) {
      lastPrinted = progress;
      Serial.printf("\nUpdating: %d%%  (%d / %d bytes)",
                    progress, current, total);
    } else Serial.print("*");
  }
}

void loop() {
}
