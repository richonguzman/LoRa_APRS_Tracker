# CrowPanel Advance 3.5" — Debug LoRa SX1262 (HT-RA62)

## Date : 2026-04-11

## Hardware

- **Board** : CrowPanel Advance 3.5" (ESP32-S3)
- **Module LoRa** : Heltec HT-RA62 (SX1262, cristal 32 MHz XTAL, **PAS de TCXO**)
- **Connecteur LoRa CrowPanel** : SPI + DIO1 + DIO2 + DIO3 + BUSY + RST. **Pas de TXEN/RXEN connectés.**

## Pinout LoRa (board_pinout.h)

| Signal | GPIO | Note |
|--------|------|------|
| SCLK | 10 | |
| MISO | 9 | |
| MOSI | 3 | |
| CS (NSS) | 0 | `0U` pour éviter ambiguïté null pointer |
| RST | 2 | Partagé avec TFT_RST (mais LGFX pin_rst=-1, pas de conflit) |
| DIO1 | 1 | IRQ pin (TX_DONE, RX_DONE) |
| DIO2 | NC | Non connecté côté ESP32 (utilisé en interne SX1262 pour RF switch) |
| DIO3 | NC | Non connecté côté ESP32 |
| BUSY | 46 | |
| TXEN | - | Existe sur HT-RA62 (pin 5) mais **pas connecté** sur CrowPanel |
| RXEN | - | Existe sur HT-RA62 (pin 11) mais **pas connecté** sur CrowPanel |

## Pinout HT-RA62 (datasheet Rev1.1)

| Pin | Nom | Type | Fonction |
|-----|------|------|----------|
| 1 | ANT | O | Sortie antenne LoRa |
| 2 | GND | P | Masse |
| 3 | 3V3 | P | Alimentation 3.3V (2.7-3.5V, typ 3.3V, >=150mA) |
| 4 | RST | I | Reset LoRa |
| 5 | TXEN | I | RF Control pin (TX enable) |
| 6 | DIO1 | I/O | DIO1 Configuration |
| 7 | DIO2 | I/O | DIO2 Configuration |
| 8 | DIO3 | I/O | DIO3 Configuration |
| 9 | GND | P | Masse |
| 10 | BUSY | I/O | LoRa BUSY |
| 11 | RXEN | I/O | RF Control pin (RX enable) |
| 12 | SCK | I/O | SPI Clock |
| 13 | MISO | I/O | SPI MISO |
| 14 | MOSI | I/O | SPI MOSI |
| 15 | NSS | I/O | SPI Chip Select |
| 16 | GND | P | Masse |

## SPI Bus

- **LoRa** : HSPI (SPI3_HOST) — `SPIClass loraSPI(HSPI)`
- **Display** : FSPI (SPI2_HOST) via LovyanGFX — pas de conflit
- **SD Card** : HSPI partagé avec LoRa, protégé par `spiMutex`
- SD pins : SCK=5, MISO=4, MOSI=6, CS=7

## Module RadioLib

```cpp
// Constructeur : RST=RADIOLIB_NC car IO2 partagé avec TFT_RST
SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIOLIB_NC, RADIO_BUSY_PIN, loraSPI);
```

- RST passé comme RADIOLIB_NC : RadioLib ne fait pas de hardware reset
- Le reset IO2 par RadioLib cause -707 (begin() échoue car IO2 reset aussi le display)
- Reset manuel IO2 fait dans LoRa_APRS_Tracker.cpp après init LoRa

## Chronologie du debug

### Problème initial
Le LoRa ne transmet pas sur CrowPanel. `radio.begin()` réussit mais `radio.transmit()` échoue.

### Hypothèses testées et éliminées

1. **Conflit SPI HSPI entre SD et LoRa** — Éliminé. loraSpiBegin/End mis en no-op → même erreur.
2. **GPIO matrix remapping** — Éliminé. pinMatrixOutAttach/InAttach testé → même erreur ou pire.
3. **CPU frequency change** — Éliminé. `lowerCpuFrequency()` désactivé → même erreur.
4. **RST pin partagé IO2** — Éliminé partiellement. Passer RST à RadioLib cause -707 au begin(). LGFX a pin_rst=-1 donc pas de conflit en théorie, mais le hardware reset IO2 LOW perturbe quand même.

### Découvertes clés

#### 1. TCXO vs XTAL (CAUSE RACINE PROBABLE)
- **Le firmware factory Elecrow** utilise `USE_DIO3_TCXO = true` avec `TCXO_CTRL_3_3V` (3.3V)
- **Le firmware factory est écrit pour** un "eByte E22" / "CircuitRocks Alora RFM1262" qui ont un **TCXO**
- **Le HT-RA62 a un cristal 32 MHz (XTAL), PAS un TCXO**
- Configurer DIO3 comme TCXO quand le module a un XTAL → le SX1262 attend un oscillateur qui ne démarre jamais → bloqué en TX/RX

#### 2. Tension TCXO insuffisante (résolu partiellement)
- Code initial : TCXO 1.8V → standby OK mais startReceive() → BUSY=1 immédiat, SPI mort
- Passage à TCXO 3.3V → startReceive() fonctionne, standby post-RX OK
- Mais TX échoue toujours (-5 TX_TIMEOUT ou -707 SPI_CMD_TIMEOUT)

#### 3. Comportement observé avec TCXO 3.3V
- `radio.begin()` : OK
- 6x `radio.standby()` : tous OK (standby=0, BUSY=0)
- `radio.startReceive()` → `radio.standby()` : OK (standby=0)
- `radio.transmit()` : **ÉCHOUE** (-5 ou -707)
- `radio.getIrqFlags()` après TX : **0x0000** — aucun IRQ généré, le SX1262 n'a jamais transmis
- DIO1 : jamais HIGH pendant TX

