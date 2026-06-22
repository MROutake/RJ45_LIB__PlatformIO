#pragma once

#include <Arduino.h>
#include <Parser_H.h>   
#include <RJ45_H.h>


// Aktuell 3 FehlerQuellen - 1. Fehlercode von RJ45 (z.B. Verbindungsfehler, Hardwarefehler, etc.)
// 2. Fehlercode von Parser (z.B. ungültige Eingabe, ungültige Header, ungültige Länge, etc.)   
// 3. Fehlercode von CommunicationManager (z.B. Fehler bei der Verarbeitung von Paketen, etc.)
#define num_erors 3  // 0: RJ45, 1: Parser, 2: CommunicationManager


class CommunicationManager {
    public:
        CommunicationManager(EthernetClient& client);

        //Getter
        set_error(int index, byte errorCode); // Fehlercode setzen, index 0: RJ45, index 1: Parser, index 2: CommunicationManager
        bool begin(byte* mac, IPAddress localIp, IPAddress serverIp, int port, int csPin, const PacketLayout& layout = PacketLayout());
        bool readPacket(Packet& packetOut, unsigned long reconnectIntervalMs = 1000, unsigned long pollDelayMs = 10);
        byte get_error(int index) const; // Fehlercode lesen, index 0: RJ45, index 1: Parser, index 2: CommunicationManager


    private:
        byte _errorcode[num_erors]; // Fehlercode-Array für die Kommunikation
        EthernetClient& _client;
        Packet _internalPacket;
        Parser _packetParser;
        RJ45* _networkInterface;
        Packet _publicOutput;
        bool _hasPublicOutput;
        bool _isInitialized;
        unsigned long _lastReconnectAttemptMs;
        unsigned long _lastPollMs;

        void handleIncomingData(); // Methode zum Verarbeiten eingehender Daten
};