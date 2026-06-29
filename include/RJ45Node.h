#pragma once
#include <Arduino.h>
#include <Ethernet.h>
#include <SPI.h>
#include <Packet.h>
#include <Parser.h>
#include <Errors.h>

/// Anzahl der Fehlerquellen mit gespeichertem Fehlercode (Network, Parser, Manager).
constexpr uint8_t NUM_ERROR_SOURCES = 3;

/**
 * @brief Zentraler Einstiegspunkt der Lib — verwaltet Ethernet-Verbindung,
 *        Paketempfang und -versand.
 *
 * Verwendung:
 * @code
 *   EthernetClient client;
 *   RJ45Node node(client);
 *
 *   node.onError(...);
 *   node.begin(mac, localIp, serverIp, port, csPin);
 *
 *   // in loop():
 *   Packet p;
 *   if (node.readPacket(p)) { ... }
 * @endcode
 */
class RJ45Node {
public:
    /// @param client  EthernetClient-Instanz; muss länger leben als der Node.
    RJ45Node(EthernetClient& client);

    /**
     * @brief Ethernet initialisieren und TCP-Verbindung zum Server aufbauen.
     *
     * @param mac      6-Byte MAC-Adresse
     * @param localIp  Statische IP des Arduino
     * @param serverIp IP des Servers
     * @param port     TCP-Port des Servers
     * @param csPin    Chip-Select-Pin des Ethernet-Shields
     * @param layout   Optionales Header-Layout (Standard: 0xAA 0xAA)
     * @return true bei Erfolg, false bei Hardware-/Verbindungsfehler
     */
    bool begin(byte* mac, IPAddress localIp, IPAddress serverIp, uint16_t port, int csPin,
               const PacketLayout& layout = PacketLayout());

    /**
     * @brief Eingehendes Paket lesen — non-blocking, in loop() aufrufen.
     *
     * Prüft intern ob die Verbindung besteht und reconnectet automatisch.
     * Gibt nur dann true zurück wenn ein vollständiges, gültiges Paket
     * empfangen wurde.
     *
     * @param out                 Ziel für das empfangene Paket
     * @param reconnectIntervalMs Mindestpause zwischen Reconnect-Versuchen (ms)
     * @param pollDelayMs         Mindestabstand zwischen Lesevorgängen (ms)
     * @return true wenn ein neues Paket in @p out liegt
     */
    bool readPacket(Packet& out,
                    unsigned long reconnectIntervalMs = 1000,
                    unsigned long pollDelayMs = 10);

    /**
     * @brief Paket senden.
     *
     * Baut den Paketrahmen (Header + CRC) intern auf.
     *
     * @param command    Befehlsbyte
     * @param payload    Zeiger auf Payload-Daten (nullptr = kein Payload)
     * @param payloadLen Länge des Payloads in Bytes (max. MAX_PAYLOAD)
     * @return true wenn alle Bytes erfolgreich gesendet wurden
     */
    bool sendPacket(uint8_t command, const uint8_t* payload = nullptr, uint8_t payloadLen = 0);

    /**
     * @brief Fehler-Callback registrieren.
     *
     * Wird asynchron aufgerufen sobald ein nicht-behebbarer Fehler auftritt.
     * Intern behebbare Fehler (Verbindungsabbruch während Reconnect-Intervall,
     * fehlerhafte Pakete) lösen keinen Callback aus.
     */
    void onError(ErrorCallback callback);

    /**
     * @brief Gespeicherten Fehlercode einer Quelle lesen.
     *
     * @param source  ErrorSource::Network, Parser oder Manager
     * @return Letzter Fehlercode; 0x00 = kein Fehler; 0xFF = ungültige Quelle
     */
    uint8_t getError(ErrorSource source) const;

    /**
     * @brief Fehler von extern melden (für CommandHandler).
     *
     * Leitet den Fehler direkt an den ErrorHandler weiter ohne ihn
     * im internen Fehlerarray zu speichern.
     */
    void reportError(ErrorSource source, uint8_t code);

private:
    EthernetClient& _client;
    byte            _mac[6];
    IPAddress       _localIp;
    IPAddress       _serverIp;
    uint16_t        _port;
    int             _csPin;
    PacketLayout    _layout;

    // _rxBuffer MUSS vor _parser deklariert sein — Parser speichert eine Referenz darauf
    // und wird im Konstruktor mit dieser Referenz initialisiert.
    Packet       _rxBuffer;
    Parser       _parser;

    Packet       _pendingPacket;
    bool         _hasPending;
    bool         _initialized;

    unsigned long _lastReconnectMs;
    unsigned long _lastPollMs;
    uint8_t       _errors[NUM_ERROR_SOURCES];
    ErrorHandler  _errorHandler;

    bool networkSetup();
    bool tryReconnect();
    void processIncoming();

    /// Fehlercode im Array speichern und ErrorHandler benachrichtigen.
    void raiseError(ErrorSource source, uint8_t code);
};
