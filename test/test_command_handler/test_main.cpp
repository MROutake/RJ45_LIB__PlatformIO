#include <Arduino.h>
#include <unity.h>
#include <Ethernet.h>
#include <CommandHandler.h>

// Hilfsfunktion: vollständiges Paket mit gegebenen Feldern bauen
static Packet makePacket(uint8_t command, const uint8_t* payload, uint8_t payloadLen) {
    Packet p = {};
    p.header1  = 0xAA;
    p.header2  = 0xAA;
    p.length   = payloadLen;
    p.command  = command;
    if (payload) {
        for (uint8_t i = 0; i < payloadLen && i < MAX_PAYLOAD; ++i) {
            p.payload[i] = payload[i];
        }
    }
    return p;
}

// ── extractBitmask ────────────────────────────────────────────────────────────

void test_extractBitmask_1byte() {
    uint8_t raw[1] = {0xAB};
    Packet p = makePacket(CMD_RX_BITMASK, raw, 1);
    TEST_ASSERT_EQUAL_UINT32(0xABUL, CommandHandler::extractBitmask(p, 1));
}

void test_extractBitmask_2bytes() {
    uint8_t raw[2] = {0x34, 0x12};
    Packet p = makePacket(CMD_RX_BITMASK, raw, 2);
    TEST_ASSERT_EQUAL_UINT32(0x1234UL, CommandHandler::extractBitmask(p, 2));
}

void test_extractBitmask_4bytes() {
    uint8_t raw[4] = {0x78, 0x56, 0x34, 0x12};
    Packet p = makePacket(CMD_RX_BITMASK, raw, 4);
    TEST_ASSERT_EQUAL_UINT32(0x12345678UL, CommandHandler::extractBitmask(p, 4));
}

void test_extractBitmask_length_clamped_to_4() {
    // length > 4 soll auf 4 begrenzt werden
    uint8_t raw[4] = {0x01, 0x02, 0x03, 0x04};
    Packet p = makePacket(CMD_RX_BITMASK, raw, 4);
    uint32_t result4 = CommandHandler::extractBitmask(p, 4);
    uint32_t result9 = CommandHandler::extractBitmask(p, 9);
    TEST_ASSERT_EQUAL_UINT32(result4, result9);
}

void test_extractBitmask_respects_packet_length() {
    // Wenn packet.length < angeforderte Bytes → nur die vorhandenen Bytes lesen
    uint8_t raw[2] = {0xFF, 0x01};
    Packet p = makePacket(CMD_RX_BITMASK, raw, 2);
    // 4 Bytes anfordern, Paket hat nur 2
    uint32_t result = CommandHandler::extractBitmask(p, 4);
    TEST_ASSERT_EQUAL_UINT32(0x01FFUL, result);
}

// ── extractIdValue ─────────────────────────────────────────────────────────────

void test_extractIdValue_valid() {
    uint8_t raw[3] = {0x07, 0x01, 0xF4};  // id=7, value=500 (0x01F4)
    Packet p = makePacket(CMD_TX_ID_VALUE, raw, 3);

    uint8_t  id    = 0;
    uint16_t value = 0;
    TEST_ASSERT_TRUE(CommandHandler::extractIdValue(p, id, value));
    TEST_ASSERT_EQUAL_UINT8(0x07, id);
    TEST_ASSERT_EQUAL_UINT16(500, value);
}

void test_extractIdValue_too_short() {
    uint8_t raw[2] = {0x01, 0x02};
    Packet p = makePacket(CMD_TX_ID_VALUE, raw, 2);

    uint8_t  id    = 0;
    uint16_t value = 0;
    TEST_ASSERT_FALSE(CommandHandler::extractIdValue(p, id, value));
}

// ── dispatch: Callback-Routing ────────────────────────────────────────────────

static bool g_callbackCalled = false;
static uint8_t g_callbackCommand = 0;

void dummyCallback(const Packet& p) {
    g_callbackCalled  = true;
    g_callbackCommand = p.command;
}

void setUp() {
    g_callbackCalled  = false;
    g_callbackCommand = 0;
}

void test_dispatch_calls_registered_callback() {
    EthernetClient client;
    RJ45Node node(client);
    CommandHandler handler(node);
    handler.on(CMD_RX_BITMASK, dummyCallback);

    Packet p = makePacket(CMD_RX_BITMASK, nullptr, 0);
    handler.dispatch(p);

    TEST_ASSERT_TRUE(g_callbackCalled);
    TEST_ASSERT_EQUAL_UINT8(CMD_RX_BITMASK, g_callbackCommand);
}

void test_dispatch_unregistered_command_no_crash() {
    EthernetClient client;
    RJ45Node node(client);
    CommandHandler handler(node);

    // 0x42 hat keinen Handler und ist kein CMD_PING/CMD_KEEPALIVE
    Packet p = makePacket(0x42, nullptr, 0);
    handler.dispatch(p);  // muss ohne Absturz durchlaufen
    TEST_PASS();
}

