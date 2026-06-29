
# Platform.ini anpassen

- Die Lib wird aus GitHub importiert und in .pio\libdeps\nano33ble\RJ45_Module gespeichert
  Es muss extern über den Libary-manager noch https://www.arduino.cc/en/Reference/Ethernet hinzugefügt werden

```cpp
[env:nano33ble]
// Standard Teil der ini
platform = nordicnrf52
board = nano33ble
framework = arduino

// Muss ergänst werden lädt die Lib runter 
lib_deps = https://github.com/MROutake/RJ45_LIB__PlatformIO.git
```

# einfaches Beispiel

```cpp
#include <Arduino.h>
#include <Ethernet.h>
#include <RJ45Lib.h>

// 1. Objekte anlegen (Reihenfolge: client → node → handler)
EthernetClient client;
RJ45Node       node(client);
CommandHandler handler(node);

byte      mac[]    = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress localIp (192, 168, 0, 50);
IPAddress serverIp(192, 168, 0, 10);

// 2. Callback für einen empfangenen Befehl
void onBitmask(const Packet& p) {
    uint32_t mask = CommandHandler::extractBitmask(p, 2);
    // ... Relais setzen o.ä.
}

void setup() {
    // 3. Fehler-Callback registrieren
    node.onError([](ErrorSource src, uint8_t code) {
        Serial.print("Fehler src="); Serial.print((uint8_t)src);
        Serial.print(" code=0x");    Serial.println(code, HEX);
    });

    // 4. Befehl-Callbacks registrieren
    handler.on(CMD_RX_BITMASK, onBitmask);

    // 5. Verbindung aufbauen
    node.begin(mac, localIp, serverIp, 5000, 10);
}

void loop() {
    // 6. Im Loop: Paket lesen und verteilen
    Packet p;
    if (node.readPacket(p)) handler.dispatch(p);
}
```

---

## Schritt für Schritt

### 1. Objekte anlegen

```cpp
EthernetClient client;   // Arduino Ethernet-Bibliothek
RJ45Node       node(client);
CommandHandler handler(node);
```

`RJ45Node` nimmt eine Referenz auf den `EthernetClient`. `CommandHandler` nimmt eine Referenz auf den `RJ45Node`. Die Reihenfolge der Deklaration muss stimmen — und alle drei müssen globale Variablen sein (nicht lokal in `setup()`), weil sie die gesamte Laufzeit existieren müssen.

---

### 2. Fehler-Callback registrieren

```cpp
node.onError([](ErrorSource src, uint8_t code) {
    // src  → wo der Fehler aufgetreten ist
    // code → was genau schiefgelaufen ist (siehe Fehlercodes unten)
});
```

**Wichtig:** `onError` muss vor `begin()` aufgerufen werden. Der Callback wird nur für Fehler aufgerufen, die die Lib nicht selbst beheben kann (z. B. kein Ethernet-Chip). Fehler die intern behoben werden (Verbindung getrennt → Reconnect läuft, beschädigtes Paket → verworfen) lösen **keinen** Callback aus.

`ErrorSource` kann folgende Werte haben:

| Wert | Quelle |
|------|--------|
| `ErrorSource::Network` | Ethernet-Schicht |
| `ErrorSource::Parser` | Paket-Parser (immer intern) |
| `ErrorSource::Manager` | RJ45Node-Verwaltung |
| `ErrorSource::CommandHandler` | Callback-Router |

---

### 3. Befehl-Callbacks registrieren

```cpp
handler.on(CMD_RX_BITMASK, onBitmask);   // Bitmask vom Server empfangen
handler.on(0x20, meinEigenerBefehl);     // beliebiger Befehlsbyte möglich
```

- Maximal **8 Callbacks** gleichzeitig (`MAX_HANDLERS`)
- Jeder Befehlsbyte kann nur einmal registriert werden — der erste Eintrag  wird beahalten
- `CMD_PING` und `CMD_KEEPALIVE` werden automatisch behandelt, können aber durch eigene Callbacks überschrieben werden

Eine Callback-Funktion hat immer diese Signatur:

```cpp
void meinCallback(const Packet& p) {
    // p.command  → welcher Befehl
    // p.length   → Anzahl Payload-Bytes
    // p.payload  → Nutzdaten (max. 32 Bytes)
}
```

---

### 4. Verbindung aufbauen

```cpp
bool ok = node.begin(mac, localIp, serverIp, port, csPin);
// optional: mit eigenem Header-Layout
bool ok = node.begin(mac, localIp, serverIp, port, csPin, layout);
```

