#pragma once
#include <Arduino.h>
#include <Errors.h>
#include <Packet.h>

/**
 * @brief Parst einen Bytestream in ein Packet-Objekt.
 *
 * Prüft Header-Bytes, Payload-Länge und CRC. Schreibt das Ergebnis
 * direkt in das per Referenz übergebene Packet.
 */
class Parser {
public:
    /**
     * @param packet  Ziel-Objekt für erfolgreich geparste Pakete.
     *                Muss länger leben als der Parser selbst.
     */
    Parser(Packet& packet);

    /**
     * @brief Erwartethe Sync-Bytes konfigurieren.
     *
     * Standard: 0xAA 0xAA (PacketLayout-Defaultwert).
     * Muss vor dem ersten parsePacket()-Aufruf gesetzt werden.
     */
    void setPacketLayout(const PacketLayout& layout);

    /**
     * @brief Bytearray in ein Paket parsen.
     *
     * @param data    Rohdaten aus dem TCP-Stream
     * @param length  Anzahl gültiger Bytes in data
     * @return true bei erfolgreichem Parse; false + gesetzter Fehlercode sonst
     */
    bool parsePacket(const uint8_t* data, size_t length);

    /// Zuletzt erfolgreich gepartes Paket (nur gültig wenn parsePacket() true lieferte).
    const Packet& getPacket() const;

    /// Fehlercode des letzten parsePacket()-Aufrufs. 0x00 = kein Fehler.
    uint8_t getError() const;

    /// Anzahl Payload-Bytes des zuletzt geparsten Pakets.
    size_t getPayloadLength() const;

    /// Maximale Gesamtlänge eines Pakets: MAX_PAYLOAD + 5 Overhead-Bytes.
    size_t maxPacketLength() const;

private:
    Packet&      _packet;
    uint8_t      _errorCode;
    PacketLayout _layout;

    void    setError(uint8_t code);
    uint8_t calculateCrc(const uint8_t* data, size_t length) const;
};
