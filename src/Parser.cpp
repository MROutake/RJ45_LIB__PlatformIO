#include <Parser_H.h>

//======================== Setter-Methoden ========================

void Parser::setPacketLayout(const PacketLayout& layout)
{
    _layout = layout;
}

void Parser::set_error(byte errorCode)
{
    _errorCode = errorCode;
}

byte Parser::get_error()
{
    return _errorCode;
}


//======================== Getter-Methoden ========================


size_t Parser::getmaxPaketlength()
{
    return MAX_PAYLOAD + 5; // 5 Bytes für Header, Länge, Befehl und CRC
}

size_t Parser::get_PayloadLength()
{
    return _packet.length;
}

const Packet& Parser::getPacket() const
{
    return _packet;
}




// ======================== Konstruktor ========================

Parser::Parser(Packet &packet)
    : _packet(packet)
{

    set_error(0x00); // Kein Fehler
}


// ======================== Hauptmethoden zum Parsen von Paketen ========================

uint8_t Parser::calculateCrc(const uint8_t *data, size_t length) const
{
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i)
    {
        crc = static_cast<uint8_t>(crc + data[i]);
    }
    return crc;
}

bool Parser::parsePacket(const uint8_t *data, size_t length)
{

    if (data == nullptr)
    {
        set_error(0x01); // Fehlercode fuer ungültige Eingabe
        return false;
    }

    if (length < 5)
    {
        set_error(0x03); // Fehlercode fuer ungueltige Laenge
        return false;
    }

    if (data[0] != _layout.header1 || data[1] != _layout.header2)
    {
        set_error(0x02); // Fehlercode fuer ungültige Header
        return false;
    }

    const uint8_t payloadLength = data[2];
    const size_t expectedLength = static_cast<size_t>(payloadLength) + 5;

    if (payloadLength > MAX_PAYLOAD || length < expectedLength)
    {
        set_error(0x03); // Fehlercode fuer ungültige Länge
        return false;
    }

    const uint8_t receivedCrc = data[expectedLength - 1];
    const uint8_t calculatedCrc = calculateCrc(data, expectedLength - 1);

    if (receivedCrc != calculatedCrc)
    {
        set_error(0x04); // Fehlercode fuer ungültige CRC
        return false;
    }

    _packet.header1 = data[0];
    _packet.header2 = data[1];
    _packet.length = payloadLength;
    _packet.command = data[3];

    for (size_t i = 0; i < payloadLength; ++i)
    {
        _packet.payload[i] = data[4 + i];
    }

    for (size_t i = payloadLength; i < MAX_PAYLOAD; ++i)
    {
        _packet.payload[i] = 0;
    }

    _packet.crc = receivedCrc;
    set_error(0x00);
    return true;
}