### Résultats des tests (session 2026-04-11)

#### 1. XTAL vs TCXO
- `radio.XTAL = true` → begin() échoue -707 (BUSY timeout pendant calibration)
- `radio.begin(..., 3.3)` (TCXO 3.3V) → begin() OK, standby OK
- **Conclusion** : le module NÉCESSITE la config TCXO 3.3V via DIO3 pour que begin() fonctionne

#### 2. Diagnostic XOSC_START_ERR
`getDeviceErrors()` après échec TX (via SPI brut, cmd 0x17) :

| Test | Mode départ | Résultat | Errors |
|------|-------------|----------|--------|
| standby(RC) | - | OK (=0) | 0x0000 |
| standby(XOSC) | STBY_RC | OK, chipMode=3 (STBY_XOSC) | 0x0000 |
| SetFs (raw SPI) | STBY_XOSC | OK, mode=4 (FS), PLL lock | 0x0000 |
| TX RadioLib +22dBm | STBY_RC | FAIL -5, timeout 5s | 0x0020 (XOSC=1) |
| TX RadioLib -9dBm | STBY_RC | FAIL, IRQ=0x0000 | 0x0020 (XOSC=1) |
| TX raw SPI | FS (XOSC actif, PLL locké) | FAIL, timeout 5s | 0x0020 (XOSC=1) |

**L'XOSC démarre correctement** en STBY_XOSC et FS. **Échoue systématiquement dès que SetTx est envoyé**, même depuis FS où l'XOSC est déjà actif, même à -9dBm.

#### 3. TCXO timeout
- RadioLib default : 5000 (78ms) → TX échoue
- Reconfiguration raw SPI : 64000 (~1s) + recalibration → calibration OK (errors=0), TX échoue toujours
- Le timeout n'est pas la cause

#### 4. RadioLib vs raw SPI
- `radio.getIrqFlags()` retourne 0xFFFF (garbage) après les appels SPI bruts — SPI désynchronisé entre RadioLib et loraSPI brut
- Les lectures raw SPI (GetIrqStatus 0x12, GetStatus 0xC0, GetDeviceErrors 0x17) retournent des valeurs cohérentes
- `startTransmit()` retourne 0 mais prend ~1s (= TCXO timeout dans la boucle BUSY infinie de startTransmit)

#### 5. Status bytes observés
- `0xA2` = STBY_RC + "data available" (post-calibration)
- `0xB2` = STBY_XOSC + "data available" (après standby(XOSC))
- `0xC2` = FS + "data available" (après SetFs)
- `0xAA` = STBY_RC + "failure to execute" (après échec TX)

### Conclusion

**XOSC_START_ERR est déclenché spécifiquement par la transition vers TX**, indépendamment de :
- La puissance (-9dBm ou +22dBm)
- Le mode de départ (STBY_RC, STBY_XOSC, FS)
- Le TCXO timeout (78ms ou 1s)
- La librairie (RadioLib ou raw SPI)

**Hypothèse : problème hardware** — le module HT-RA62 ou son intégration sur le CrowPanel empêche le TX. Possibilités :
1. Module HT-RA62 défectueux (PA ou circuit XOSC/TCXO)
2. TXEN/RXEN (pins 5/11 du HT-RA62) nécessaires mais non connectés sur CrowPanel
3. Problème de découplage alimentation PA sur le PCB CrowPanel
4. Antenne non connectée → VSWR infini (mais -9dBm devrait pas poser problème)

### Prochaine étape critique

**Flasher le firmware factory Elecrow (HMI3-5.ino)** et vérifier si le TX fonctionne :
- Si TX OK → on rate une config, comparer avec SX126x-Arduino init sequence
- Si TX KO → hardware défectueux, retour board

## Code diagnostique en place (à retirer après résolution)

1. **DIAG[0-5]** : 6x standby() dans setup() après radio.begin()
2. **DIAG-TCXO** : reconfiguration TCXO 3.3V timeout 64000 + recalibration
3. **DIAG-XOSC** : standby(XOSC) + lecture status chip + erreurs
4. **DIAG-FS** : SetFs raw SPI + lecture status/erreurs
5. **DIAG-TX-RAW** : TX complet via raw SPI depuis FS mode
6. **DIAG-TX** : standby() + probe avant transmit() dans sendNewPacket()
7. **lowerCpuFrequency()** désactivé dans LoRa_APRS_Tracker.cpp
8. **loraSpiBegin/End** : actuellement no-op
9. **RADIOLIB_GODMODE** : retiré (méthodes protégées contournées par raw SPI)

## Fichiers modifiés

- `src/lora_utils.cpp` — TCXO config, sondes diagnostiques raw SPI, loraSpiBegin/End no-op
- `src/LoRa_APRS_Tracker.cpp` — lowerCpuFrequency() commenté, IO2 forcé HIGH
- `include/LGFX_CrowPanel_35.h` — pin_rst=-1
- `src/power_utils.cpp` — pins safety (ledTx=-1, ledMsg=-1, ptt=-1)
- `variants/crowpanel_advance_35/board_pinout.h` — Pinout LoRa

## Référence factory firmware

- Fichier : `CrowPanel-Advance-3.5-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-480x320/factory_sourcecode/HMI3-5/HMI3-5.ino`
- Lib LoRa : SX126x-Arduino (pas RadioLib)
- SPI LoRa : FSPI (SPIClass par défaut), pas HSPI
- Config : `USE_DIO2_ANT_SWITCH=true`, `USE_DIO3_TCXO=true`, `TCXO_CTRL_3_3V`, `TXEN=-1`, `RXEN=-1`
- **Attention** : ce firmware est pour un module eByte E22 / RFM1262 (TCXO), PAS pour un HT-RA62 (XTAL)
