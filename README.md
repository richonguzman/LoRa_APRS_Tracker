# Richonguzman / CD2RXU LoRa APRS Tracker/Station
# (Firmware for Tx and Rx !!!)

NOTE: To take advantage of Tx/Rx capabilities you should have an Tx/R   x LoRa iGate (near you) like:

https://github.com/richonguzman/LoRa_APRS_iGate

____________________________________________________
Recent improvements:
- NOW WE HAVE A MENU (just pushing the central button IO38) !!!
    - Saving, Reading and Deleting Messages.
    - Asking Weather Report
- Processor from 240Mhz to 80MHz to save almost 20% power consumption (from ~ 100mA to almost ~80mA) (Thanks Mane76).
- 4th line of the OLED SCREEN shows New Messages Received.
- 5th line of the OLED SCREEN shows Recent Heard Trackers/Station/iGates Tx.
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
- 1 long press/push    = Listen to other Trackers (working on it!!!).
- 2 short press/pushes = Menu 0 (back to the Tracker Screen).


____________________________________________________
Versions:
- 2023.04.16 Sending and Receiving LoRa Packets.
- 2023.05.12 Saving Messages to Internal Memory.
- 2023.05.14 Adding Menu.
- 2023.05.21 Adding Last-Heard LoRa Stations/Trackers
- 2023.05.29 Adding Altitude + Speed or Course + Speed in the encoded GPS info.
____________________________________________________
This code was inspided by OE5BPA LoRa Tracker and uses byte-saving part of the APRS 434 firmware published by Serge Y. Stroobandt, ON4AA.
- https://github.com/aprs434/lora.tracker
- https://github.com/lora-aprs/LoRa_APRS_Tracker
____________________________________________________

# Hope You Enjoy this, 73 !!  CD2RXU , Valparaiso, Chile