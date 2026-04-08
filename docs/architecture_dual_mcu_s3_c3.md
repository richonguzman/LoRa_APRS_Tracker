# Architecture Dual MCU : ESP32-S3 (UI) + ESP32-C3 (Radio/GNSS)

## Contexte

Les ecrans tactiles 4.3" et plus (Elecrow CrowPanel, Waveshare) utilisent un bus RGB 16-bit parallele qui consomme ~16 GPIO de l'ESP32-S3. Il devient impossible de caser LoRa SPI + GNSS UART sur le meme chip.

Le design mono-chip atteint sa limite a 3.5" SPI (480x320, CrowPanel Advance 3.5").

## Architecture retenue

```
+-------------------------+          UART           +-------------------------+
|   ESP32-S3 (Waveshare   | <--------------------> |   ESP32-C3 DevKitC-02   |
|   Touch LCD 4.3")        |   230400-460800 baud   |                         |
|                          |                         |                         |
|   - LVGL UI              |   C3 TX -> S3 RX       |   - GNSS (UART)         |
|   - Carte / tuiles SD    |   C3 RX <- S3 TX       |   - LoRa SPI (SX1262)   |
|   - Messages / contacts  |   GND commun            |   - Protocole serie     |
|   - WiFi / BLE           |                         |                         |
+-------------------------+                         +-------------------------+
```

## Allocation peripheriques C3

### UART1 -- GNSS u-blox NEO-M8N
- TX/RX
- PPS -> GPIO (interrupt)

### SPI2 -- LoRa SX1262 (ou HT-RA62 en mode SPI)
- SCK / MOSI / MISO -> bus SPI
- CS -> GPIO
- RST -> GPIO
- BUSY -> GPIO
- DIO1 -> GPIO (IRQ)

**Attention** : les pins SPICS0/SPICLK/SPIQ/SPID du C3 sont reservees au flash interne. Utiliser d'autres GPIO pour le bus SPI LoRa.

### UART0 -- Liaison vers S3
- UART0 redirige vers le S3 (logs desactives en production)
- Debug via USB JTAG integre du C3

## Contraintes ESP32-C3

- **2 UART** : UART0 (USB/debug) + UART1. Pas d'UART2.
- **1 SPI libre** : SPI2. SPI0/SPI1 reserves au flash.
- **RISC-V** single core, 160 MHz
- Pas de PSRAM

## Organisation firmware

