# Chat64

**The Chat Cartridge for the Commodore64.**

When flashing the ESP32 you need to make sure you have installed following tools and libraries:

---
### Arduino IDE
Download and install the Arduino IDE

[Download here](https://www.arduino.cc/en/software)

---

Within the Arduino IDE go to  File > Preferences

![Preferences](/Artwork/arduino-preferences.png)

and enter following line in the section Additional Boards Manager URLs

 `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json, http://arduino.esp8266.com/stable/package_esp8266com_index.json `


You will now be able to select the correct board. make sure you choose `WEMOS D1 MINI ESP32`

![SelectBoard](/Artwork/arduino-boardselect.png)


Next you will need to install a few libraries.
go to Tools > Manage Libraries

![ManageLibraries](/Artwork/arduino-managelibraries.png)

The following window will appear

![LibraryManager](/Artwork/arduino-librarymanager.png)

Make sure you have installed the following libraries. If one of the libraries is missing, compiling the code will result in an error. If that happens search and install the following libraries:

* ArduinoJson &nbsp;by Benoit Blanchon
* WiFi &emsp;&emsp;&emsp;&ensp;&nbsp;built in by Arduino
* HttpClient &emsp;&nbsp;by Adrian McEwen

You may need to install a driver for your operating system. Depending on the USB to serial chip on your ESP board you ma

---
### Windows Driver for the ESP32

- If your ESP32 uses the CH9102F USB-to-Serial Chip. You'll need the following driver for windows 10/11 [Download here](http://chat64.nl/drivers/CH343SER.zip)

- If your ESP32 uses the CP210x USB-to-Serial Chip. You'll need the following driver for windows 10/11 [Download here](http://chat64.nl/drivers/CP210x_Windows_Drivers_with_Serial_Enumeration.zip)

---
### Compile the code and upload to your ESP32
In the Arduino IDE go to Tools > Ports and select the port where your ESP32 is connected to. In this example it's COM3. This might be different on your computer.

![Port](/Artwork/arduino-selectport.png)

After this you are able to compile the code and upload it to your ESP32. For this you can use the designated button within the Arduino IDE

![Port](/Artwork/arduino-upload.png)

When compilation and upload had succeeded you will see a notification in the lower part of your Adruino IDE where all messages are being displayed.

---


By Sven Pook

13-03-2024
