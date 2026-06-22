#pragma once

#include <Arduino.h>
#include <Ethernet.h>
#include <SPI.h>

class RJ45 {
    
    public:

        RJ45(byte *mac, IPAddress ip, IPAddress server, int port, int _CS_PIN, EthernetClient& client); // Konstruktor
        
       
        bool setup(); // Setup-Methode
        bool connect(); // Verbindungs-Methode
        bool isConnected(); // Verbindungsstatus lesen
        size_t getData(byte* buffer, size_t maxLen); // Bytepaket lesen
        int available(); // Anzahl aktuell verfuegbarer Bytes


        //======================== Getter-Methoden ========================

        byte get_MAC(byte *mac); // MAC-Adresse lesen
        IPAddress get_IPAddress(); // IP-Adresse lesen
        IPAddress get_SERVER_IP(); // Server-IP-Adresse lesen
        int get_HTTP_PORT(); // HTTP-Port lesen
        int get_CS_PIN(); // CS-Pin lesen
        byte read_errorCode(); // Fehlercode lesen

    private:

        // Private Membervariablen
        byte _MAC[6];
        IPAddress _IP_ADDRESS;
        IPAddress _SERVER_IP;
        int _HTTP_PORT;
        int _CS_PIN;
        byte _errorCode;
        EthernetClient& _client;
        

        //======================== Setter-Methoden ========================
        
        void set_mac(byte *mac); // MAC-Adresse setzen
        void set_IPAddress(IPAddress ip); // IP-Adresse setzen
        void set_SERVER_IP(IPAddress server); // Server-IP-Adresse setzen
        void set_HTTP_PORT(int port); // HTTP-Port setzen
        void set_CS_PIN(int cs_pin); // CS-Pin setzen
        void set_error(byte errorCode); // Fehlercode setzen

};