### C3 (backend radio/navigation)
- Lecture GNSS via UART1
- Gestion LoRa via SPI2 (RadioLib)
- Protocole serie binaire vers S3 (frames + CRC)
- ~1500 lignes estimees
- **Framework : ESP-IDF natif** (pas d'Arduino)

### S3 (frontend UI)
- Reception UART depuis C3
- Parsing leger des trames
- Affichage LVGL (code existant quasi inchange)
- **Framework : Arduino + LVGL** (code actuel)

## RadioLib sur ESP-IDF + C3

RadioLib supporte ESP-IDF natif via le HAL `EspHal`. Le code inclut les targets C3 via `CONFIG_IDF_TARGET_ESP32C3`.

### Statut verifie (avril 2026)
- Composant ESP-IDF officiel disponible (v7.0.x) sur le registre Espressif
- HAL EspHal compile pour C3
- Precedents utilisateurs avec SX1262 + C3 en ESP-IDF
- Le HAL exemple necessite une customisation des pins SPI pour le C3
- Le manifest ESP-IDF ne specifie pas correctement les targets (issue metadata, pas de code)

### References
- RadioLib ESP-IDF component : https://components.espressif.com/components/jgromes/radiolib
- Discussion HAL ESP-IDF : https://github.com/jgromes/RadioLib/discussions/1130
- C3 + SX1262 light sleep : https://github.com/jgromes/RadioLib/discussions/1015
- Issue targets manifest : https://github.com/jgromes/RadioLib/issues/1322

## Impact sur le code existant

### Fichiers inchanges cote S3 (~7300 lignes, ~80% du code)
- `lvgl_ui.cpp` -- init LVGL + navigation
- `ui_dashboard.cpp` -- ecran principal
- `ui_messaging.cpp` -- messages, conversations, contacts
- `ui_settings.cpp` -- ecrans de configuration
- `ui_popups.cpp` -- popups TX/RX/notifications
- `ui_map_manager.cpp` + `src/map/` -- carte
- `msg_utils.cpp` -- gestion messages

L'UI ne fait aucun appel direct SPI/UART hardware. Elle consomme des structures (`gpsFix`, `lastReceivedPacket`, `Config`).

### Fichiers a remplacer cote S3 (~4 fichiers)
- `gps_utils.cpp` -- remplacer lecture serie GPS par parsing UART depuis C3
- `lora_utils.cpp` -- remplacer RadioLib SPI par envoi/reception UART vers C3
- `LoRa_APRS_Tracker.cpp` -- simplifier main loop (plus d'init SPI radio/GPS)
- Nouveau `uart_link.cpp` -- protocole serie C3<->S3

### Firmware C3 (neuf, ~1500 lignes)
- Lecture GNSS (NeoGPS ou driver NMEA custom)
- LoRa (RadioLib + EspHal)
- Protocole UART vers S3
- Pas d'UI, pas de LVGL, pas de SD

## Protocole serie C3 <-> S3

### Recommandations
- Baud : 230400 ou 460800 (eviter 115200 si UI riche)
- Format : binaire compact (eviter JSON)
- Ring buffer cote S3 pour eviter pertes
- Frame : header + type + length + payload + CRC16

### Types de messages
- `GPS_FIX` : lat, lon, alt, speed, heading, sats, hdop, timestamp
- `LORA_RX` : packet APRS recu (raw + RSSI + SNR)
- `LORA_TX_REQ` : demande d'emission (S3 -> C3)
- `LORA_TX_ACK` : confirmation emission (C3 -> S3)
- `CONFIG` : changement frequence/puissance (S3 -> C3)
- `STATUS` : etat radio/GPS (heartbeat periodique)

## Avantages
- Pas de conflit GPIO
- UI fluide (S3 dedie LVGL)
- Radio stable (C3 temps reel)
- Ecrans 4.3" a 7" possibles
- Debug maitrisable (JTAG C3 + serial S3)

## Limites ecrans par design

| Taille | Bus      | Architecture    | Statut        |
|--------|----------|-----------------|---------------|
| 2.8"   | SPI      | Mono-chip S3    | Supporte      |
| 3.5"   | SPI      | Mono-chip S3    | Supporte (CrowPanel) |
| 4.3"   | RGB //   | Dual S3+C3      | A developper  |
| 5.0"   | RGB //   | Dual S3+C3      | A developper  |
| 7.0"   | RGB //   | Dual S3+C3      | A developper  |

## Cablage physique

- Niveau logique : 3.3V partout (S3 et C3)
- GND commun obligatoire
- Cables UART courts (eviter bruit)
- Alimentation : regulateur commun ou regulateurs separes avec GND commun

## Resume des decisions cles

1. **Separation stricte des responsabilites** : S3 = UI/LVGL + SD. C3 = GNSS + LoRa. Pas de partage de peripheriques entre les deux chips.

2. **Allocation C3 corrigee** :
   - UART1 -> GNSS NEO-M8N + PPS
   - SPI2 -> LoRa SX1262 (HT-RA62), pins hors SPI0/1 flash
   - CS/RST/BUSY/DIO1 sur GPIO disponibles
   - UART0 -> liaison S3 (logs USB desactives en prod, debug via JTAG)

3. **Firmware** :
   - S3 : code LVGL existant quasi inchange (~80%)
   - C3 : nouveau firmware ESP-IDF natif (~1500 lignes) : GNSS parsing + LoRa SPI RadioLib + protocole serie binaire

4. **Protocole serie** :
   - Binaire compact, header + type + longueur + payload + CRC16
   - Messages : GPS_FIX, LORA_RX, LORA_TX_REQ/ACK, CONFIG, STATUS
   - Baud 230400-460800, ring buffer cote S3

5. **Points a surveiller** :
   - Cables UART courts, GND commun
   - SPI LoRa hors des pins flash du C3
   - CRC16 obligatoire pour eviter corruption
   - Verifier que le debit de trames GNSS + LoRa ne surcharge pas le ring buffer S3
   - Le C3 n'a pas de PSRAM : toute la memoire est en SRAM interne (~400 KB)
