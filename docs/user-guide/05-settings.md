# 5. Paramètres

Accès depuis le Dashboard → bouton **SET**.

---

## Menu principal

| Option | Description |
|--------|-------------|
| **Callsign** | Votre indicatif APRS |
| **LoRa Frequency** | Fréquence d'émission/réception |
| **LoRa Speed** | Débit de données LoRa |
| **Display** | Luminosité, économie d'énergie |
| **Sound** | Volume et types de bips |
| **Repeater** | Mode répéteur APRS |
| **GPS** | Paramètres GPS (fix 3D strict) |
| **WiFi** | Connexion réseau |
| **Bluetooth** | Configuration BLE |
| **Web-Conf Mode** | Configuration avancée via navigateur |
| **Reboot** | Redémarrer l'appareil |
| **About** | Version du firmware |

---

## Callsign

Saisir votre indicatif APRS complet, avec le SSID si souhaité.

Exemples : `F4ABC`, `F4ABC-9`, `F4ABC-7`

Les SSID courants :
- `-9` : voiture
- `-7` : vélo
- `-5` : piéton
- `-1` à `-4` : station fixe

---

## LoRa Frequency

Sélectionner la fréquence selon votre région :

| Région | Fréquence |
|--------|-----------|
| EU/WORLD | 433.775 MHz |
| POLAND | 434.855 MHz |
| UK | 439.913 MHz |

La fréquence active est affichée en vert.

---

## LoRa Speed

Sélectionner le débit LoRa. Un débit plus bas améliore la portée mais allonge la durée d'émission.

Débits disponibles :
- **1200 bps** (SF9)
- **610 bps** (SF10)
- **300 bps** (SF12)
- **244 bps** (SF12)
- **209 bps** (SF12)
- **183 bps** (SF12)

---

## Display

| Paramètre | Description |
|-----------|-------------|
| **ECO Mode** | Éteint l'écran après inactivité |
| **ECO Timeout** | Délai avant extinction (2–15 min) |
| **Brightness** | Luminosité de l'écran (5–100%) |

> ECO Timeout et Brightness sont ajustables uniquement en mode débloqué (toggle actif).

---

## GPS

Paramètres du récepteur GPS.

| Paramètre | Description |
|-----------|-------------|
| **Strict 3D Fix (Mountain)** | Active la vérification PDOP (≤5.0) au lieu de HDOP pour les balises APRS. Nécessite au moins 6 satellites. |

### Critères de transmission

| Mode | Critère | Description |
|------|---------|-------------|
| **Normal (OFF)** | HDOP ≤ 5.0 | Précision horizontale seulement |
| **Strict 3D (ON)** | PDOP ≤ 5.0 | Précision 3D combinée (horizontal + vertical) |

> L'icône GPS 3D jaune sur le dashboard indique que ce mode est activé.

> **Note** : Si le critère n'est pas satisfait, le beacon est silencieusement sauté (SmartBeacon continue). Le mode PDOP garantit des altitudes précises en montagne, mais peut causer des délais en forêt/ville où le ciel est partiellement masqué.

---

## Sound

| Paramètre | Description |
|-----------|-------------|
| **Sound** | Active/désactive tous les sons |
| **Volume** | Volume général (0–100%) |
| **TX Beep** | Bip à chaque transmission |
| **Message Beep** | Bip à la réception d'un message |
| **Station Beep** | Bip à la détection d'une nouvelle station |

---

## Repeater

Active le mode répéteur : le tracker retransmet tous les paquets APRS reçus.

> Consomme plus d'énergie. À utiliser uniquement si vous opérez un digi-répéteur.

---

## WiFi

| État | Description |
|------|-------------|
| **OFF (disabled)** | WiFi désactivé |
| **Connecting...** | Tentative de connexion en cours |
| **Connected** | Connecté, IP affichée |
| **Eco (retry)** | Mode éco, reconnexion périodique |

Une fois connecté, l'IP locale et le RSSI WiFi s'affichent.

> **WiFi et BLE ne peuvent pas fonctionner simultanément.** Activer l'un désactive l'autre.

---

## Bluetooth (BLE)

Permet la connexion d'une application mobile (ex: APRSDroid via BLE).

| État | Description |
|------|-------------|
| **OFF** | BLE désactivé |
| **ON (waiting)** | En attente de connexion |
| **Connected** | Client connecté |

> **WiFi et BLE ne peuvent pas fonctionner simultanément.** Activer l'un désactive l'autre.

---

## Web-Conf Mode

Lance un point d'accès WiFi pour la configuration avancée :

1. Activer **Web-Conf Mode**
2. Se connecter au WiFi `LoRa-Tracker-AP` (mot de passe : `1234567890`)
3. Ouvrir `http://192.168.4.1`
4. Configurer SmartBeacon, filtres APRS, commentaire de beacon, etc.
5. Sauvegarder puis **Reboot** sur le T-Deck
