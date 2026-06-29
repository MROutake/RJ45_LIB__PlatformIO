#!/usr/bin/env python3
"""
Test-Server für RJ45_LIB.

Protokoll:  [AA AA][len][cmd][payload...][crc]
CRC:        uint8_t-Summe aller Bytes vor dem CRC (wrapping)

Arduino verbindet sich als Client (IP 192.168.0.50 → Server 192.168.0.10:5000).
Dieses Script übernimmt die Server-Rolle — auf dem PC ausführen, dann Arduino starten.
"""

import socket
import threading
import time

HOST = "0.0.0.0"
PORT = 5000

HEADER = bytes([0xAA, 0xAA])

CMD_PING        = 0x01
CMD_KEEPALIVE   = 0x02
CMD_RX_BITMASK  = 0x10
CMD_PONG        = 0x99
CMD_TX_BITMASK  = 0x98
CMD_TX_ID_VALUE = 0x80

CMD_NAMES = {
    CMD_PING:        "PING",
    CMD_KEEPALIVE:   "KEEPALIVE",
    CMD_RX_BITMASK:  "RX_BITMASK",
    CMD_PONG:        "PONG",
    CMD_TX_BITMASK:  "TX_BITMASK",
    CMD_TX_ID_VALUE: "TX_ID_VALUE",
}


def crc8(data: bytes) -> int:
    return sum(data) & 0xFF


def build_packet(cmd: int, payload: bytes = b"") -> bytes:
    frame = HEADER + bytes([len(payload), cmd]) + payload
    return frame + bytes([crc8(frame)])


def parse_packet(data: bytes):
    """Gibt (cmd, payload) zurück oder None bei ungültigem Paket."""
    if len(data) < 5:
        return None
    if data[0] != 0xAA or data[1] != 0xAA:
        return None
    payload_len = data[2]
    packet_len = payload_len + 5
    if len(data) < packet_len:
        return None
    received_crc = data[packet_len - 1]
    if received_crc != crc8(data[:packet_len - 1]):
        print(f"\n  [WARN] CRC-Fehler: erhalten=0x{received_crc:02X} "
              f"erwartet=0x{crc8(data[:packet_len-1]):02X}")
        return None
    return data[3], data[4:4 + payload_len]


def receive_loop(conn: socket.socket, stop: threading.Event) -> None:
    buf = bytearray()
    conn.settimeout(0.5)

    while not stop.is_set():
        try:
            chunk = conn.recv(64)
            if not chunk:
                print("\n[INFO] Verbindung vom Arduino getrennt.")
                stop.set()
                break
            buf.extend(chunk)
        except socket.timeout:
            continue
        except OSError:
            break

        # vollständige Pakete aus dem Stream extrahieren
        while len(buf) >= 5:
            if buf[0] != 0xAA or buf[1] != 0xAA:
                buf.pop(0)
                continue
            packet_len = buf[2] + 5
            if len(buf) < packet_len:
                break
            result = parse_packet(bytes(buf[:packet_len]))
            del buf[:packet_len]
            if result is None:
                continue

            cmd, payload = result
            ts = time.strftime("%H:%M:%S")
            name = CMD_NAMES.get(cmd, f"0x{cmd:02X}")

            if cmd == CMD_PONG:
                print(f"\n  [{ts}] ← PONG")
            elif cmd == CMD_TX_BITMASK:
                mask = int.from_bytes(payload[:2], "little") if len(payload) >= 2 else 0
                print(f"\n  [{ts}] ← TX_BITMASK   state=0b{mask:016b}  (0x{mask:04X})")
            elif cmd == CMD_TX_ID_VALUE:
                if len(payload) >= 3:
                    sensor_id = payload[0]
                    value = (payload[1] << 8) | payload[2]
                    print(f"\n  [{ts}] ← TX_ID_VALUE  id=0x{sensor_id:02X}"
                          f"  raw={value}  ({value / 10:.1f} wenn ×10-skaliert)")
                else:
                    print(f"\n  [{ts}] ← {name}  payload={payload.hex()}")
            else:
                print(f"\n  [{ts}] ← {name}  payload={payload.hex()}")

            print("  > ", end="", flush=True)


HELP = """
Befehle:
  ping              — CMD_PING senden  (Arduino antwortet automatisch mit PONG)
  keep              — CMD_KEEPALIVE senden
  relay <wert>      — CMD_RX_BITMASK: 16-Bit-Relaisbitmask setzen
                        relay 0xFF00   → obere 8 Relais ein
                        relay 65535    → alle 16 ein
                        relay 0        → alle aus
  help / ?          — diese Hilfe
  quit / exit       — beenden
"""


def main() -> None:
    print(f"RJ45 Test-Server  —  lausche auf {HOST}:{PORT} ...")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as srv:
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((HOST, PORT))
        srv.listen(1)
        srv.settimeout(1.0)

        conn = addr = None
        while conn is None:
            try:
                conn, addr = srv.accept()
            except socket.timeout:
                continue

        print(f"Arduino verbunden: {addr[0]}:{addr[1]}")
        print(HELP)

        stop = threading.Event()
        rx = threading.Thread(target=receive_loop, args=(conn, stop), daemon=True)
        rx.start()

        try:
            while not stop.is_set():
                try:
                    line = input("  > ").strip()
                except (EOFError, KeyboardInterrupt):
                    break
                if not line:
                    continue

                parts = line.split()
                cmd = parts[0].lower()

                if cmd in ("quit", "exit"):
                    break
                elif cmd in ("help", "?"):
                    print(HELP)
                elif cmd == "ping":
                    conn.sendall(build_packet(CMD_PING))
                    print("  → PING gesendet")
                elif cmd == "keep":
                    conn.sendall(build_packet(CMD_KEEPALIVE))
                    print("  → KEEPALIVE gesendet")
                elif cmd == "relay":
                    if len(parts) < 2:
                        print("  Verwendung: relay <hex oder dez>   Beispiel: relay 0xFF00")
                        continue
                    try:
                        mask = int(parts[1], 0) & 0xFFFF
                    except ValueError:
                        print("  Ungültiger Wert.   Beispiel: relay 0xFF00 oder relay 255")
                        continue
                    conn.sendall(build_packet(CMD_RX_BITMASK, mask.to_bytes(2, "little")))
                    print(f"  → RX_BITMASK gesendet: 0b{mask:016b}  (0x{mask:04X})")
                else:
                    print(f"  Unbekannter Befehl '{cmd}' — 'help' für Übersicht")
        finally:
            stop.set()
            conn.close()
            print("\nServer beendet.")


if __name__ == "__main__":
    main()
