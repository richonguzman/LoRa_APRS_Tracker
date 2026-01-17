#!/bin/bash
# Script pour remplir contacts.json facilement

# Chercher la carte SD montée
SD_PATH=$(find /media/$USER -maxdepth 1 -type d 2>/dev/null | grep -v "^/media/$USER$" | head -1)

if [ -n "$SD_PATH" ]; then
    DEFAULT_PATH="$SD_PATH/LoRa_Tracker/Contacts/contacts.json"
else
    DEFAULT_PATH="./contacts.json"
fi

echo "=== Ajout de contacts ==="
echo ""
read -p "Fichier de sortie [$DEFAULT_PATH]: " OUTPUT_FILE
OUTPUT_FILE="${OUTPUT_FILE:-$DEFAULT_PATH}"

# Créer le répertoire si nécessaire
mkdir -p "$(dirname "$OUTPUT_FILE")" 2>/dev/null

# Charger contacts existants si le fichier existe
if [ -f "$OUTPUT_FILE" ]; then
    CONTACTS=$(cat "$OUTPUT_FILE")
    COUNT=$(echo "$CONTACTS" | jq 'length')
    echo "-> $COUNT contact(s) existant(s) chargé(s)"
else
    CONTACTS="[]"
fi

echo "(Appuyez sur Entrée sans rien taper pour terminer)"
echo ""

while true; do
    echo "--- Nouveau contact ---"

    read -p "Callsign: " CALLSIGN

    # Si callsign vide, on arrête
    if [ -z "$CALLSIGN" ]; then
        break
    fi

    # Convertir en majuscules
    CALLSIGN=$(echo "$CALLSIGN" | tr '[:lower:]' '[:upper:]')

    read -p "Nom: " NAME
    read -p "Commentaire: " COMMENT

    # Ajouter au JSON avec jq
    CONTACTS=$(echo "$CONTACTS" | jq --arg c "$CALLSIGN" --arg n "$NAME" --arg m "$COMMENT" \
        '. += [{"callsign": $c, "name": $n, "comment": $m}]')

    echo "-> $CALLSIGN ajouté"
    echo ""
done

# Sauvegarder
echo "$CONTACTS" | jq '.' > "$OUTPUT_FILE"

COUNT=$(echo "$CONTACTS" | jq 'length')
echo ""
echo "=== $COUNT contact(s) sauvegardé(s) dans $OUTPUT_FILE ==="
echo ""
echo "Copiez ce fichier sur la carte SD dans:"
echo "  /LoRa_Tracker/Contacts/contacts.json"
