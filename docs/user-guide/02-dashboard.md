# 2. Dashboard

L'écran principal s'affiche au démarrage et montre toutes les informations en temps réel.

![Dashboard](../../tdeck_dashboard.jpg)

---

## Barre de statut (haut)

| Élément | Description |
|---------|-------------|
| **Callsign** | Votre indicatif APRS |
| **Symbole APRS** | Icône représentant votre station |
| **Date/Heure** | Format JJ/MM HH:MM (heure GPS) |
| **GPS 3D** | Icône jaune quand le mode fix 3D strict est activé dans les paramètres (les balises nécessitent alors PDOP ≤5.0 au lieu de HDOP) |
| **WiFi** | Icône verte quand connecté |
| **Bluetooth** | Icône verte quand un client BLE est connecté |
| **Batterie** | Pourcentage + couleur (vert >50%, orange 20–50%, rouge <20%) |

---

## Zone centrale

### GPS
- **Satellites** — Nombre de satellites + indicateur de qualité HDOP :
  - `+` HDOP ≤ 2.0 (excellent)
  - `-` HDOP 2–5 (bon)
  - `X` HDOP > 5 (médiocre)
- **Locator** — Carré Maidenhead 8 caractères (ex: `JN03AA12`)
- **Position** — Latitude / Longitude en degrés décimaux
- **Altitude** — en mètres
- **Vitesse** — en km/h

### LoRa
- **Fréquence** — En MHz (ex: `433.775`)
- **Vitesse** — En bps

### Dernières stations reçues
Les 4 stations les plus récemment reçues avec leur RSSI (dBm) et SNR (dB).

---

## Boutons d'action (bas)

| Bouton | Couleur | Action |
|--------|---------|--------|
| **BCN** | Rouge | Envoyer une balise APRS immédiatement (si qualité GPS suffisante : ≥6 satellites et HDOP ≤5.0) |
| **MSG** | Bleu | Ouvrir la messagerie |
| **MAP** | Vert | Ouvrir la carte |
| **SET** | Violet | Ouvrir les paramètres |

---

## Balise automatique

Le tracker envoie des balises automatiquement en utilisant la logique **SmartBeacon** :
- Plus fréquentes quand vous vous déplacez vite
- Moins fréquentes à l'arrêt
- **Conditions GPS** : nécessite au moins 6 satellites et HDOP ≤5.0 (ou PDOP ≤5.0 en mode 3D strict)

Le bouton **BCN** permet d'envoyer une balise manuelle à tout moment, sous les mêmes conditions de qualité GPS.