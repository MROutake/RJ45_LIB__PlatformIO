#include <CommandHandler.h>

CommandHandler::CommandHandler(RJ45Node& node)
    : _node(node), _handlerCount(0)
{
    for (uint8_t i = 0; i < MAX_HANDLERS; ++i) {
        _handlers[i] = {0, nullptr, false};
    }
}

void CommandHandler::on(uint8_t command, CommandCallback callback)
{
    if (_handlerCount >= MAX_HANDLERS) {
        _node.reportError(ErrorSource::CommandHandler, ERR_CMD_LIMIT_REACHED);
        return;
    }
    _handlers[_handlerCount++] = {command, callback, true};
}

void CommandHandler::dispatch(const Packet& packet)
{
    // Eigene Callbacks haben Vorrang — auch CMD_PING/CMD_KEEPALIVE überschreibbar
    for (uint8_t i = 0; i < _handlerCount; ++i) {
        if (_handlers[i].active && _handlers[i].command == packet.command) {
            _handlers[i].callback(packet);
            return;
        }
    }

    switch (packet.command) {
        case CMD_PING:      sendPong(); break;
        case CMD_KEEPALIVE:             break;
        default:                        break;
    }
}

void CommandHandler::sendPong()
{
    _node.sendPacket(CMD_PONG);
}

uint32_t CommandHandler::extractBitmask(const Packet& packet, uint8_t length)
{
    uint32_t result = 0;
    uint8_t  n      = (length > 4) ? 4 : length;
    for (uint8_t i = 0; i < n && i < packet.length; ++i) {
        result |= (static_cast<uint32_t>(packet.payload[i]) << (i * 8));
    }
    return result;
}

bool CommandHandler::extractIdValue(const Packet& packet, uint8_t& id, uint16_t& value)
{
    if (packet.length < 3) return false;
    id    = packet.payload[0];
    value = (static_cast<uint16_t>(packet.payload[1]) << 8) | packet.payload[2];
    return true;
}

void CommandHandler::sendBitmask(uint8_t command, uint32_t bitmask, uint8_t length)
{
    if (length == 0 || length > 4) {
        _node.reportError(ErrorSource::CommandHandler, ERR_CMD_BAD_LENGTH);
        return;
    }
    uint8_t payload[4];
    for (uint8_t i = 0; i < length; ++i) {
        payload[i] = static_cast<uint8_t>((bitmask >> (i * 8)) & 0xFF);
    }
    _node.sendPacket(command, payload, length);
}

void CommandHandler::sendIdValue(uint8_t command, uint8_t id, uint16_t value)
{
    uint8_t payload[3] = {
        id,
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF)
    };
    _node.sendPacket(command, payload, 3);
}
