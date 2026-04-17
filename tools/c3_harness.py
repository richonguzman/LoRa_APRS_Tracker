#!/usr/bin/env python3
"""
C3 Test Harness — simulates an S3 on the other end of the UART protocol.

Connects to the C3 DevKit via USB serial, sends/receives binary frames
defined in uart_proto.h. Interactive CLI for testing.

Usage:
    python3 c3_harness.py /dev/ttyUSB0
    python3 c3_harness.py /dev/ttyACM0 --baud 460800
"""

import argparse
import struct
import threading
import time
import sys
from datetime import datetime, timezone

import serial

# --- Protocol constants (mirror uart_proto.h) ---

SYNC = 0xAA
MAX_PAYLOAD = 512

# Message types C3 -> S3
MSG_GPS_FIX     = 0x01
MSG_LORA_RX     = 0x02
MSG_LORA_TX_ACK = 0x11
MSG_STATUS      = 0x30
MSG_ERROR       = 0x3F

# Message types S3 -> C3
MSG_LORA_TX_REQ = 0x10
MSG_CONFIG      = 0x20
MSG_CONFIG_ACK  = 0x21

# TX ACK status
TX_STATUS = {0x00: "OK", 0x01: "TIMEOUT", 0x02: "SPI_ERROR", 0x03: "BUSY"}

MSG_NAMES = {
    MSG_GPS_FIX:     "GPS_FIX",
    MSG_LORA_RX:     "LORA_RX",
    MSG_LORA_TX_ACK: "LORA_TX_ACK",
    MSG_STATUS:      "STATUS",
    MSG_ERROR:       "ERROR",
    MSG_LORA_TX_REQ: "LORA_TX_REQ",
    MSG_CONFIG:      "CONFIG",
    MSG_CONFIG_ACK:  "CONFIG_ACK",
}

# --- CRC-16/CCITT (same as proto_crc16) ---

def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

# --- Frame builder / parser ---

def build_frame(msg_type: int, payload: bytes = b"") -> bytes:
    length = len(payload)
    header = bytes([SYNC, msg_type, length & 0xFF, (length >> 8) & 0xFF])
    crc_data = header[1:4] + payload  # TYPE + LEN(2) + PAYLOAD
    crc = crc16(crc_data)
    return header + payload + struct.pack("<H", crc)

class FrameParser:
    """State machine parser, mirrors proto_parser_feed."""

    IDLE, TYPE, LEN_LO, LEN_HI, PAYLOAD, CRC_LO, CRC_HI = range(7)

    def __init__(self):
        self.state = self.IDLE
        self.msg_type = 0
        self.length = 0
        self.payload = bytearray()
        self.crc_lo = 0
        self.frames = []  # list of (type, payload_bytes)
        self.crc_errors = 0

    def feed(self, data: bytes):
        for b in data:
            self._feed_byte(b)

    def _feed_byte(self, b: int):
        if self.state == self.IDLE:
            if b == SYNC:
                self.state = self.TYPE
        elif self.state == self.TYPE:
            self.msg_type = b
            self.state = self.LEN_LO
        elif self.state == self.LEN_LO:
            self.length = b
            self.state = self.LEN_HI
        elif self.state == self.LEN_HI:
            self.length |= b << 8
            if self.length > MAX_PAYLOAD:
                self.state = self.IDLE
            elif self.length == 0:
                self.payload = bytearray()
                self.state = self.CRC_LO
            else:
                self.payload = bytearray()
                self.state = self.PAYLOAD
        elif self.state == self.PAYLOAD:
            self.payload.append(b)
            if len(self.payload) >= self.length:
                self.state = self.CRC_LO
        elif self.state == self.CRC_LO:
            self.crc_lo = b
            self.state = self.CRC_HI
        elif self.state == self.CRC_HI:
            rx_crc = self.crc_lo | (b << 8)
            hdr = bytes([self.msg_type, self.length & 0xFF, (self.length >> 8) & 0xFF])
            calc = crc16(hdr + bytes(self.payload))
            if rx_crc == calc:
                self.frames.append((self.msg_type, bytes(self.payload)))
            else:
                self.crc_errors += 1
            self.state = self.IDLE

    def get_frames(self):
        out = self.frames[:]
        self.frames.clear()
        return out

# --- Payload decoders ---

def decode_gps_fix(data: bytes) -> dict:
    if len(data) < 30:
        return {"error": f"too short ({len(data)}B)"}
    lat, lon, alt, speed, heading, sats, fix_type, hdop, timestamp, flags, _ = \
        struct.unpack("<iiihHBBHIBB", data[:22])
    # Reparse with correct struct size
    fields = struct.unpack("<iiihHBBHIBB", data[:22])
    return {
        "lat": lat / 1e7,
        "lon": lon / 1e7,
        "alt_m": alt / 1000.0,
        "speed_ms": speed / 100.0,
        "heading": heading / 100.0,
        "sats": sats,
        "fix": fix_type,
        "hdop": hdop / 100.0,
        "time": datetime.fromtimestamp(timestamp, tz=timezone.utc).isoformat() if timestamp else "N/A",
        "flags": f"0x{flags:02X}",
    }

def decode_status(data: bytes) -> dict:
    if len(data) < 12:
        return {"error": f"too short ({len(data)}B)"}
    gps_fix, gps_sats, lora_state, flags, rx, tx, err, uptime = \
        struct.unpack("<BBBBHHHH", data[:12])
    lora_states = {0: "IDLE", 1: "RX", 2: "TX", 3: "CAD", 4: "SLEEP"}
    return {
        "gps_fix": gps_fix,
        "gps_sats": gps_sats,
        "lora": lora_states.get(lora_state, f"?{lora_state}"),
        "rx": rx, "tx": tx, "errors": err,
        "uptime_min": uptime,
    }