void test_dispatch_ping_without_override_no_crash() {
    // CMD_PING ohne eigenen Handler ruft sendPong() auf.
    // Node ist nicht initialisiert → sendPacket() schlägt fehl aber crasht nicht.
    EthernetClient client;
    RJ45Node node(client);
    CommandHandler handler(node);

    Packet p = makePacket(CMD_PING, nullptr, 0);
    handler.dispatch(p);
    TEST_PASS();
}

void test_dispatch_keepalive_no_crash() {
    EthernetClient client;
    RJ45Node node(client);
    CommandHandler handler(node);

    Packet p = makePacket(CMD_KEEPALIVE, nullptr, 0);
    handler.dispatch(p);
    TEST_PASS();
}

void test_dispatch_custom_overrides_ping() {
    // Eigener Callback auf CMD_PING muss Vorrang vor dem eingebauten sendPong() haben
    EthernetClient client;
    RJ45Node node(client);
    CommandHandler handler(node);
    handler.on(CMD_PING, dummyCallback);

    Packet p = makePacket(CMD_PING, nullptr, 0);
    handler.dispatch(p);

    TEST_ASSERT_TRUE(g_callbackCalled);
}

void test_dispatch_first_matching_handler_wins() {
    // Zwei verschiedene Befehle, sicherstellen dass nur der passende aufgerufen wird
    EthernetClient client;
    RJ45Node node(client);
    CommandHandler handler(node);
    handler.on(CMD_RX_BITMASK, dummyCallback);

    Packet p = makePacket(CMD_KEEPALIVE, nullptr, 0);
    handler.dispatch(p);

    TEST_ASSERT_FALSE(g_callbackCalled);
}

// ── on(): Limit ───────────────────────────────────────────────────────────────

static bool     g_limitErrorReceived = false;
static uint8_t  g_limitErrorCode     = 0;

void onLimitError(ErrorSource src, uint8_t code) {
    if (src == ErrorSource::CommandHandler && code == ERR_CMD_LIMIT_REACHED) {
        g_limitErrorReceived = true;
        g_limitErrorCode     = code;
    }
}

void test_on_limit_reached_triggers_error() {
    g_limitErrorReceived = false;

    EthernetClient client;
    RJ45Node node(client);
    node.onError(onLimitError);
    CommandHandler handler(node);

    // MAX_HANDLERS (8) Registrierungen — alle sollen erfolgreich sein
    for (uint8_t i = 0; i < MAX_HANDLERS; ++i) {
        handler.on(static_cast<uint8_t>(i + 0x20), dummyCallback);
    }
    TEST_ASSERT_FALSE(g_limitErrorReceived);

    // Eine Registrierung zu viel → ERR_CMD_LIMIT_REACHED
    handler.on(0xFF, dummyCallback);
    TEST_ASSERT_TRUE(g_limitErrorReceived);
    TEST_ASSERT_EQUAL_HEX8(ERR_CMD_LIMIT_REACHED, g_limitErrorCode);
}

// ── sendBitmask: Fehler bei ungültiger Länge ──────────────────────────────────

static bool    g_badLenErrorReceived = false;

void onBadLenError(ErrorSource src, uint8_t code) {
    if (src == ErrorSource::CommandHandler && code == ERR_CMD_BAD_LENGTH) {
        g_badLenErrorReceived = true;
    }
}

void test_sendBitmask_length_zero_triggers_error() {
    g_badLenErrorReceived = false;

    EthernetClient client;
    RJ45Node node(client);
    node.onError(onBadLenError);
    CommandHandler handler(node);

    handler.sendBitmask(CMD_TX_BITMASK, 0xFF, 0);  // length=0 → ungültig
    TEST_ASSERT_TRUE(g_badLenErrorReceived);
}

void test_sendBitmask_length_over_4_triggers_error() {
    g_badLenErrorReceived = false;

    EthernetClient client;
    RJ45Node node(client);
    node.onError(onBadLenError);
    CommandHandler handler(node);

    handler.sendBitmask(CMD_TX_BITMASK, 0xFF, 5);  // length=5 → ungültig
    TEST_ASSERT_TRUE(g_badLenErrorReceived);
}

void setup() {
    UNITY_BEGIN();

    RUN_TEST(test_extractBitmask_1byte);
    RUN_TEST(test_extractBitmask_2bytes);
    RUN_TEST(test_extractBitmask_4bytes);
    RUN_TEST(test_extractBitmask_length_clamped_to_4);
    RUN_TEST(test_extractBitmask_respects_packet_length);

    RUN_TEST(test_extractIdValue_valid);
    RUN_TEST(test_extractIdValue_too_short);

    RUN_TEST(test_dispatch_calls_registered_callback);
    RUN_TEST(test_dispatch_unregistered_command_no_crash);
    RUN_TEST(test_dispatch_ping_without_override_no_crash);
    RUN_TEST(test_dispatch_keepalive_no_crash);
    RUN_TEST(test_dispatch_custom_overrides_ping);
    RUN_TEST(test_dispatch_first_matching_handler_wins);

    RUN_TEST(test_on_limit_reached_triggers_error);

    RUN_TEST(test_sendBitmask_length_zero_triggers_error);
    RUN_TEST(test_sendBitmask_length_over_4_triggers_error);

    UNITY_END();
}

void loop() {}
