#include <Errors.h>

ErrorHandler::ErrorHandler() : _callback(nullptr) {}

void ErrorHandler::setCallback(ErrorCallback cb)
{
    _callback = cb;
}

void ErrorHandler::report(ErrorSource source, uint8_t code)
{
    if (code == 0x00 || _callback == nullptr) return;
    if (isExternal(source, code)) {
        _callback(source, code);
    }
}

bool ErrorHandler::isExternal(ErrorSource source, uint8_t code)
{
    switch (source) {
        case ErrorSource::Network:
            // ERR_NET_DISCONNECTED (0x04) ist intern — RJ45Node reconnectet automatisch.
            // Alle anderen Network-Fehler sind fatal oder erfordern manuelle Intervention.
            return code != ERR_NET_DISCONNECTED;

        case ErrorSource::Parser:
            // Parser-Fehler sind immer intern — das Paket wird verworfen
            // und die Lib läuft weiter. Keine Nutzeraktion erforderlich.
            return false;

        case ErrorSource::Manager:
            // ERR_MGR_RECONNECTING (0x05): normaler Betrieb zwischen Reconnect-Versuchen.
            // ERR_MGR_NOT_CONNECTED (0x07): send() während Reconnect läuft — temporär.
            return (code != ERR_MGR_RECONNECTING && code != ERR_MGR_NOT_CONNECTED);

        case ErrorSource::CommandHandler:
            // Alle CommandHandler-Fehler sind Programmierfehler → immer extern.
            return true;

        default:
            return true;
    }
}
