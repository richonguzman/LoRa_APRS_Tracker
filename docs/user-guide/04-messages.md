# 4. Messagerie

Accès depuis le Dashboard → bouton **MSG**.

![Messagerie](../tdeck_messaging.jpg)

---

## Onglets

La messagerie comporte 5 onglets :

| Onglet | Contenu |
|--------|---------|
| **APRS** | Conversations APRS par indicatif (threads) |
| **Winlink** | Messages Winlink reçus |
| **Contacts** | Carnet d'adresses |
| **Frames** | Trames brutes reçues |
| **Stats** | Statistiques par station |

> **Bouton corbeille rouge** (en haut à droite) : supprime tous les messages de l'onglet actif (APRS ou Winlink) après confirmation.

---

## Conversations (onglet APRS)

Les messages APRS sont organisés par conversation (un fil par indicatif).

- **Appui simple** — Ouvrir la conversation

### Dans une conversation
- Messages reçus : bulles grises, à gauche
- Messages envoyés : bulles bleues, à droite
- **Appui long sur un message** : le supprimer (confirmation demandée)

Bouton **Reply** (icône crayon) en haut à droite pour répondre.

> La suppression d'une conversation entière n'est pas disponible depuis la liste. Pour effacer une conversation, supprimez tous ses messages individuellement, ou utilisez le bouton corbeille rouge depuis l'onglet APRS.

Les messages APRS sont sauvegardés sur la carte SD et persistent entre les redémarrages.

---

## Messages Winlink (onglet Winlink)

Liste des messages Winlink reçus.

- **Appui simple** — Afficher le contenu du message
- **Appui long** — Supprimer le message (confirmation)

> Le bouton corbeille rouge supprime tous les messages Winlink.

---

## Composer un message

1. **MSG → icône crayon** ou **Reply** dans une conversation, ou **icône d'une station sur la carte**
2. Remplir le champ **To:** (indicatif destinataire)
3. Saisir le message dans la zone de texte
4. **Send** pour envoyer

Le clavier QWERTY physique du T-Deck fonctionne directement. Sur l'écran tactile, un clavier virtuel est disponible.

### Touches spéciales du clavier
- **Shift** : majuscules momentanées
- **Double-tap Shift** : verrouillage majuscules
- **Symbol** : caractères spéciaux

---

## Contacts

Gérer un carnet d'adresses pour composer rapidement.

- **Appui simple** — Composer un message vers ce contact
- **Appui long** — Éditer ou supprimer le contact

### Ajouter un contact
1. Bouton **+** en haut à droite
2. Renseigner : Indicatif (obligatoire), Nom, Commentaire
3. **Save**

Les contacts sont sauvegardés sur la carte SD et persistent entre les redémarrages.

---

## Frames (trames brutes)

Affiche les 20 dernières trames APRS reçues.

- **Format abrégé** : date/heure et début de la trame (coupé si trop long)
- **Cliquez sur une trame** pour afficher son contenu complet dans un popup

Les trames sont sauvegardées sur la carte SD et persistent entre les redémarrages.
Utile pour le diagnostic et l'analyse des trames brutes.


---

## Stats

Tableau des stations reçues avec :
- Indicatif
- RSSI moyen (dBm)
- SNR moyen (dB)
- Nombre de paquets reçus

Les 20 stations les plus récentes sont sauvegardées sur la carte SD et persistent entre les redémarrages.

> Les stations sont classées par ordre d'arrivée ; la plus ancienne est évincée quand la liste est pleine.
