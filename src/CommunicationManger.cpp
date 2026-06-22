#include <CommunicationManager_H.h>

CommunicationManager::CommunicationManager(EthernetClient& client)
    : _client(client), _packetParser(_internalPacket), _networkInterface(nullptr), _hasPublicOutput(false), _isInitialized(false), _lastReconnectAttemptMs(0), _lastPollMs(0)
{
    // Fehlercode-Array initialisieren
    for (int i = 0; i < num_erors; ++i) {
        _errorcode[i] = 0x00; // Kein Fehler
    }
}

CommunicationManager::~CommunicationManager()
{
    if (_networkInterface != nullptr) {
        delete _networkInterface;
        _networkInterface = nullptr;
    }
}

byte CommunicationManager::get_error(int index) const
{
    if (index >= 0 && index < num_erors) {
        return _errorcode[index];
    }
    return 0xFF; // Ungültiger Index
}

bool CommunicationManager::begin(byte* mac, IPAddress localIp, IPAddress serverIp, int port, int csPin, const PacketLayout& layout)
{
    if (_networkInterface != nullptr) {
        delete _networkInterface;
        _networkInterface = nullptr;
    }

    _packetParser.setPacketLayout(layout);
    _networkInterface = new RJ45(mac, localIp, serverIp, port, csPin, _client);

    if (_networkInterface == nullptr) {
        _errorcode[2] = 0x02; // Speicherfehler
        _isInitialized = false;
        return false;
    }

    if (!_networkInterface->setup()) {
        _errorcode[0] = _networkInterface->read_errorCode();
        _isInitialized = false;
        return false;
    }

    _errorcode[0] = 0x00;
    _errorcode[1] = 0x00;
    _errorcode[2] = 0x00;
    _lastReconnectAttemptMs = 0;
    _lastPollMs = 0;
    _hasPublicOutput = false;
    _isInitialized = true;
    return true;
}



void CommunicationManager::handleIncomingData()
{
    if (!_isInitialized || _networkInterface == nullptr) {
        _errorcode[2] = 0x03; // Noch nicht initialisiert
        _hasPublicOutput = false;
        return;
    }

    if (_networkInterface->available() > 0)
    {
        uint8_t buffer[_packetParser.getmaxPaketlength()]; 
        size_t bytesRead = _networkInterface->getData(buffer, sizeof(buffer));

        if (bytesRead > 0)
        {
            if (_packetParser.parsePacket(buffer, bytesRead))
            {
                _publicOutput = _packetParser.getPacket();
                _hasPublicOutput = true;
                _errorcode[1] = 0x00;
            }
            else
            {
                _errorcode[1] = _packetParser.get_error();
                _hasPublicOutput = false;

            }
        }else 
        {
            _hasPublicOutput = false;
        }
    }else
    {
        _hasPublicOutput = false;
    }
}

bool CommunicationManager::readPacket(Packet& packetOut, unsigned long reconnectIntervalMs, unsigned long pollDelayMs)
{
    if (!_isInitialized || _networkInterface == nullptr) {
        _errorcode[2] = 0x03; // Noch nicht initialisiert
        return false;
    }

    const unsigned long now = millis();

    // Manager-Fehlercodes:
    // 0x00 = kein Fehler
    // 0x03 = nicht initialisiert
    // 0x04 = reconnect fehlgeschlagen
    // 0x05 = reconnect in Arbeit / Verbindung getrennt
    // 0x06 = ungueltige Poll-Parameter
    if (pollDelayMs == 0) {
        _errorcode[2] = 0x06;
        return false;
    }

    // Non-blocking Reconnect: pro Aufruf nur ein Versuch nach Ablauf des Intervalls.
    if (!_networkInterface->isConnected()) {
        if ((now - _lastReconnectAttemptMs) >= reconnectIntervalMs) {
            _lastReconnectAttemptMs = now;
            if (!_networkInterface->connect()) {
                _errorcode[0] = _networkInterface->read_errorCode();
                _errorcode[2] = 0x04;
                return false;
            }

            _errorcode[0] = 0x00;
            _errorcode[2] = 0x00;
        } else {
            _errorcode[2] = 0x05;
            return false;
        }
    }

    // Non-blocking Poll: nur alle pollDelayMs wirklich lesen/parsen.
    if ((now - _lastPollMs) < pollDelayMs) {
        return false;
    }
    _lastPollMs = now;

    handleIncomingData();

    if (_hasPublicOutput) {
        packetOut = _publicOutput;
        _hasPublicOutput = false;
        _errorcode[2] = 0x00;
        return true;
    }

    // Kein Paket verfuegbar ist kein Kommunikationsfehler.
    if (_errorcode[2] == 0x00 || _errorcode[2] == 0x05) {
        _errorcode[2] = 0x00;
    }
    return false;
}

