#pragma once
#include <Arduino.h>

/// Quelle eines Fehlers innerhalb der Lib.
enum class ErrorSource : uint8_t {
    Network        = 0, ///< Netzwerkschicht (Ethernet, TCP)
    Parser         = 1, ///< Paket-Parser
    Manager        = 2, ///< RJ45Node (Verbindungsverwaltung, Senden)
    CommandHandler = 3, ///< CommandHandler (Callback-System)
};

/**
 * @brief Fehler-Callback-Typ.
 *
 * Wird nur bei nicht-behebbaren Fehlern aufgerufen.
 * Intern behebbare Fehler (auto-reconnect, schlechtes Paket verwerfen)
 * lösen keinen Callback aus.
 *
 * @param source  Quelle des Fehlers
 * @param code    Fehlercode (siehe ERR_*-Konstanten)
 */
typedef void (*ErrorCallback)(ErrorSource source, uint8_t code);

// ── Fehlercodes: Network ─────────────────────────────────────────────────────
constexpr uint8_t ERR_NET_NO_HARDWARE    = 0x01; ///< Ethernet-Chip nicht gefunden (fatal)
constexpr uint8_t ERR_NET_NO_CABLE       = 0x02; ///< Kein Kabel beim Setup
constexpr uint8_t ERR_NET_CONNECT_FAILED = 0x03; ///< TCP-Verbindung fehlgeschlagen
constexpr uint8_t ERR_NET_DISCONNECTED   = 0x04; ///< Verbindung getrennt — intern, auto-reconnect

// ── Fehlercodes: Parser ──────────────────────────────────────────────────────
// Alle Parser-Fehler sind intern — das fehlerhafte Paket wird verworfen,
// die Lib läuft ohne Unterbrechung weiter.
constexpr uint8_t ERR_PARSE_NULL_INPUT   = 0x01; ///< Null-Pointer übergeben
constexpr uint8_t ERR_PARSE_BAD_HEADER   = 0x02; ///< Sync-Bytes stimmen nicht
constexpr uint8_t ERR_PARSE_BAD_LENGTH   = 0x03; ///< Paket zu kurz oder Payload > MAX_PAYLOAD
constexpr uint8_t ERR_PARSE_BAD_CRC      = 0x04; ///< CRC-Prüfung fehlgeschlagen

// ── Fehlercodes: Manager ─────────────────────────────────────────────────────
constexpr uint8_t ERR_MGR_MEMORY         = 0x02; ///< Speicher-Allokierung fehlgeschlagen (fatal)
constexpr uint8_t ERR_MGR_NOT_INIT       = 0x03; ///< readPacket/send vor begin() aufgerufen
constexpr uint8_t ERR_MGR_RECONNECT_FAIL = 0x04; ///< Reconnect-Versuch fehlgeschlagen
constexpr uint8_t ERR_MGR_RECONNECTING   = 0x05; ///< Warte auf Reconnect-Intervall — intern
constexpr uint8_t ERR_MGR_BAD_PARAMS     = 0x06; ///< Ungültige Parameter (z.B. pollDelayMs = 0)
constexpr uint8_t ERR_MGR_NOT_CONNECTED  = 0x07; ///< Senden während Verbindung getrennt — intern
constexpr uint8_t ERR_MGR_SEND_FAILED    = 0x08; ///< TCP-Write lieferte falsche Bytezahl

// ── Fehlercodes: CommandHandler ──────────────────────────────────────────────
constexpr uint8_t ERR_CMD_LIMIT_REACHED  = 0x01; ///< MAX_HANDLERS überschritten
constexpr uint8_t ERR_CMD_BAD_LENGTH     = 0x02; ///< sendBitmask mit length 0 oder > 4

/**
 * @brief Interner Fehler-Dispatcher.
 *
 * Klassifiziert Fehler als extern (Callback auslösen) oder intern
 * (Lib behandelt selbst, kein Callback). Wird von RJ45Node verwendet.
 */
class ErrorHandler {
public:
    ErrorHandler();

    /// Fehler-Callback registrieren.
    void setCallback(ErrorCallback cb);

    /**
     * @brief Fehler melden.
     *
     * Ruft den Callback nur auf wenn der Fehler nicht intern behebbar ist
     * und ein Callback registriert ist. code == 0x00 wird immer ignoriert.
     */
    void report(ErrorSource source, uint8_t code);

private:
    ErrorCallback _callback;

    /// Gibt true zurück wenn der Fehler nach außen gemeldet werden soll.
    static bool isExternal(ErrorSource source, uint8_t code);
};
