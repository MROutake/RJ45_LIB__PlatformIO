#pragma once
#include <Arduino.h>
#include <Packet.h>
#include <Commands.h>
#include <Errors.h>
#include <RJ45Node.h>

/// Callback-Typ für empfangene Befehle.
typedef void (*CommandCallback)(const Packet&);

/// Maximale Anzahl gleichzeitig registrierbarer Callbacks.
constexpr uint8_t MAX_HANDLERS = 8;

/**
 * @brief Callback-basierter Befehlsrouter.
 *
 * Ordnet eingehende Befehlsbytes registrierten Callback-Funktionen zu.
 * CMD_PING und CMD_KEEPALIVE werden intern behandelt, können aber durch
 * eigene Callbacks überschrieben werden.
 *
 * Verwendung:
 * @code
 *   CommandHandler handler(node);
 *   handler.on(CMD_RX_BITMASK, [](const Packet& p) {
 *       uint32_t mask = CommandHandler::extractBitmask(p, 2);
 *       ...
 *   });
 *
 *   // in loop():
 *   if (node.readPacket(p)) handler.dispatch(p);
 * @endcode
 */
class CommandHandler {
public:
    /// @param node  RJ45Node-Instanz für Senden und Fehlerreporting.
    CommandHandler(RJ45Node& node);

    /**
     * @brief Callback für einen Befehlsbyte registrieren.
     *
     * Reihenfolge der Registrierung bestimmt keine Priorität — jeder
     * Befehlsbyte kann nur einmal registriert werden (erster Treffer gewinnt).
     * Meldet ERR_CMD_LIMIT_REACHED wenn MAX_HANDLERS überschritten.
     *
     * @param command   Befehlsbyte auf den reagiert werden soll
     * @param callback  Funktion die bei diesem Befehl aufgerufen wird
     */
    void on(uint8_t command, CommandCallback callback);

    /**
     * @brief Eingehendes Paket an registrierten Callback weiterleiten.
     *
     * Eigene Callbacks haben Vorrang — auch CMD_PING und CMD_KEEPALIVE
     * können überschrieben werden. In loop() nach readPacket() aufrufen.
     */
    void dispatch(const Packet& packet);

    /**
     * @brief Bitmask aus Paket-Payload lesen.
     *
     * Liest @p length Bytes Little-Endian aus dem Payload und gibt sie
     * als uint32_t zurück. Passend zu sendBitmask().
     *
     * @param packet  Empfangenes Paket
     * @param length  Anzahl Bytes die gelesen werden sollen (1–4)
     * @return Bitmask als uint32_t
     */
    static uint32_t extractBitmask(const Packet& packet, uint8_t length);

    /**
     * @brief ID + 16-Bit-Wert aus Paket-Payload lesen.
     *
     * Erwartet mindestens 3 Bytes: [id][value_hi][value_lo].
     * Passend zu sendIdValue().
     *
     * @param packet  Empfangenes Paket
     * @param id      Ausgabe: ID-Byte (payload[0])
     * @param value   Ausgabe: Wert Big-Endian aus payload[1..2]
     * @return false wenn payload.length < 3
     */
    static bool extractIdValue(const Packet& packet, uint8_t& id, uint16_t& value);

    /**
     * @brief Bitmask mit beliebigem Befehlsbyte senden.
     *
     * @param command  Befehlsbyte
     * @param bitmask  Zu sendende Bitmask (bis 32 Bit)
     * @param length   Anzahl Bytes im Payload (1–4, Little-Endian)
     */
    void sendBitmask(uint8_t command, uint32_t bitmask, uint8_t length);

    /**
     * @brief ID + 16-Bit-Wert mit beliebigem Befehlsbyte senden.
     *
     * Payload: [id][value_hi][value_lo] — 3 Bytes Big-Endian für value.
     *
     * @param command  Befehlsbyte
     * @param id       ID-Byte (z.B. Sensor-Nummer)
     * @param value    Messwert (skaliert, z.B. Temperatur × 10)
     */
    void sendIdValue(uint8_t command, uint8_t id, uint16_t value);

private:
    RJ45Node& _node;

    struct Entry {
        uint8_t         command;
        CommandCallback callback;
        bool            active;
    };

    Entry   _handlers[MAX_HANDLERS];
    uint8_t _handlerCount;

    void sendPong();
};
