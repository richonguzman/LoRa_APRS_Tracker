# T-Beam 1W: BLE security and OLED activity wake

Base: upstream tag V4.0.1. Build label: 4.0.1-sec-oled.2.

## Behavior

- BLE KISS/TNC2 access requires encryption AND authenticated passkey pairing.
- Pairing displays a fresh six-digit PIN on the tracker; enter it on the phone.
- Bonding stores peer keys in NVS. Known phones can reconnect without another PIN.
- An unauthenticated connection is disconnected after 60 seconds.
- Only one BLE central is supported (also enforced in the NimBLE build configuration).
- Web configuration: select **Forget paired BLE devices when saving**, then save.
  The stored bonds are removed next time BLE starts. Also forget the tracker on
  the phone before pairing again. Existing configuration files need no reset.
- Valid LoRa APRS-format RX, TX and button activity wake the OLED. The existing
  display timeout still applies; TX restarts it after the blocking radio send.
- A first short press on a sleeping display retains upstream behavior: wake only.
- Regular menu redraws do not reset the timeout. Menu timeout behavior is unchanged.
- The pairing prompt temporarily keeps the display awake, for at most 60 seconds.

## Compatibility and provenance

The pairing design is adapted from Antonio Megia / EA4HXZ's GPL-licensed work:
https://github.com/armegia/LoRa_APRS_Tracker/tree/pr/diagnostics-ble-bonding
Discussion: https://github.com/richonguzman/LoRa_APRS_Tracker/issues/255

This change deliberately does not import that fork's logging/UI redesign.
It uses authenticated **BLE Legacy Passkey pairing**, as his tested NimBLE 1.4.1
compatibility approach does. This is not LE Secure Connections; captured initial
legacy pairing has weaker protection. No fallback to unauthenticated Just Works
is permitted. Bluetooth Classic behavior is unchanged.

The BLE callbacks enqueue bounded raw chunks; parsing, OLED access, and radio
transmission occur in the main loop. Disconnect/overflow discards queued or partial
frames. Session IDs prevent connection-handle reuse from authorizing stale data.
Outbound KISS chunks respect the peer's negotiated ATT MTU.

## Build

```
python -m pip install platformio
python -m platformio run -e ttgo-t-beam-1W
```

Firmware-only update for an existing V4.0.1 T-Beam 1W installation:
write `firmware.bin` at **0x10000**. Do not erase flash, write it at 0x0,
or upload a new filesystem image: those operations can erase settings/bonds or
make the board fail to boot. Export a configuration backup first.

Example with esptool (replace COM5 with the actual port):

```
python -m pip install esptool
python -m esptool --chip esp32s3 --port COM5 write_flash 0x10000 firmware.bin
```

## Validation

`python tests/host/run.py` executes host behavioral tests against the production
BLE implementation with mocked BLE/radio devices, and the actual display wake,
toggle and timeout functions with mocked display hardware. These checks do not
replace hardware pairing/interoperability testing.

Hardware acceptance checks still required on T-Beam 1W + Graywolf:

1. A fresh phone receives a PIN prompt; a wrong/cancelled PIN cannot send packets.
2. Correct PIN: phone-to-radio position and radio-to-phone message/ACK work.
3. Disconnect/reconnect and reboot preserve the bond, without another PIN prompt.
4. After clearing bonds, a fresh PIN is required again.
5. OLED sleeps after timeout, wakes on TX and RX, then sleeps again.
6. First short button press wakes; double/triple press menus still work.

No physical-device tests have been performed in this environment.

## Revision .2

During PIN pairing, both normal display render paths yield to the BLE prompt.
This prevents the periodic main screen and TX/RX messages from overwriting it.
Normal rendering resumes after authentication, disconnect or timeout.
