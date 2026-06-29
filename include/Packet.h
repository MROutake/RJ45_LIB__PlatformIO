#pragma once
#include <Arduino.h>

/// Maximale Payload-Größe in Bytes pro Paket.
constexpr uint8_t MAX_PAYLOAD = 32;

/**
 * @brief Empfangenes oder zu sendendes Datenpaket.
 *
 * Paketformat im Bytestream:
 *   [header1][header2][length][command][payload...][crc]
 *     1 Byte   1 Byte  1 Byte  1 Byte  0–32 Bytes  1 Byte
 */
struct Packet {
    uint8_t header1;             ///< Erstes Sync-Byte
    uint8_t header2;             ///< Zweites Sync-Byte
    uint8_t length;              ///< Anzahl Payload-Bytes
    uint8_t command;             ///< Befehlsbyte
    uint8_t payload[MAX_PAYLOAD];///< Nutzdaten
    uint8_t crc;                 ///< Prüfsumme über alle Bytes vor dem CRC
};

/**
 * @brief Konfigurierbare Sync-Bytes für den Paketrahmen.
 *
 * Standard: 0xAA 0xAA. Nur ändern wenn das Protokoll andere Header erfordert.
 */
struct PacketLayout {
    uint8_t header1 = 0xAA;
    uint8_t header2 = 0xAA;
};
