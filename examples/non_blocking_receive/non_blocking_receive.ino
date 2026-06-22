#include <Arduino.h>
#include <Ethernet.h>
#include <CommunicationManager_H.h>

EthernetClient client;
CommunicationManager com(client);

byte mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress localIp(192, 168, 0, 50);
IPAddress serverIp(192, 168, 0, 10);

void setup() {
  Serial.begin(115200);

  PacketLayout layout;
  layout.header1 = 0xAA;
  layout.header2 = 0xAA;

  if (!com.begin(mac, localIp, serverIp, 5000, 10, layout)) {
    Serial.print("Begin Fehler RJ45=");
    Serial.print(com.get_error(0), HEX);
    Serial.print(" MGR=");
    Serial.println(com.get_error(2), HEX);
  }
}

void loop() {
  Packet packet;

  if (com.readPacket(packet, 1000, 10)) {
    Serial.print("CMD: ");
    Serial.println(packet.command, HEX);

    Serial.print("LEN: ");
    Serial.println(packet.length);

    Serial.print("PAYLOAD: ");
    for (size_t i = 0; i < packet.length; ++i) {
      Serial.print(packet.payload[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
  }

  const byte errRj45 = com.get_error(0);
  const byte errParser = com.get_error(1);
  const byte errMgr = com.get_error(2);
  if (errRj45 != 0x00 || errParser != 0x00 || errMgr != 0x00) {
    Serial.print("ERR RJ45=");
    Serial.print(errRj45, HEX);
    Serial.print(" PARSER=");
    Serial.print(errParser, HEX);
    Serial.print(" MGR=");
    Serial.println(errMgr, HEX);
  }
}