| Parameter | Typ | Beschreibung |
|-----------|-----|-------------|
| `mac` | `byte[6]` | MAC-Adresse des Shields |
| `localIp` | `IPAddress` | Statische IP des Arduino |
| `serverIp` | `IPAddress` | IP des Servers |
| `port` | `uint16_t` | TCP-Port des Servers |
| `csPin` | `int` | Chip-Select-Pin des Ethernet-Shields |
| `layout` | `PacketLayout` | (optional) eigene Sync-Bytes statt `0xAA 0xAA` |

`begin()` gibt `false` zurück wenn der Ethernet-Chip nicht gefunden wurde oder die Verbindung zum Server fehlschlägt. Die Lib versucht danach in `readPacket()` automatisch neu zu verbinden — ein Programmabbruch sollte nicht nötig sein.

---

### 5. Im Loop: Pakete empfangen

```cpp
void loop() {
    Packet p;
    if (node.readPacket(p)) {
        handler.dispatch(p);
    }
}
```

`readPacket()` ist **non-blocking** — es kehrt sofort zurück wenn kein vollständiges Paket vorliegt. Intern:
- Prüft ob die Verbindung besteht, reconnectet automatisch wenn nicht
- Liest Bytes aus dem TCP-Stream
- Parst das Paket (Header, Länge, CRC) — beschädigte Pakete werden verworfen
- Gibt `true` zurück nur wenn ein vollständiges, gültiges Paket in `p` liegt

`dispatch()` sucht den passenden Callback für `p.command` und ruft ihn auf. Ist kein Callback registriert: `CMD_PING` → automatisches Pong, `CMD_KEEPALIVE` → stillschweigend verworfen, unbekannte Befehle → ignoriert.

**Timing-Parameter** (optional):

```cpp
node.readPacket(p, 2000, 20);
//                  
//pollDelayMs: min. Abstand zwischen Lesevorgängen (Standard: 10 ms)
//reconnectIntervalMs: Pause zwischen Reconnect-Versuchen (Standard: 1000 ms)
```

---

### 6. Daten senden

#### Bitmask senden

```cpp
// 16-Bit-Maske in 2 Bytes (Little-Endian)
handler.sendBitmask(CMD_TX_BITMASK, 0x00FF, 2);

// 32-Bit-Maske in 4 Bytes
handler.sendBitmask(CMD_TX_BITMASK, 0xDEADBEEF, 4);
```

#### ID + Messwert senden

```cpp
// ID=1, Wert=235 (z.B. 23.5 °C × 10)
handler.sendIdValue(CMD_TX_ID_VALUE, 0x01, 235);
// Payload: [0x01][0x00][0xEB]  (Big-Endian für value)
```

#### Rohes Paket senden (direkt über RJ45Node)

```cpp
uint8_t payload[] = {0x01, 0x02, 0x03};
node.sendPacket(0x80, payload, 3);
```

---

## Payload-Hilfsmethoden

### `extractBitmask` — Bitmask aus Paket lesen

```cpp
void onBitmask(const Packet& p) {
    uint32_t mask = CommandHandler::extractBitmask(p, 2);
    // Liest 2 Bytes Little-Endian aus p.payload → uint32_t
    // Byte 0 = Low-Byte, Byte 1 = High-Byte
}
```

| Parameter | Beschreibung |
|-----------|-------------|
| `packet` | Empfangenes Paket |
| `length` | Wie viele Bytes lesen (1–4, wird auf 4 und auf `packet.length` begrenzt) |

### `extractIdValue` — ID + Wert aus Paket lesen

```cpp
void onIdValue(const Packet& p) {
    uint8_t  id;
    uint16_t value;
    if (CommandHandler::extractIdValue(p, id, value)) {
        // id    = p.payload[0]
        // value = (p.payload[1] << 8) | p.payload[2]  (Big-Endian)
    }
    // gibt false zurück wenn p.length < 3
}
```

---

## Fehlerbehandlung im Detail

### Intern vs. extern

| Verhalten | Was passiert | Callback? |
|-----------|-------------|-----------|
| Paket beschädigt (Header/CRC falsch) | wird verworfen, Lib läuft weiter | **Nein** |
| Verbindung getrennt | Reconnect läuft automatisch | **Nein** |
| Reconnect-Intervall läuft noch | readPacket() gibt false zurück | **Nein** |
| Sendeversuch ohne Verbindung | sendPacket() gibt false zurück | **Nein** |
| Ethernet-Chip nicht gefunden | fatal | **Ja** |
| Kein Kabel beim Start | fatal | **Ja** |
| Reconnect dauerhaft fehlgeschlagen | Lib versucht weiter | **Ja** |
| Sendefehler (TCP write failed) | | **Ja** |
| Zu viele Callbacks (`> MAX_HANDLERS`) | Registrierung ignoriert | **Ja** |
| `sendBitmask` mit `length` 0 oder > 4 | Senden ignoriert | **Ja** |

