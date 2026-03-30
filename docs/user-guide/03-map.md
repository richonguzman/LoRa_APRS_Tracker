# 3. Carte

Accès depuis le Dashboard → bouton **MAP**.

![Carte vectorielle](../tdeck_vector_map.jpg)

---

## Interface

### Barre de titre (verte, en haut)
- **< BACK** — Retour au dashboard
- **MAP (Zx)** — Titre avec le niveau de zoom actuel
- **Bouton GPS** — Recentre la carte sur votre position
  - Bleu foncé : suivi GPS actif
  - Orange : suivi GPS désactivé (pan manuel)
- **+** / **−** — Zoom avant / zoom arrière
- **GPX** — Lance/arrête l'enregistrement de trace GPX

### Barre d'info (bleu foncé, en bas)
Affiche :
- Les coordonnées du centre de la carte (latitude, longitude)
- Le nombre de stations visibles (Stn)
- Le delta GPS (d) : distance entre la position brute du GPS et la position filtrée, en mètres
- Le coefficient alpha (a) : 1.00 = pas de filtrage, < 1.00 = filtrage actif (réduit le bruit)

---

## Navigation

| Geste | Action |
|-------|--------|
| **Glisser** | Déplacer la carte (pan) |
| **+** / **−** | Changer le niveau de zoom |
| **Double-tap** | Basculer en plein écran (cache les barres) |

> Le zoom par pinch (deux doigts) n'est pas disponible.

Après un pan, la carte continue légèrement sur sa lancée (inertie). Pour réactiver le suivi GPS automatique, appuyer sur le bouton GPS.

---

## Modes d'affichage

### Raster (Z6–Z8)
Tuiles JPEG/PNG style OpenStreetMap. Nécessite des tuiles sur la carte SD.

### Vectoriel / NAV (Z9–Z17)
Rendu vectoriel avec routes, chemins, bâtiments, cours d'eau. Plus détaillé, basé sur les fichiers `.nav` sur la carte SD.

La transition raster→vectoriel est automatique à partir de Z9.

---

## Affichage des stations

- Symboles APRS des stations reçues avec leur indicatif
- Les labels se décalent automatiquement pour éviter les chevauchements
- Votre propre position est affichée avec votre symbole APRS
- **Trace GPS** : les 500 derniers points de votre trajet (effacés après 60 min)

### Envoyer un message à une station

Appuyez sur l'icône d'une station pour ouvrir l'écran de composition de message avec son indicatif pré-rempli dans le champ **À :**.

---

## Enregistrement GPX

1. Appuyer sur le bouton **GPX** pour démarrer l'enregistrement
2. Le bouton devient rouge pendant l'enregistrement
3. Appuyer à nouveau pour arrêter
4. Le fichier est sauvegardé sur la carte SD dans `/LoRa_Tracker/GPX/`


