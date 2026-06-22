#include <Arduino.h>
#include "RJ45_H.h"

RJ45::RJ45(byte *mac, IPAddress ip, IPAddress server, int port, int CS_PIN, EthernetClient &client)
    : _client(client)
{

  // Konstruktor-Implementierung
  set_mac(mac);
  set_IPAddress(ip);
  set_SERVER_IP(server);
  set_HTTP_PORT(port);
  set_CS_PIN(CS_PIN);
  set_error(0x00); // Kein Fehler
}

bool RJ45::connect()
{

  // Verbindungs-Implementierung
  if (_client.connect(get_SERVER_IP(), get_HTTP_PORT()))
  {
    return true;
  }
  else
  {
    set_error(0x03); // Fehlercode für Verbindungsfehler
    return false;
  }
}

bool RJ45::isConnected()
{
  return _client.connected();
}

bool RJ45::setup()
{

  // Setup-Implementierung
  Ethernet.init(get_CS_PIN()); // CS-Pin für Ethernet-Shield initialisieren
  Ethernet.begin(_MAC, get_IPAddress());

  // Prüfen nach Angeschlossener Hardware
  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    set_error(0x01); // Fehlercode für fehlende Hardware
    return false;
  }
  // Prüffen nach Kabelverbindung
  if (Ethernet.linkStatus() == LinkOFF)
  {
    set_error(0x02); // Fehlercode für fehlende Kabelverbindung
    return false;
  }

  return connect(); // Verbindung zum Server herstellen
}

size_t RJ45::getData(byte *buffer, size_t maxLen)
{
  // Liest bis zu maxLen Bytes aus dem TCP-Stream in den uebergebenen Buffer.
  if (buffer == nullptr || maxLen == 0)
  {
    return 0;
  }

  size_t bytesRead = 0;
  while (_client.available() && bytesRead < maxLen)
  {
    buffer[bytesRead++] = static_cast<byte>(_client.read());
  }

  if (!_client.connected() && bytesRead == 0)
  {
    set_error(0x04); // Fehlercode fuer getrennte Verbindung waehrend Read.
  }

  return bytesRead;
}

int RJ45::available()
{
  return _client.available();
}

//======================== Setter-Methoden ========================

void RJ45::set_mac(byte *mac)
{
  memcpy(_MAC, mac, 6);
}

void RJ45::set_error(byte errorCode)
{
  _errorCode = errorCode;
}
void RJ45::set_IPAddress(IPAddress ip)
{
  _IP_ADDRESS = ip;
}
void RJ45::set_SERVER_IP(IPAddress server)
{
  _SERVER_IP = server;
}
void RJ45::set_HTTP_PORT(int port)
{
  _HTTP_PORT = port;
}
void RJ45::set_CS_PIN(int cs_pin)
{
  _CS_PIN = cs_pin;
}

//======================== Getter-Methoden ========================

byte RJ45::get_MAC(byte *mac)
{
  memcpy(mac, _MAC, 6);
  return 6;
}
IPAddress RJ45::get_IPAddress()
{
  return _IP_ADDRESS;
}
IPAddress RJ45::get_SERVER_IP()
{
  return _SERVER_IP;
}
int RJ45::get_HTTP_PORT()
{
  return _HTTP_PORT;
}
int RJ45::get_CS_PIN()
{
  return _CS_PIN;
}
byte RJ45::read_errorCode()
{
  return _errorCode;
}
