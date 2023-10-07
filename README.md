# Richonguzman / CD2RXU LoRa APRS Tracker/Station

This firmware is for using ESP32 based boards with LoRa Modules and GPS to live in the APRS world.

![Screenshot](https://github.com/richonguzman/LoRa_APRS_Tracker/blob/main/images/OledScreen.jpeg)

(NOTE: To use Tx/Rx capabilities of this tracker you should have also an Tx/Rx <a href="https://github.com/richonguzman/LoRa_APRS_iGate" target="_blank">LoRa iGate</a> near you)

____________________________________________________

## You can support this project to continue to grow:

[<img src="https://github.com/richonguzman/LoRa_APRS_Tracker/blob/main/images/github-sponsors.png">](https://github.com/sponsors/richonguzman)     [<img src="https://github.com/richonguzman/LoRa_APRS_Tracker/blob/main/images/paypalme.png">](http://paypal.me/richonguzman)

____________________________________________________
- NOW WE HAVE A MENU !!! (just pushing the central button IO38)
    - Saving, Reading and Deleting Messages.
    - Asking Weather Report
    - Listening to other Trackers arround.
    - Changing Display Eco Mode (turn off after 4 seg).
- Processor from 240Mhz to 80MHz to save almost 20% power consumption.
- All GPS beacons/packet are encoded for less time on RF/LoRa Tx.
- Oled Screen shows Altitude+Speed+Course or BME280 Wx Data or Number of New Messages Received.
- Oled Screen shows Recent Heard Trackers/Station/iGates Tx.
- Bluetooth capabilities to connect Android + APRSDroid and use it as TNC.
- Led Notifications for Tx and Messages Received
- Sound Notifications with YL44 Buzzer Module
- Wx data with BME280 Module showed on Screen and transmited as Wx Telemetry.


____________________________________________________

# WIKI

### 1. Installation Guide --> <a href="https://github.com/richonguzman/LoRa_APRS_Tracker/wiki/1.-Installation-Guide" target="_blank">here</a>.

### 2. Tracker Configuration and Explanation for each setting --> <a href="https://github.com/richonguzman/LoRa_APRS_Tracker/wiki/2.-Tracker-Configuration" target="_blank">here</a>.

### 3. Supported Boards and Environment Selection --> <a href="https://github.com/richonguzman/LoRa_APRS_Tracker/wiki/3.-Supported-Boards-and-Environment-Selection" target="_blank">here</a>.

### 4. Upload Firmware and Filesystem --> <a href="https://github.com/richonguzman/LoRa_APRS_Tracker/wiki/4.-Upload-Firmware-and-Filesystem" target="_blank">here</a>.

### 5. Tracker Menu Guide --> <a href="https://github.com/richonguzman/LoRa_APRS_Tracker/wiki/5.-Menu-Guide" target="_blank">here</a>.

### 6. Bluetooth Guide --> <a href="https://github.com/richonguzman/LoRa_APRS_Tracker/wiki/6.-Bluetooth-Connection" target="_blank">here</a>.

### 7. First Time Boot and GPS --> <a href="https://github.com/richonguzman/LoRa_APRS_Tracker/wiki/7.-First-Time-Boot-and-GPS" target="_blank">here</a>.

### 8. Adding Leds, Buzzer and BME280 Modules --> <a href="https://github.com/richonguzman/LoRa_APRS_Tracker/wiki/8.-Adding-Leds,-Buzzer-and-BME280-Modules" target="_blank">here</a>.

____________________________________________________
## Timeline (Versions):

- 2023.10.07 Screen Brightness control added.
- 2023.10.01 Added Wx Telemetry Tx with BME280 Module attached to Tracker.
- 2023.09.28 Added Support for V.1 board with SX1268 LoRa Module
- 2023.09.25 Wiki added.
- 2023.09.16 Adding Led notification for Beacon Tx and for Message Received.
- 2023.09.14 Adding buzzer sounds for BootUp, BeaconTx, MessageRx and more.
- 2023.09.11 Saving last used Callsign into internal Memory to remember it at next boot.
- 2023.09.05 Adding "simplified Tracker Mode": only GPS beacons Tx.
- 2023.08.27 Adding support to connect BME280 and see Temperature, Humidity, Pressure.
- 2023.08.12 Adding also support for old V0_7 board. Thanks Béla Török.
- 2023.08.09 Adding Bluetooth capabilities with Kiss and TNC2, TTGO Lora 32. Thanks Thomas DL9SAU.
- 2023.08.08 Added Maidenhead info on Screen. Thanks Mathias "mpbraendli".
- 2023.08.06 Added Bluetooth Support for TNC in Android/APRSDroid. Thanks Valentin F4HVV.
- 2023.08.05 New Support for SH1106 Oled Screen (0,96" and 1.3")
- 2023.07.24 New Validation for Callsings, Overlay change and New Icons (Bike, Motorcycle).
- 2023.07.18 Add Support for triggering PTT to external amplifier.
- 2023.07.16 New Icons for Oled Screen (Runner, Car, Jeep)
- 2023.07.01 Added Support for new T-Beam AXP2101 v1.2 Board.
- 2023.06.26 Weather Report now stays until button pressed, to avoid missing it.
- 2023.06.25 Sends comment after X count of beacons.
- 2023.06.24 displayEcoMode=true doesn't turn the screen off at boot.
- 2023.06.23 Return to from any Menu number to Main Menu (Tracker) after 30 segs.
- 2023.06.20 Major Code Repacking.
- 2023.06.01 Adding Turn Slope calculations for Smart Beacon and Display Eco Mode.
- 2023.05.29 New Config file for adding more new ideas to the Tracker.
- 2023.05.27 Adding Altitude + Speed or Course + Speed in the encoded GPS info.
- 2023.05.21 Adding Last-Heard LoRa Stations/Trackers.
- 2023.05.14 Adding Menu.
- 2023.05.12 Saving Messages to Internal Memory.
- 2023.04.16 Sending and Receiving LoRa Packets.


____________________________________________________
## This code was based on the work of :
- https://github.com/aprs434/lora.tracker : Serge - ON4AA on base91 byte-saving/encoding
- https://github.com/lora-aprs/LoRa_APRS_Tracker : Peter - OE5BPA LoRa Tracker
- https://github.com/Mane76/LoRa_APRS_Tracker : Manfred - DC2MH (Mane76) mods for multiple Callsigns and processor speed
- https://github.com/dl9sau/TTGO-T-Beam-LoRa-APRS : Thomas - DL9SAU for the Kiss <> TNC2 lib
____________________________________________________

# Hope You Enjoy this, 73 !!  CD2RXU , Valparaiso, Chile