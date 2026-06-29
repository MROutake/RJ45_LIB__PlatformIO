#pragma once
#include <Arduino.h>

// ── Empfangene Befehle (Server → Arduino) ───────────────────────────────────

/// Verbindungsprüfung — wird automatisch mit CMD_PONG beantwortet.
constexpr uint8_t CMD_PING        = 0x01;

/// Keepalive — wird stillschweigend verworfen.
constexpr uint8_t CMD_KEEPALIVE   = 0x02;

/// Bitmask setzen. Payload: [byte0]..[byteN], 1–4 Bytes, Little-Endian.
constexpr uint8_t CMD_RX_BITMASK  = 0x10;

// ── Gesendete Befehle (Arduino → Server) ────────────────────────────────────

/// Antwort auf CMD_PING. Kein Payload.
constexpr uint8_t CMD_PONG        = 0x99;

/// Bitmask-Status senden. Payload: [byte0]..[byteN], 1–4 Bytes, Little-Endian.
constexpr uint8_t CMD_TX_BITMASK  = 0x98;

/// ID + Messwert senden. Payload: [id][value_hi][value_lo], 3 Bytes.
constexpr uint8_t CMD_TX_ID_VALUE = 0x80;
