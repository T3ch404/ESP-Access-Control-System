# ESP-Access-Control-System
The ESP Access Control System is a project that uses affordable ESP-WROOM-32 microcontroller devboards available on [Amazon](https://www.amazon.com/Development-Microcontroller-Integrated-Antenna-Amplifiers/dp/B09GK74F7N/ref=sr_1_1_sspa?crid=1PY58QU6O3CGX&keywords=esp32&qid=1687741353&sprefix=esp32%2Caps%2C126&sr=8-1-spons&sp_csd=d2lkZ2V0TmFtZT1zcF9hdGY&psc=1), along with the HID ProxPro 5355 RFID reader, to create a simple door access controller. Created using Arduino code, this program creates a simple web interface enabling users to easily add, modify, or remove RFID cards for granting access to connected door systems.

### Installation
  1. Download [program files](https://github.com/T3ch404/ESP-Access-Control-System/archive/refs/heads/main.zip) and open in Arduino IDE
  2. Change ssid, password, httpUsername, and httpPassword variables
  3. Add predefined cards to the allowedCard list
  4. Upload to your ESP board

### ToDo list:
  - Review/standardize comments and variable names
  - Make the allowedCards list persistant
  - Add support for SD card logging
  - Add functionality to add new cards by scanning them in
  - Release the scamatics for the custom ESP32 Dev board addon board that I'm using for this project
  - More settings (time based access, relay latching/timing, etc.)

### Credit
Thanks [@dc540_nova](https://github.com/dc540) for the Wiegand protocol handling! [arduinohidprox](https://github.com/dc540/arduinohidprox)
