#include <Arduino.h>
#include <Ethernet.h>
#include <RJ45Lib.h>

// ── Netzwerk ─────────────────────────────────────────────────────────────────
EthernetClient client;
RJ45Node       node(client);
CommandHandler handler(node);

byte      mac[]    = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress localIp (192, 168, 0, 50);
IPAddress serverIp(192, 168, 0, 10);

// ── Anwendungszustand ─────────────────────────────────────────────────────────
const uint8_t RELAY_PINS[16] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A0, A1, A2, A3};
uint16_t relayState = 0;

void applyRelays(uint16_t mask) {
    relayState = mask;
    for (uint8_t i = 0; i < 16; ++i) {
        digitalWrite(RELAY_PINS[i], (mask >> i) & 1 ? HIGH : LOW);
    }
}

// ── Callbacks ─────────────────────────────────────────────────────────────────
void onSetRelays(const Packet& p) {
    applyRelays(CommandHandler::extractBitmask(p, 2));
    handler.sendBitmask(CMD_TX_BITMASK, relayState, 2);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    for (uint8_t i = 0; i < 16; ++i) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], LOW);
    }

    node.onError([](ErrorSource src, uint8_t code) {
        Serial.print("Fehler src=");
        Serial.print(static_cast<uint8_t>(src));
        Serial.print(" code=0x");
        Serial.println(code, HEX);
    });

    handler.on(CMD_RX_BITMASK, onSetRelays);

    if (!node.begin(mac, localIp, serverIp, 5000, 10)) {
        Serial.println("Verbindung fehlgeschlagen.");
    }
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    Packet p;
    if (node.readPacket(p)) {
        handler.dispatch(p);
    }

    // Beispiel: Messwert alle 5 Sekunden senden (ID 0x01, Wert 235 = 23.5)
    static unsigned long lastMs = 0;
    if (millis() - lastMs >= 5000) {
        lastMs = millis();
        handler.sendIdValue(CMD_TX_ID_VALUE, 0x01, 235);
    }
}
