# 1. Installation & premier démarrage

## Flasher le firmware

La méthode la plus simple est le **Web Flasher** — aucun outil à installer.

1. Connecter le T-Deck Plus en USB
2. Ouvrir le flasher : [https://moricef.github.io/LoRa_APRS_Tracker/](https://moricef.github.io/LoRa_APRS_Tracker/)
3. Cliquer **Install** et sélectionner le port USB
4. Si c'est une nouvelle installation, cocher **Erase device** pour effacer la configuration existante

> Le Web Flasher nécessite Chrome ou Edge (pas Firefox).

---

## Premier démarrage — Configuration Web Automatique

Au premier démarrage (callsign par défaut `NOCALL-7`) :

1. **Écran de démarrage (splash screen)** avec la version du firmware
2. **Écran d'initialisation** montrant la progression :
   - Stockage (SPIFFS, détection carte SD)
   - Module GPS
   - Radio LoRa
   - WiFi/BLE (désactivés par défaut)
3. **Écran de première configuration** s'affiche automatiquement car le callsign est `NOCALL-7` :
   - Point d'accès WiFi `LoRa-Tracker-AP` démarre (mot de passe : `1234567890`)
   - Adresse IP : `192.168.4.1`
   - Connecter un PC/téléphone à ce réseau WiFi
   - Ouvrir `http://192.168.4.1` dans un navigateur
4. **Interface web de configuration** :
   - Définir votre indicatif APRS (obligatoire)
   - Configurer la fréquence et vitesse LoRa
   - Optionnellement configurer un réseau WiFi pour la connexion APRS-IS et le mode web-conf
   - Configurer SmartBeacon, filtres, commentaire de balise, etc.
5. Cliquer **Save**, puis **Reboot** sur le T-Deck
6. Après reboot, l'écran **dashboard** apparaît avec votre indicatif configuré
7. Le GPS commence à chercher des satellites (peut prendre 1-2 minutes à l'extérieur)

> La configuration web automatique ne s'exécute que si le callsign est `NOCALL-7`. Une fois configuré, les démarrages suivants passent directement au dashboard.

---

## Configuration minimale

Avant de l'utiliser, configurer au minimum :

| Paramètre | Où | Valeur |
|-----------|-----|--------|
| Callsign | Web config → Callsign | Votre indicatif APRS (ex: `F4ABC-9`) |
| Fréquence LoRa | Web config → LoRa Frequency | 433.775 MHz (Europe) |
| Vitesse LoRa | Web config → LoRa Speed | 300 bps (par défaut) |

Le WiFi et Bluetooth sont **désactivés par défaut** pour économiser la mémoire. Les activer uniquement quand nécessaire via Paramètres.

---

## Mode Web-Conf Manuel (après premier boot)

Pour des changements de configuration avancés ultérieurement :

1. Aller dans **Settings → Web-Conf Mode** et l'activer
2. Le tracker reboote et démarre le point d'accès WiFi `LoRa-Tracker-AP`
3. Se connecter à `http://192.168.4.1`
4. Configurer les paramètres, puis cliquer **Save**
5. Sur le T-Deck, appuyer sur **Back** pour quitter le mode Web-Conf et rebooter

> Le mode Web-Conf exécute un serveur web léger et affiche un écran de configuration minimal avec les informations de connexion WiFi. L'interface LVGL normale est suspendue pendant ce mode.
