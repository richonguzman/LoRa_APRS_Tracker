# T-Beam 1W – BLE-PIN und OLED-Aufwecken

Basis: LoRa_APRS_Tracker **V4.0.1**, eigene Variante **4.0.1-sec-oled.3**.
Nur für **LilyGO T-Beam 1W / ESP32-S3**.

## Änderungen

- Beim ersten BLE-Koppeln erscheint ein zufälliger sechsstelliger PIN auf dem
  OLED. Diesen auf dem Handy eingeben. Erfolgreiche Kopplungen werden gespeichert.
- Funkdaten über BLE erfordern eine verschlüsselte und authentifizierte Verbindung.
  Eine unbestätigte Verbindung wird nach spätestens 60 Sekunden getrennt.
- Es kann jeweils ein BLE-Gerät verbunden sein.
- In der Web-Konfiguration löscht „Forget paired BLE devices when saving“
  gespeicherte Kopplungen beim nächsten BLE-Start. Danach auch auf dem Handy
  die alte Kopplung löschen.
- Senden, gültiger APRS-Empfang und Tastendruck wecken das Display auf.
  **Eco-Modus eingeschaltet lassen** und den gewünschten Timeout einstellen.
  Der Timer beginnt nach dem Senden neu; das OLED bleibt nicht dauerhaft an.

Die Firmware verwendet BLE Legacy Passkey Pairing mit Bonding, kein LE Secure
Connections. Damit werden beliebige ungekoppelte Zugriffe verhindert; der
Kryptoschutz beim erstmaligen Koppeln ist schwächer als bei Secure Connections.
Die Umsetzung baut auf der Arbeit von Antonio Megia / EA4HXZ auf:
https://github.com/armegia/LoRa_APRS_Tracker/tree/pr/diagnostics-ble-bonding

## Firmware installieren

Die Datei `firmware.bin` ist ein **Anwendungsimage**, kein vollständiges Flashimage.
Für ein bereits mit V4.0.1 laufendes T-Beam 1W:

1. Konfiguration sichern und USB anschließen. Serielle Anwendungen schließen.
2. Archiv entpacken und ein Terminal im Ordner mit `firmware.bin` öffnen.
3. Mit Python und esptool installieren (COM5 durch den tatsächlichen Port ersetzen):

```sh
python -m pip install esptool
python -m esptool --chip esp32s3 --port COM5 write_flash 0x10000 firmware.bin
```

**Adresse 0x10000 verwenden, nicht 0x0. Kein „Erase Flash“ ausführen.**
Bootloader, Partitionstabelle und Dateisystem werden bei diesem Update nicht
geschrieben. Die vorhandene Konfiguration bleibt erhalten.
Unter Linux heißt der Port beispielsweise `/dev/ttyACM0`, unter macOS `/dev/cu.usbmodem…`.

## Erster Test mit Graywolf

1. Gegebenenfalls die bisherige Tracker-Kopplung auf dem Handy löschen.
2. Tracker wie bisher innerhalb von Graywolf per BLE verbinden.
3. Den PIN vom Tracker auf dem Handy bestätigen; gegebenenfalls die
   Android-Kopplungsbenachrichtigung öffnen.
4. Position senden und deren Empfang an der Gegenstation prüfen.
5. Empfang in Gegenrichtung testen, danach Verbindung und Tracker neu starten:
   Die gespeicherte Kopplung sollte ohne erneute PIN-Eingabe funktionieren.
6. OLED einschlafen lassen. TX und RX müssen es aufwecken, anschließend muss
   es nach dem eingestellten Timeout wieder ausgehen.

## Quellcode und Prüfungen

Das Archiv enthält den vollständigen Quellcode und einen Patch gegen V4.0.1.
Lizenz: GPL-3.0-or-later; siehe LICENSE im Quellcode.

```sh
python -m pip install platformio
python -m platformio run -e ttgo-t-beam-1W
python tests/host/run.py
```

Die Host-Tests prüfen BLE-Zugriffssperren, PIN-Anzeige, Datenverarbeitung bei
Verbindungsabbruch und Wiederverbindung, Pufferüberlauf sowie OLED-Aufwecken
und Timeout anhand des Produktionscodes mit simulierten Geräten.
Ein echter Hardwaretest mit T-Beam 1W und Graywolf steht noch aus.

## Revision .2

Während der PIN-Kopplung hat BLE PAIR Vorrang vor Hauptbildschirm und Meldungen.
Nach Kopplung, Abbruch oder Timeout wird die normale Anzeige wieder freigegeben.

## Revision .3

Bei verbundenem Bluetooth-KISS-Client erzeugt die App die Nachrichten-ACKs.
Der Tracker zeigt/speichert Nachrichten weiter und reicht das Originalpaket
weiter, erzeugt dafür aber kein zusätzliches ACK. Ohne verbundenen KISS-Client
bestätigt er wieder selbst. Das gilt für BLE (erst nach authentifizierter
Kopplung) und Bluetooth Classic. Rufzeichen/SSID in Tracker und App gleich
einstellen und ACKs in der App aktivieren. Eine verbundene App ohne ACK-Funktion
hat keinen automatischen Tracker-Fallback. TNC2 und andere automatische
Antworten bleiben unverändert.
