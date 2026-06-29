# Ziel

Die Library kapselt Netzwerkkommunikation, Paket-Parsing und Fehlerbehandlung.
Von außen wird nur [[src\RJ45Node.cpp]]  verwendet.

---
# Verwendung


```cpp

#include <RJ45Lib.h>

EthernetClient client;
RJ45Node       node(client);
CommandHandler handler(node);

byte      mac[]    = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress localIp (192, 168, 0, 50);
IPAddress serverIp(192, 168, 0, 10);


void onBefehl(const Packet& p) {
    uint32_t wert = CommandHandler::extractBitmask(p, 2);
    // ...
}

void setup() {

    node.onError([](ErrorSource src, uint8_t code) {
        Serial.print("Fehler src="); Serial.print((uint8_t)src);
        Serial.print(" code=0x"); Serial.println(code, HEX);
    });

    handler.on(CMD_RX_BITMASK, onBefehl);
    node.begin(mac, localIp, serverIp, 5000, 10);

}

void loop() {

    Packet p;
    if (node.readPacket(p)) handler.dispatch(p);
}

```

---
# Ordner Struktur

Directory structure:
└── Lib_RJ45/
    ├── examples/
    │   └── non_blocking_receive/
    │       └── non_blocking_receive.ino
    ├── include/
    │   ├── CommandHandler.h
    │   ├── Commands.h
    │   ├── Errors.h
    │   ├── Packet.h
    │   ├── Parser.h
    │   ├── RJ45Lib.h
    │   └── RJ45Node.h
    ├── libary.json
    ├── platformio.ini
    ├── README.md
    ├── src/
    │   ├── CommandHandler.cpp
    │   ├── ErrorHandler.cpp
    │   ├── main.cpp
    │   ├── Parser.cpp
    │   └── RJ45Node.cpp
    └── test/
        └── test_parser/
            └── test_main.cpp

| Hauptordner | Inhalt                                                                     |
| ----------- | -------------------------------------------------------------------------- |
| Examples    | Beinhaltet Beispiel-Code für den Arduino wie die LIB verwendet werden kann |
| Include     | Header Dateien und Definition von Klassen, Konstanten und Strukturen       |
| Src         | Haupt-Datein für Klassen und Defintion der Funktionen                      |
| Test        | Test Dateien für Klassen mit Plattform IO Testumgebung                     |

---
# Klassen

## RJ45Node
Zentraler Einstiegspunkt — verwaltet Verbindung, Empfang und Senden.
```cpp
RJ45Node node(client);
```

| Methode                                      | Beschreibung                           |
| -------------------------------------------- | -------------------------------------- |
| `begin(mac, localIp, serverIp, port, csPin)` | Ethernet initialisieren und verbinden  |
| `begin(..., layout)`                         | Mit eigenem Header-Layout              |
| `readPacket(packet)`                         | Eingehendes Paket lesen (non-blocking) |
| `readPacket(packet, reconnectMs, pollMs)`    | Mit eigenen Timing-Parametern          |
| `sendPacket(command, payload, length)`       | Rohes Paket senden                     |
| `onError(callback)`                          | Fehler-Callback registrieren           |
| `getError(source)`                           | Letzten Fehlercode einer Quelle lesen  |

**Timing-Parameter in `readPacket`:**
- `reconnectIntervalMs` (Standard: 1000 ms) — Pause zwischen Reconnect-Versuchen
- `pollDelayMs` (Standard: 10 ms) — Mindestabstand zwischen Lesevorgängen

## CommandHandler

Befehl-Router mit Callback-System.

```cpp
CommandHandler handler(node);
```

| Methode | Beschreibung |
|---------|-------------|
| `on(command, callback)` | Callback für einen Befehlsbyte registrieren (max. 8) |
| `dispatch(packet)` | Paket an passendem Callback weiterleiten |
| `sendBitmask(command, bitmask, length)` | Bitmask senden (1–4 Bytes, Little-Endian) |
| `sendIdValue(command, id, value)` | ID (1 Byte) + Wert (2 Bytes) senden |
| `extractBitmask(packet, length)` | Bitmask aus empfangenem Paket lesen (static) |
| `extractIdValue(packet, id, value)` | ID + Wert aus empfangenem Paket lesen (static) |