def decode_tx_ack(data: bytes) -> dict:
    if len(data) < 4:
        return {"error": f"too short ({len(data)}B)"}
    status, _, irq = struct.unpack("<BBH", data[:4])
    return {
        "status": TX_STATUS.get(status, f"?{status}"),
        "irq_flags": f"0x{irq:04X}",
    }

def decode_error(data: bytes) -> dict:
    if len(data) < 4:
        return {"error": f"too short ({len(data)}B)"}
    code, detail = struct.unpack("<HH", data[:4])
    return {"code": f"0x{code:04X}", "detail": f"0x{detail:04X}"}

def decode_lora_rx(data: bytes) -> dict:
    if len(data) < 8:
        return {"error": f"too short ({len(data)}B)"}
    rssi, snr, cr, pkt_len, freq_err = struct.unpack("<hbBHH", data[:8])
    pkt = data[8:8 + pkt_len] if len(data) >= 8 + pkt_len else b""
    return {
        "rssi": rssi / 10.0,
        "snr": snr / 4.0,
        "cr": cr,
        "freq_err_hz": freq_err,
        "pkt_len": pkt_len,
        "aprs": pkt[3:].decode("ascii", errors="replace") if len(pkt) > 3 else pkt.decode("ascii", errors="replace"),
    }

DECODERS = {
    MSG_GPS_FIX:     decode_gps_fix,
    MSG_STATUS:      decode_status,
    MSG_LORA_TX_ACK: decode_tx_ack,
    MSG_ERROR:       decode_error,
    MSG_LORA_RX:     decode_lora_rx,
}

# --- Payload builders (S3 -> C3) ---

def build_config(freq=433775000, tx_power=22, sf=12, bw=7, cr=5,
                 preamble=8, sb_active=1, sb_slow=180, sb_fast=60,
                 sb_min_speed=5, sb_max_speed=90) -> bytes:
    return struct.pack("<IbBBBHBBBBB",
                       freq, tx_power, sf, bw, cr, preamble,
                       sb_active, sb_slow, sb_fast, sb_min_speed, sb_max_speed)

def build_tx_req(aprs_packet: str) -> bytes:
    raw = b"\x3c\xff\x01" + aprs_packet.encode("ascii")
    return struct.pack("<H", len(raw)) + raw

# --- RX thread ---

def rx_thread(ser: serial.Serial, parser: FrameParser, running: threading.Event):
    while running.is_set():
        data = ser.read(ser.in_waiting or 1)
        if data:
            parser.feed(data)
            for msg_type, payload in parser.get_frames():
                name = MSG_NAMES.get(msg_type, f"0x{msg_type:02X}")
                decoder = DECODERS.get(msg_type)
                if decoder:
                    info = decoder(payload)
                    print(f"\n  << {name}: {info}")
                else:
                    print(f"\n  << {name}: {payload.hex()}")
                print("> ", end="", flush=True)

# --- CLI ---

def print_help():
    print("""
Commands:
  config [freq]     Send CONFIG (default 433775000 Hz)
  tx <text>         Send LORA_TX_REQ with APRS packet text
  ping              Send empty CONFIG to trigger ACK
  status            (passive — C3 sends STATUS periodically)
  stats             Show parser stats (frames, CRC errors)
  raw <hex>         Send raw bytes (hex string)
  help              This help
  quit              Exit
""")

def main():
    ap = argparse.ArgumentParser(description="C3 UART test harness")
    ap.add_argument("port", help="Serial port (e.g. /dev/ttyUSB0)")
    ap.add_argument("--baud", type=int, default=460800)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.1)
    parser = FrameParser()
    running = threading.Event()
    running.set()

    t = threading.Thread(target=rx_thread, args=(ser, parser, running), daemon=True)
    t.start()

    print(f"Connected to {args.port} @ {args.baud} baud")
    print("Type 'help' for commands.\n")

    try:
        while True:
            try:
                cmd = input("> ").strip()
            except EOFError:
                break
            if not cmd:
                continue

            parts = cmd.split(maxsplit=1)
            verb = parts[0].lower()

            if verb == "quit" or verb == "q":
                break
            elif verb == "help" or verb == "h":
                print_help()
            elif verb == "config":
                freq = int(parts[1]) if len(parts) > 1 else 433775000
                frame = build_frame(MSG_CONFIG, build_config(freq=freq))
                ser.write(frame)
                print(f"  >> CONFIG sent (freq={freq})")
            elif verb == "tx":
                if len(parts) < 2:
                    print("Usage: tx <aprs text>")
                    continue
                frame = build_frame(MSG_LORA_TX_REQ, build_tx_req(parts[1]))
                ser.write(frame)
                print(f"  >> TX_REQ sent ({len(parts[1])} chars)")
            elif verb == "ping":
                frame = build_frame(MSG_CONFIG, build_config())
                ser.write(frame)
                print("  >> CONFIG (ping) sent")
            elif verb == "stats":
                print(f"  CRC errors: {parser.crc_errors}")
            elif verb == "raw":
                if len(parts) < 2:
                    print("Usage: raw <hex>")
                    continue
                ser.write(bytes.fromhex(parts[1]))
                print(f"  >> Raw sent ({len(parts[1])//2} bytes)")
            else:
                print(f"Unknown command: {verb}. Type 'help'.")

    except KeyboardInterrupt:
        pass
    finally:
        running.clear()
        ser.close()
        print("\nBye.")

if __name__ == "__main__":
    main()
