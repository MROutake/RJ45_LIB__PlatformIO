#include <Parser.h>

Parser::Parser(Packet& packet)
    : _packet(packet), _errorCode(0x00)
{
}

void Parser::setPacketLayout(const PacketLayout& layout)
{
    _layout = layout;
}

void Parser::setError(uint8_t code)
{
    _errorCode = code;
}

uint8_t Parser::getError() const
{
    return _errorCode;
}

const Packet& Parser::getPacket() const
{
    return _packet;
}

size_t Parser::getPayloadLength() const
{
    return _packet.length;
}

size_t Parser::maxPacketLength() const
{
    // 5 Overhead-Bytes: header1, header2, length, command, crc
    return MAX_PAYLOAD + 5;
}

uint8_t Parser::calculateCrc(const uint8_t* data, size_t length) const
{
    // Einfache Additionsprüfsumme — kein Carry, Überlauf ist gewollt (uint8_t)
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc = static_cast<uint8_t>(crc + data[i]);
    }
    return crc;
}

bool Parser::parsePacket(const uint8_t* data, size_t length)
{
    if (data == nullptr) {
        setError(ERR_PARSE_NULL_INPUT);
        return false;
    }
    // Mindestlänge: header1 + header2 + length + command + crc = 5 Bytes
    if (length < 5) {
        setError(ERR_PARSE_BAD_LENGTH);
        return false;
    }
    if (data[0] != _layout.header1 || data[1] != _layout.header2) {
        setError(ERR_PARSE_BAD_HEADER);
        return false;
    }

    const uint8_t payloadLength  = data[2];
    const size_t  expectedLength = static_cast<size_t>(payloadLength) + 5;

    if (payloadLength > MAX_PAYLOAD || length < expectedLength) {
        setError(ERR_PARSE_BAD_LENGTH);
        return false;
    }

    // CRC-Prüfung über alle Bytes außer dem CRC selbst
    const uint8_t receivedCrc   = data[expectedLength - 1];
    const uint8_t calculatedCrc = calculateCrc(data, expectedLength - 1);

    if (receivedCrc != calculatedCrc) {
        setError(ERR_PARSE_BAD_CRC);
        return false;
    }

    _packet.header1 = data[0];
    _packet.header2 = data[1];
    _packet.length  = payloadLength;
    _packet.command = data[3];   // Byte-Index 3: nach header1, header2, length
    _packet.crc     = receivedCrc;

    for (size_t i = 0; i < payloadLength; ++i)          _packet.payload[i] = data[4 + i];
    for (size_t i = payloadLength; i < MAX_PAYLOAD; ++i) _packet.payload[i] = 0;

    setError(0x00);
    return true;
}
