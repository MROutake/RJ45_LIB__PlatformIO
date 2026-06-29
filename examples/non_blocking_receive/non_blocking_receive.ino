/**
 * Beispiel: 16-Kanal-Relais-Steuerung über Ethernet
 *
 * Der Server sendet eine 16-Bit-Bitmask (CMD_RX_BITMASK), die angibt
 * welche Relais ein- oder ausgeschaltet werden sollen.
 * Der Arduino bestätigt den neuen Zustand und meldet alle 5 Sekunden
 * einen Messwert (hier: Dummy-Temperatur).
 *
 * Protokoll-Ablauf:
 *   Server → Arduino : [AA AA 02 10 HI LO CRC]   CMD_RX_BITMASK
 *   Arduino → Server : [AA AA 02 98 HI LO CRC]   CMD_TX_BITMASK  (Bestätigung)
 *   Arduino → Server : [AA AA 03 80 01 HI LO CRC] CMD_TX_ID_VALUE (Messwert)
 *   Server → Arduino : [AA AA 00 01 CRC]           CMD_PING
 *   Arduino → Server : [AA AA 00 99 CRC]           CMD_PONG        (automatisch)
 */

#include <Arduino.h>
#include <Ethernet.h>
#include <RJ45Lib.h>

// ── Netzwerk-Konfiguration ────────────────────────────────────────────────────
byte      mac[]    = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress localIp (192, 168, 0, 50);
IPAddress serverIp(192, 168, 0, 10);
const uint16_t SERVER_PORT = 5000;
const int      CS_PIN      = 10;   // Chip-Select-Pin des Ethernet-Shields

// ── Lib-Objekte (Reihenfolge beachten: client → node → handler) ──────────────
EthernetClient client;
RJ45Node       node(client);
CommandHandler handler(node);

// ── Anwendungszustand ─────────────────────────────────────────────────────────
const uint8_t RELAY_COUNT    = 16;
const uint8_t RELAY_PINS[RELAY_COUNT] = {
    2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A0, A1, A2, A3
};
uint16_t relayState = 0;

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────

void applyRelays(uint16_t mask) {
    relayState = mask;
    for (uint8_t i = 0; i < RELAY_COUNT; ++i) {
        digitalWrite(RELAY_PINS[i], (mask >> i) & 1 ? HIGH : LOW);
    }
}

// ── Befehl-Callbacks ──────────────────────────────────────────────────────────

// Wird aufgerufen wenn der Server CMD_RX_BITMASK sendet.
// Payload: 2 Bytes Little-Endian → 16-Bit-Maske für die Relais.
void onSetRelays(const Packet& p) {
    uint32_t mask = CommandHandler::extractBitmask(p, 2);
    applyRelays(static_cast<uint16_t>(mask));

    // Neuen Zustand an den Server zurückmelden
    handler.sendBitmask(CMD_TX_BITMASK, relayState, 2);
}

// ── Fehler-Callback ───────────────────────────────────────────────────────────

// Wird nur für externe (nicht-selbst-heilbare) Fehler aufgerufen.
// Intern behebbare Fehler (Reconnect läuft, Paket verworfen) lösen
// keinen Callback aus — die Lib kümmert sich selbst darum.
void onNetworkError(ErrorSource src, uint8_t code) {
    Serial.print(F("[ERR] src="));
    switch (src) {
        case ErrorSource::Network:        Serial.print(F("Network"));        break;
        case ErrorSource::Manager:        Serial.print(F("Manager"));        break;
        case ErrorSource::CommandHandler: Serial.print(F("CommandHandler")); break;
        default:                          Serial.print(F("?"));              break;
    }
    Serial.print(F(" code=0x"));
    Serial.println(code, HEX);

    // Diagnose-Tipp: Fehlercode gegen Errors.h prüfen.
    // ERR_NET_NO_HARDWARE (0x01) → Ethernet-Chip nicht gefunden → Hardware prüfen.
    // ERR_NET_NO_CABLE    (0x02) → Kabel nicht gesteckt beim Start.
    // ERR_MGR_RECONNECT_FAIL (0x04) → Server nicht erreichbar, Lib versucht weiter.
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Relais-Pins konfigurieren
    for (uint8_t i = 0; i < RELAY_COUNT; ++i) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], LOW);
    }

    // 1. Fehler-Callback registrieren (vor begin())
    node.onError(onNetworkError);

    // 2. Befehl-Callbacks registrieren (vor begin())
    //    CMD_PING / CMD_KEEPALIVE werden automatisch behandelt — kein on() nötig.
    handler.on(CMD_RX_BITMASK, onSetRelays);

    // 3. Verbindung aufbauen
    //    begin() blockiert bis Ethernet initialisiert ist.
    //    Liefert false wenn Hardware fehlt oder Server nicht erreichbar.
    if (!node.begin(mac, localIp, serverIp, SERVER_PORT, CS_PIN)) {
        Serial.println(F("[WARN] begin() fehlgeschlagen — Lib versucht automatisch neu."));
    }
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    // readPacket() ist non-blocking: gibt true zurück wenn ein vollständiges,
    // gültiges Paket eingegangen ist. Intern wird automatisch reconnectet
    // wenn die Verbindung verloren geht.
    Packet p;
    if (node.readPacket(p)) {
        // dispatch() sucht den passenden Callback für p.command.
        // CMD_PING → sendPong() automatisch; CMD_KEEPALIVE → verworfen.
        handler.dispatch(p);
    }

    // Periodisch einen Messwert senden (ID=0x01, Wert=235 entspricht 23.5 °C × 10)
    static unsigned long lastSendMs = 0;
    if (millis() - lastSendMs >= 5000) {
        lastSendMs = millis();
        handler.sendIdValue(CMD_TX_ID_VALUE, 0x01, 235);
    }
}
