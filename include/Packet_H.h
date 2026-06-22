#pragma once

#include <Arduino.h>

#define MAX_PAYLOAD 32

struct Packet {
    uint8_t header1;        
    uint8_t header2;        
    uint8_t length;         // Länge der Nutzdaten
    uint8_t command;        // Befehl
    uint8_t payload[MAX_PAYLOAD];
    uint8_t crc;            // einfache Prüfsumme
};

struct PacketLayout {
    uint8_t header1 = 0xAA;
    uint8_t header2 = 0xAA;
};