---

# Paket Aufbau ( [[include\Packet.h]] )

| Feld | Typ | Beschreibung |
| --- | --- | --- |
| header1 | uint8_t | Header Byte 1 |
| header2 | uint8_t | Header Byte 2 |
| length | uint8_t | Payload-Laenge |
| command | uint8_t | Kommando |
| payload | uint8_t[MAX_PAYLOAD] | Nutzdaten (max. 32 Byte) |
| crc | uint8_t | Checksumme |

## Interpretation

- Die beiden Header-Bytes sind Fest auf den Wert 0xAA gesetzt
- Es gibt verscheiden Kommandos die interpretiert werden können ( [[include\Commands.h]])
  
```cpp
// ── Empfangene Befehle (Server → Arduino) ───────────────────────────────────

/// Verbindungsprüfung — wird automatisch mit CMD_PONG beantwortet.
constexpr uint8_t CMD_PING        = 0x01;
/// Keepalive — wird stillschweigend verworfen.
constexpr uint8_t CMD_KEEPALIVE   = 0x02;
/// Bitmask setzen. Payload: [byte0]..[byteN], 1–4 Bytes, Little-Endian.
constexpr uint8_t CMD_RX_BITMASK  = 0x10;

  

// ── Gesendete Befehle (Arduino → Server) ────────────────────────────────────

/// Antwort auf CMD_PING. Kein Payload.
constexpr uint8_t CMD_PONG        = 0x99;
/// Bitmask-Status senden. Payload: [byte0]..[byteN], 1–4 Bytes, Little-Endian.
constexpr uint8_t CMD_TX_BITMASK  = 0x98;
/// ID + Messwert senden. Payload: [id][value_hi][value_lo], 3 Bytes.
constexpr uint8_t CMD_TX_ID_VALUE = 0x80;
```

Vom Server Kommen:

| Kommando       | Wert | Bedeutung                                                                    |
| -------------- | ---- | ---------------------------------------------------------------------------- |
| CMD_PING       | 0x01 | Debug Funktion zum Testen ob Zwischen beiden Seiten Kommuniziert werden kann |
| CMD_KEEPALIVE  | 0x02 | Funktion zum Erhalt der Verbindug                                            |
| CMD_RX_BITMASK | 0x10 | Empfangen einer Bitmaske                                                     |
Vom Arduino Kommen:

| Kommando        | Wert | Bedeutung                                                                    |
| --------------- | ---- | ---------------------------------------------------------------------------- |
| CMD_PONG        | 0x99 | Debug Funktion zum Testen ob Zwischen beiden Seiten Kommuniziert werden kann |
| CMD_TX_BITMASK  | 0x98 | Senden einer Bitmaske                                                        |
| CMD_TX_ID_VALUE | 0x80 | Senden von Sensor Daten                                                      |

## CRC ([[src\Parser.cpp]])

Aktuell wird eine einfache Byte-Summe verwendet:
  
```cpp
uint8_t Parser::calculateCrc(const uint8_t* data, size_t length) const{
    // Einfache Additionsprüfsumme — kein Carry, Überlauf ist gewollt (uint8_t)
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc = static_cast<uint8_t>(crc + data[i]);
    }
    return crc;
}
```

---
# Fehlercodes ( [[include\Errors.h]] )