### Alle Fehlercodes

```cpp
// ErrorSource::Network
ERR_NET_NO_HARDWARE    = 0x01  // Ethernet-Chip nicht gefunden → Hardware prüfen
ERR_NET_NO_CABLE       = 0x02  // Kein Kabel beim begin() → Kabel prüfen
ERR_NET_CONNECT_FAILED = 0x03  // TCP-Verbindung fehlgeschlagen → Server prüfen
ERR_NET_DISCONNECTED   = 0x04  // Verbindung getrennt (INTERN, kein Callback)

// ErrorSource::Parser — alle intern, kein Callback
ERR_PARSE_NULL_INPUT   = 0x01  // nullptr übergeben
ERR_PARSE_BAD_HEADER   = 0x02  // Sync-Bytes falsch
ERR_PARSE_BAD_LENGTH   = 0x03  // Paket zu kurz oder Payload > 32 Bytes
ERR_PARSE_BAD_CRC      = 0x04  // Prüfsumme falsch

// ErrorSource::Manager
ERR_MGR_MEMORY         = 0x02  // Speicher-Fehler (fatal)
ERR_MGR_NOT_INIT       = 0x03  // readPacket/send vor begin() aufgerufen
ERR_MGR_RECONNECT_FAIL = 0x04  // Reconnect fehlgeschlagen (Lib versucht weiter)
ERR_MGR_RECONNECTING   = 0x05  // Warte auf Reconnect-Intervall (INTERN)
ERR_MGR_BAD_PARAMS     = 0x06  // pollDelayMs = 0 o.ä.
ERR_MGR_NOT_CONNECTED  = 0x07  // Senden ohne Verbindung (INTERN)
ERR_MGR_SEND_FAILED    = 0x08  // TCP write() hat weniger Bytes gesendet

// ErrorSource::CommandHandler
ERR_CMD_LIMIT_REACHED  = 0x01  // > 8 Callbacks registriert
ERR_CMD_BAD_LENGTH     = 0x02  // sendBitmask mit length 0 oder > 4
```

### Fehlercode nachträglich abfragen

```cpp
uint8_t code = node.getError(ErrorSource::Network);
// 0x00 = kein Fehler
// 0xFF = ungültige ErrorSource
```

---
## Vollständiges Beispiel

```cpp
#include <Arduino.h>
#include <Ethernet.h>
#include <RJ45Lib.h>

// ── Netzwerk ──────────────────────────────────────────────────────────────────
byte      mac[]    = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress localIp (192, 168, 0, 50);
IPAddress serverIp(192, 168, 0, 10);

EthernetClient client;
RJ45Node       node(client);
CommandHandler handler(node);

// ── Anwendung ─────────────────────────────────────────────────────────────────
const uint8_t RELAY_PINS[16] = {2,3,4,5,6,7,8,9,10,11,12,13,A0,A1,A2,A3};
uint16_t relayState = 0;

void applyRelays(uint16_t mask) {
    relayState = mask;
    for (uint8_t i = 0; i < 16; ++i)
        digitalWrite(RELAY_PINS[i], (mask >> i) & 1 ? HIGH : LOW);
}

// ── Befehl-Callbacks ──────────────────────────────────────────────────────────
void onSetRelays(const Packet& p) {
    applyRelays(static_cast<uint16_t>(CommandHandler::extractBitmask(p, 2)));
    handler.sendBitmask(CMD_TX_BITMASK, relayState, 2);
}

// ── Fehler-Callback ───────────────────────────────────────────────────────────
void onError(ErrorSource src, uint8_t code) {
    Serial.print(F("[ERR] src="));
    Serial.print(static_cast<uint8_t>(src));
    Serial.print(F(" code=0x"));
    Serial.println(code, HEX);
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    for (uint8_t i = 0; i < 16; ++i) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], LOW);
    }

    node.onError(onError);
    handler.on(CMD_RX_BITMASK, onSetRelays);

    if (!node.begin(mac, localIp, serverIp, 5000, 10))
        Serial.println(F("[WARN] begin() fehlgeschlagen"));
}

void loop() {
    Packet p;
    if (node.readPacket(p)) handler.dispatch(p);

    // Messwert alle 5 s senden: ID=1, Wert=235 (23.5 °C × 10)
    static unsigned long t = 0;
    if (millis() - t >= 5000) { t = millis(); handler.sendIdValue(CMD_TX_ID_VALUE, 0x01, 235); }
}
```

Testskript für den Python-Server ist unter (test/Python_Server/test_server.py)