#pragma once

#include <Arduino.h>
#include <Packet_H.h>



class Parser {
    public:
    Parser(Packet& packet);

    bool parsePacket(const uint8_t* data, size_t length);
    void setPacketLayout(const PacketLayout& layout);
    byte get_error(); // Fehlercode lesen
    size_t getmaxPaketlength();  // Maximale Payload-Größe zurückgeben
    size_t get_PayloadLength(); // Aktuelle Payload-Länge zurückgeben
    const Packet& getPacket() const; // Letztes erfolgreich geparstes Paket

    private:

    Packet& _packet;
    byte _errorCode;
    PacketLayout _layout;

    uint8_t calculateCrc(const uint8_t* data, size_t length) const;

    void set_error(byte errorCode);

};


