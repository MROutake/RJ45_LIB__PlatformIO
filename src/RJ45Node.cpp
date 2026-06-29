#include <RJ45Node.h>

RJ45Node::RJ45Node(EthernetClient& client)
    : _client(client), _port(0), _csPin(0),
      _parser(_rxBuffer),   // _rxBuffer muss im Header VOR _parser deklariert sein
      _hasPending(false), _initialized(false),
      _lastReconnectMs(0), _lastPollMs(0)
{
    memset(_errors, 0, sizeof(_errors));
}

//======================== Öffentliche API ========================

void RJ45Node::onError(ErrorCallback callback)
{
    _errorHandler.setCallback(callback);
}

uint8_t RJ45Node::getError(ErrorSource source) const
{
    uint8_t idx = static_cast<uint8_t>(source);
    return (idx < NUM_ERROR_SOURCES) ? _errors[idx] : 0xFF;
}

void RJ45Node::reportError(ErrorSource source, uint8_t code)
{
    // Kein Eintrag im _errors-Array — CommandHandler-Fehler haben keinen
    // festen Slot, sie werden nur als Ereignis nach außen gemeldet.
    _errorHandler.report(source, code);
}

bool RJ45Node::begin(byte* mac, IPAddress localIp, IPAddress serverIp, uint16_t port, int csPin,
                     const PacketLayout& layout)
{
    memcpy(_mac, mac, 6);
    _localIp  = localIp;
    _serverIp = serverIp;
    _port     = port;
    _csPin    = csPin;
    _layout   = layout;
    _parser.setPacketLayout(layout);

    _initialized     = false;
    _hasPending      = false;
    _lastReconnectMs = 0;
    _lastPollMs      = 0;
    memset(_errors, 0, sizeof(_errors));

    if (!networkSetup()) return false;

    _initialized = true;
    return true;
}

bool RJ45Node::readPacket(Packet& out, unsigned long reconnectIntervalMs, unsigned long pollDelayMs)
{
    if (!_initialized) {
        raiseError(ErrorSource::Manager, ERR_MGR_NOT_INIT);
        return false;
    }
    if (pollDelayMs == 0) {
        // 0 würde in jedem loop()-Durchlauf lesen — blockiert bei hohem Datenstrom
        raiseError(ErrorSource::Manager, ERR_MGR_BAD_PARAMS);
        return false;
    }

    const unsigned long now = millis();

    if (!_client.connected()) {
        if ((now - _lastReconnectMs) >= reconnectIntervalMs) {
            _lastReconnectMs = now;
            if (!tryReconnect()) {
                raiseError(ErrorSource::Manager, ERR_MGR_RECONNECT_FAIL);
                return false;
            }
            // Reconnect erfolgreich — Fehlerstatus zurücksetzen
            _errors[static_cast<uint8_t>(ErrorSource::Network)] = 0x00;
            _errors[static_cast<uint8_t>(ErrorSource::Manager)] = 0x00;
        } else {
            // Intervall läuft noch — kein Callback, einfach warten
            raiseError(ErrorSource::Manager, ERR_MGR_RECONNECTING);
            return false;
        }
    }

    // Non-blocking Poll: Bytestrom nur alle pollDelayMs ms auslesen,
    // damit die loop()-Frequenz nicht durch Netzwerklatenz dominiert wird.
    if ((now - _lastPollMs) < pollDelayMs) return false;
    _lastPollMs = now;

    processIncoming();

    if (_hasPending) {
        out = _pendingPacket;
        _hasPending = false;
        _errors[static_cast<uint8_t>(ErrorSource::Manager)] = 0x00;
        return true;
    }

    return false;
}

bool RJ45Node::sendPacket(uint8_t command, const uint8_t* payload, uint8_t payloadLen)
{
    if (!_initialized || !_client.connected()) {
        raiseError(ErrorSource::Manager, ERR_MGR_NOT_CONNECTED);
        return false;
    }

    // Frame-Layout: [h1][h2][len][cmd][payload...][crc]
    uint8_t frame[MAX_PAYLOAD + 5];
    frame[0] = _layout.header1;
    frame[1] = _layout.header2;
    frame[2] = payloadLen;
    frame[3] = command;
    if (payload && payloadLen > 0) memcpy(&frame[4], payload, payloadLen);

    const size_t frameLen = 4 + payloadLen;
    uint8_t crc = 0;
    for (size_t i = 0; i < frameLen; ++i) crc = static_cast<uint8_t>(crc + frame[i]);
    frame[frameLen] = crc;

    // write() gibt die tatsächlich gesendeten Bytes zurück —
    // bei TCP-Puffer-Überlauf kann das weniger als totalLen sein.
    if (_client.write(frame, frameLen + 1) != frameLen + 1) {
        raiseError(ErrorSource::Manager, ERR_MGR_SEND_FAILED);
        return false;
    }
    return true;
}

//======================== Private Hilfsmethoden ========================

void RJ45Node::raiseError(ErrorSource source, uint8_t code)
{
    uint8_t idx = static_cast<uint8_t>(source);
    if (idx < NUM_ERROR_SOURCES) _errors[idx] = code;
    if (code != 0x00) _errorHandler.report(source, code);
}

bool RJ45Node::networkSetup()
{
    Ethernet.init(_csPin);
    Ethernet.begin(_mac, _localIp);

    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        raiseError(ErrorSource::Network, ERR_NET_NO_HARDWARE);
        return false;
    }
    if (Ethernet.linkStatus() == LinkOFF) {
        raiseError(ErrorSource::Network, ERR_NET_NO_CABLE);
        return false;
    }
    return tryReconnect();
}

bool RJ45Node::tryReconnect()
{
    if (_client.connect(_serverIp, _port)) return true;
    raiseError(ErrorSource::Network, ERR_NET_CONNECT_FAILED);
    return false;
}

void RJ45Node::processIncoming()
{
    if (_client.available() <= 0) {
        _hasPending = false;
        return;
    }

    uint8_t buffer[MAX_PAYLOAD + 5];
    size_t bytesRead = 0;
    while (_client.available() && bytesRead < sizeof(buffer)) {
        buffer[bytesRead++] = static_cast<uint8_t>(_client.read());
    }

    if (bytesRead == 0) {
        _hasPending = false;
        return;
    }

    if (_parser.parsePacket(buffer, bytesRead)) {
        _pendingPacket = _parser.getPacket();
        _hasPending    = true;
        _errors[static_cast<uint8_t>(ErrorSource::Parser)] = 0x00;
    } else {
        // Fehlerhaftes Paket wird verworfen — kein Callback (intern).
        // raiseError() setzt trotzdem den Code im Array für getError().
        raiseError(ErrorSource::Parser, _parser.getError());
        _hasPending = false;
    }
}
