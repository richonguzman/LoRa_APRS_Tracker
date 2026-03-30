# 7. Dépannage

---

## L'écran reste noir au démarrage

- Maintenir le bouton latéral enfoncé 2 secondes
- Si aucune réaction : recharger la batterie (brancher USB)
- Si le problème persiste : reflasher le firmware via le Web Flasher

---

## Le GPS ne trouve pas de fix

- Se placer à l'extérieur, à découvert
- Attendre 1–2 minutes (cold start)
- Vérifier l'indicateur HDOP sur le dashboard (doit afficher `+` ou `-`)
- Un `X` signifie un signal trop faible

---

## Pas de paquets LoRa reçus

1. Vérifier la fréquence dans **Settings → LoRa Frequency** (433.775 MHz pour l'Europe)
2. S'assurer que l'antenne est bien vissée
3. Vérifier qu'il y a des stations actives dans votre zone sur [aprs.fi](https://aprs.fi)

---

## La carte est grise / sans tuiles

- La carte SD n'est pas insérée ou non reconnue
- Les tuiles ne sont pas au bon emplacement (voir [SD Card](06-sd-card.md))
- Vérifier que la carte SD est formatée en FAT32
- Rebooter après avoir inséré la SD

---

## Le Bluetooth ne démarre pas

- S'assurer que le WiFi est désactivé (WiFi et BLE sont mutuellement exclusifs)
- Rebooter et réessayer
- Si l'erreur persiste, c'est une contrainte mémoire — voir note ci-dessous

> Sur ESP32-S3, WiFi et BLE partagent la même DRAM interne. Il n'est pas possible de les activer simultanément.

---



---

## Les messages ne sont pas sauvegardés

- Vérifier que la carte SD est présente et reconnue
- Sans SD card, les messages sont perdus au reboot

---

## L'appareil redémarre tout seul

- Pile déchargée : recharger via USB
- Mémoire insuffisante lors d'une opération lourde : rebooter et éviter WiFi + Carte simultanément
- Consulter les logs série (115200 baud) pour identifier la cause

---

## Réinitialisation complète

Pour effacer toute la configuration :

1. Reflasher avec l'option **Erase device** cochée dans le Web Flasher
2. OU supprimer `config.json` de la carte SD

---

## Obtenir de l'aide

- Issues GitHub : [moricef/LoRa_APRS_Tracker](https://github.com/moricef/LoRa_APRS_Tracker/issues)
- Forum APRS francophone : [f5len.org](https://www.f5len.org)