```cpp

// ── Fehlercodes: Network ─────────────────────────────────────────────────────

///< Ethernet-Chip nicht gefunden (fatal)
constexpr uint8_t ERR_NET_NO_HARDWARE    = 0x01; 
///< Kein Kabel beim Setup
constexpr uint8_t ERR_NET_NO_CABLE       = 0x02;
///< TCP-Verbindung fehlgeschlagen
constexpr uint8_t ERR_NET_CONNECT_FAILED = 0x03; 
///< Verbindung getrennt — intern, auto-reconnect
constexpr uint8_t ERR_NET_DISCONNECTED   = 0x04; 

// ── Fehlercodes: Parser ──────────────────────────────────────────────────────
// Alle Parser-Fehler sind intern — das fehlerhafte Paket wird verworfen,
// die Lib läuft ohne Unterbrechung weiter.

///< Null-Pointer übergeben
constexpr uint8_t ERR_PARSE_NULL_INPUT   = 0x01; 
///< Sync-Bytes stimmen nicht
constexpr uint8_t ERR_PARSE_BAD_HEADER   = 0x02; 
///< Paket zu kurz oder Payload > MAX_PAYLOAD
constexpr uint8_t ERR_PARSE_BAD_LENGTH   = 0x03; 
///< CRC-Prüfung fehlgeschlagen
constexpr uint8_t ERR_PARSE_BAD_CRC      = 0x04; 

// ── Fehlercodes: Manager ─────────────────────────────────────────────────────

///< Speicher-Allokierung fehlgeschlagen (fatal)
constexpr uint8_t ERR_MGR_MEMORY         = 0x02; 
///< readPacket/send vor begin() aufgerufen
constexpr uint8_t ERR_MGR_NOT_INIT       = 0x03; 
///< Reconnect-Versuch fehlgeschlagen
constexpr uint8_t ERR_MGR_RECONNECT_FAIL = 0x04; 
///< Warte auf Reconnect-Intervall — intern
constexpr uint8_t ERR_MGR_RECONNECTING   = 0x05; 
///< Ungültige Parameter (z.B. pollDelayMs = 0)
constexpr uint8_t ERR_MGR_BAD_PARAMS     = 0x06; 
///< Senden während Verbindung getrennt — intern
constexpr uint8_t ERR_MGR_NOT_CONNECTED  = 0x07; 
///< TCP-Write lieferte falsche Bytezahl
constexpr uint8_t ERR_MGR_SEND_FAILED    = 0x08; 

// ── Fehlercodes: CommandHandler ──────────────────────────────────────────────

 ///< MAX_HANDLERS überschritten
constexpr uint8_t ERR_CMD_LIMIT_REACHED  = 0x01;
///< sendBitmask mit length 0 oder > 4
constexpr uint8_t ERR_CMD_BAD_LENGTH     = 0x02; 
```

Fehler werden automatisch klassifiziert:
- **Intern behebbar** → Lib behandelt selbst, kein Callback
- **Nicht behebbar** → `onError`-Callback wird aufgerufen

```cpp

node.onError([](ErrorSource src, uint8_t code) {
    // src: ErrorSource::Network / Parser / Manager / CommandHandler
    // code: Fehlercode (siehe Tabelle)
});
```

| Quelle | Code | Bedeutung | Behandlung |
|--------|------|-----------|-----------|
| `Network` | `0x01` | Ethernet-Hardware nicht gefunden | **Callback** |
| `Network` | `0x02` | Kein Kabel | **Callback** |
| `Network` | `0x03` | Verbindung fehlgeschlagen | **Callback** |
| `Network` | `0x04` | Verbindung getrennt | intern (auto-reconnect) |
| `Parser` | `0x01–0x04` | Ungültiges Paket (Header/Länge/CRC) | intern (verworfen) |
| `Manager` | `0x04` | Reconnect-Versuch fehlgeschlagen | **Callback** |
| `Manager` | `0x05` | Reconnect wartet auf Intervall | intern |
| `Manager` | `0x06` | Ungültige Parameter | **Callback** |
| `Manager` | `0x08` | Sendefehler | **Callback** |
| `CommandHandler` | `0x01` | Zu viele Callbacks registriert | **Callback** |
| `CommandHandler` | `0x02` | Ungültige Bitmask-Länge | **Callback** |

