# 4. Messaging

Access from Dashboard → **MSG** button.

![Messaging](../../tdeck_messaging.jpg)

---

## Tabs

The messaging screen has 5 tabs:

| Tab | Content |
|-----|---------|
| **APRS** | APRS conversations by callsign (threads) |
| **Winlink** | Received Winlink messages |
| **Contacts** | Address book |
| **Frames** | Raw received frames |
| **Stats** | Per-station statistics |

> **Red trash button** (top right): deletes all messages in the active tab (APRS or Winlink) after confirmation.

---

## Conversations (APRS tab)

APRS messages are organized by conversation (one thread per callsign).

- **Short press** — Open the conversation

### Inside a conversation
- Received messages: grey bubbles, on the left
- Sent messages: blue bubbles, on the right
- **Long press on a message** — delete it (confirmation required)

**Reply** button (pencil icon) in the top right to reply.

> Deleting an entire conversation is not available from the list. To clear a conversation, delete all its messages individually, or use the red trash button from the APRS tab.

APRS messages are saved on the SD card and persist across reboots.

---

## Winlink Messages (Winlink tab)

List of received Winlink messages.

- **Short press** — Display message content
- **Long press** — Delete the message (confirmation)

> The red trash button deletes all Winlink messages.

---

## Composing a Message

1. **MSG → pencil icon** or **Reply** inside a conversation, or a **station icon on the map**
2. Fill in the **To:** field (recipient callsign)
3. Type the message in the text area
4. **Send** to transmit

The T-Deck's physical QWERTY keyboard works directly. On the touchscreen, an on-screen keyboard is available.

### Special Keyboard Keys
- **Shift**: momentary uppercase
- **Double-tap Shift**: caps lock
- **Symbol**: special characters

---

## Contacts

Manage an address book for quick message composition.

- **Short press** — Compose a message to this contact
- **Long press** — Edit or delete the contact

### Adding a Contact
1. **+** button in the top right
2. Fill in: Callsign (required), Name, Comment
3. **Save**

Contacts are saved on the SD card and persist across reboots.

---

## Frames (Raw Frames)

Displays the last 20 received APRS frames.

- **Abbreviated format**: date/time and start of frame (truncated if too long)
- **Tap a frame** to display its full content in a popup

Frames are saved on the SD card and persist across reboots.
Useful for diagnostics and raw frame analysis.

---

## Stats

Table of received stations with:
- Callsign
- Average RSSI (dBm)
- Average SNR (dB)
- Number of packets received

The 20 most recent stations are saved on the SD card and persist across reboots.

> Stations are ordered by arrival; the oldest is evicted when the list is full.
