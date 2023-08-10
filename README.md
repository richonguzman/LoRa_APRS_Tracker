# Richonguzman / CD2RXU LoRa APRS Tracker/Station
# (Firmware for Tx and Rx !!!)

NOTE: To take advantage of Tx/Rx capabilities you should have an Tx/R   x LoRa iGate (near you) like:

https://github.com/richonguzman/LoRa_APRS_iGate

____________________________________________________
- NOW WE HAVE A MENU (just pushing the central button IO38)
    - Saving, Reading and Deleting Messages.
    - Asking Weather Report
    - Listening to other Trackers arround.
    - Changing Display Eco Mode (turn off after 4 seg).
- Processor from 240Mhz to 80MHz to save almost 20% power consumption (from ~ 100mA to almost ~80mA) (Thanks Mane76).
- All GPS beacons/packet are encoded for less time on RF/LoRa Tx.
- 4th line of the OLED SCREEN shows Altitude+Speed+Course or Number of New Messages Received.
- 5th line of the OLED SCREEN shows Recent Heard Trackers/Station/iGates Tx.
____________________________________________________

# INSTRUCTIONS:
- (1). Change _Callsign_, _Symbol_ and _Comment_ on /data/tracker_config.json
- (2). Upload this changes via Platformio --> Upload Filesystem Image (to your TTGO Board)
- (3). Build and Upload the Firmware with Platformio in VSCODE

____________________________________________________

# MENU EXPLANATION

on the Tracker Screen/Menu 0:
- 1 short press/push   = Forced GPS Beacon Tx
- 1 long press/push    = Change between three Callsigns saved on "/data/tracker.json".
- 2 short press/pushes = Menu 1 (where you cand read Messages)

on the Menu 1:
- 1 short press/push   = Read Received Messages saved on internal Memory.
- 1 long press/push    = Delete all Messages from internal Memory.
- 2 short press/pushes = Menu 2 (where you cand ask for Weather Report and more).

on the Menu 2:
- 1 short press/push   = Ask for Weather Report (WX report will arrive in seconds).
- 1 long press/push    = Listen to other Trackers and show distance and course to them.
- 2 short press/pushes = Menu 3 (where you cand change Display Eco Mode and more).

on the Menu 3:
- 1 short press/push   = NOTHING YET... (any ideas?).
- 1 long press/push    = Change Display Eco Mode (Turn off after 4 seg).
- 2 short press/pushes = Menu 0 (back to the Tracker Screen).

____________________________________________________

# BLUETOOTH EXPLANATION (**Only for Android**)

## APRSDroid

- Pair your phone with the tracker. Its name is "Lora Tracker XXXX"
- Install [APRSDroid](https://aprsdroid.org/) app
- Open app and go to Settings and click on connection preferences
- Protocol : TNC2 or Kiss
- Type : BLuetooth SPP
- Module : Select our tracker name
- Tadam !

## ShareGps (NMEA)

- Pair your phone with the tracker. Its name is "Lora Tracker XXXX"
- Install [ShareGPS](https://play.google.com/store/apps/details?id=com.jillybunch.shareGPS&pcampaignid=web_share) app
- Open app and go to Connections tab
- Click on add button
- Choose NMEA as Data Type
- Choose Bluetooth as Connectoin Method
- Name it and click next to set you tracker
- To connect to it : long press on the connection name and click connect
- BT is listening, repeat the operation a second time to initiate the connection
- Tadam !

____________________________________________________
Timeline (Versions):
- 2023.08.09 Adding Bluetooth capabilities with Kiss and TNC2, TTGO Lora 32
- 2023.04.16 Sending and Receiving LoRa Packets.
- 2023.05.12 Saving Messages to Internal Memory.
- 2023.05.14 Adding Menu.
- 2023.05.21 Adding Last-Heard LoRa Stations/Trackers
- 2023.05.27 Adding Altitude + Speed or Course + Speed in the encoded GPS info.
- 2023.05.29 New Config file for adding more new ideas to the Tracker.
- 2023.06.01 Adding Turn Slope calculations for Smart Beacon and Display Eco Mode.
- 2023.06.20 Major Code Repacking.
- 2023.06.23 Return to from any Menu number to Main Menu (Tracker) after 30 segs.
- 2023.06.24 displayEcoMode=true doesn't turn the screen off at boot.
- 2023.06.25 Sends comment after X count of beacons.
- 2023.06.26 Weather Report now stays until button pressed, to avoid missing it.
- 2023.07.01 Added Support for new T-Beam AXP2101 v1.2 Board.
- 2023.07.16 New Icons for Oled Screen (Runner, Car, Jeep)
- 2023.07.18 Add Support for triggering PTT to external amplifier.
- 2023.07.24 New Validation for Callsings, Overlay change and New Icons (Bike, Motorcycle).
- 2023.08.05 New Support for SH1106 Oled Screen (0,96" and 1.3")
- 2023.08.06 Added Bluetooth Support for TNC in Android/APRSDroid. Thanks Valentin F4HVV
- 2023.08.08 Added Maidenhead info (now changes between GPS and Maidenhead on Screen) Thanks Mathias "mpbraendli"
____________________________________________________
This code was based on the work of :
- Serge Y. Stroobandt : base91 and others ideas
- https://github.com/aprs434/lora.tracker : ON4AA in the byte-saving part of the APRS 434 firmware
- https://github.com/lora-aprs/LoRa_APRS_Tracker : OE5BPA LoRa Tracker
- https://github.com/Mane76/LoRa_APRS_Tracker : Manfred DC2MH (Mane76) with the mods for multiple Callsigns and processor speed
- https://github.com/dl9sau/TTGO-T-Beam-LoRa-APRS : DL9SAU for the Kiss <> TNC2 lib
____________________________________________________

# Hope You Enjoy this, 73 !!  CD2RXU , Valparaiso, Chile