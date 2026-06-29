#include <Arduino.h>
#include <unity.h>
#include <Parser.h>

namespace {
uint8_t calcCrc(const uint8_t* data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) crc = static_cast<uint8_t>(crc + data[i]);
    return crc;
}
}

// ── Bestehende Tests ────────────────────────────────────────────────────────

void test_parse_valid_packet() {
    Packet packet = {};
    Parser parser(packet);

    const uint8_t payloadLength = 3;
    uint8_t data[8] = {0xAA, 0xAA, payloadLength, 0x10, 0x01, 0x02, 0x03, 0x00};
    data[7] = calcCrc(data, 7);

    TEST_ASSERT_TRUE(parser.parsePacket(data, sizeof(data)));
    TEST_ASSERT_EQUAL_HEX8(0x00, parser.getError());
    TEST_ASSERT_EQUAL_UINT8(0xAA, packet.header1);
    TEST_ASSERT_EQUAL_UINT8(0xAA, packet.header2);
    TEST_ASSERT_EQUAL_UINT8(payloadLength, packet.length);
    TEST_ASSERT_EQUAL_UINT8(0x10, packet.command);
    TEST_ASSERT_EQUAL_UINT8(0x01, packet.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(0x02, packet.payload[1]);
    TEST_ASSERT_EQUAL_UINT8(0x03, packet.payload[2]);
    TEST_ASSERT_EQUAL_UINT8(data[7], packet.crc);
}

void test_parse_invalid_header() {
    Packet packet = {};
    Parser parser(packet);

    uint8_t data[5] = {0xAB, 0xAA, 0x00, 0x10, 0x00};
    data[4] = calcCrc(data, 4);

    TEST_ASSERT_FALSE(parser.parsePacket(data, sizeof(data)));
    TEST_ASSERT_EQUAL_HEX8(ERR_PARSE_BAD_HEADER, parser.getError());
}

void test_parse_invalid_length_too_short() {
    Packet packet = {};
    Parser parser(packet);

    uint8_t data[4] = {0xAA, 0xAA, 0x00, 0x10};

    TEST_ASSERT_FALSE(parser.parsePacket(data, sizeof(data)));
    TEST_ASSERT_EQUAL_HEX8(ERR_PARSE_BAD_LENGTH, parser.getError());
}

void test_parse_invalid_crc() {
    Packet packet = {};
    Parser parser(packet);

    uint8_t data[6] = {0xAA, 0xAA, 0x01, 0x22, 0x99, 0x00};
    data[5] = static_cast<uint8_t>(calcCrc(data, 5) + 1);

    TEST_ASSERT_FALSE(parser.parsePacket(data, sizeof(data)));
    TEST_ASSERT_EQUAL_HEX8(ERR_PARSE_BAD_CRC, parser.getError());
}

void test_parse_null_data() {
    Packet packet = {};
    Parser parser(packet);

    TEST_ASSERT_FALSE(parser.parsePacket(nullptr, 0));
    TEST_ASSERT_EQUAL_HEX8(ERR_PARSE_NULL_INPUT, parser.getError());
}

void test_parse_custom_layout() {
    Packet packet = {};
    Parser parser(packet);

    PacketLayout layout;
    layout.header1 = 0x55;
    layout.header2 = 0xCC;
    parser.setPacketLayout(layout);

    uint8_t data[5] = {0x55, 0xCC, 0x00, 0x33, 0x00};
    data[4] = calcCrc(data, 4);

    TEST_ASSERT_TRUE(parser.parsePacket(data, sizeof(data)));
    TEST_ASSERT_EQUAL_HEX8(0x00, parser.getError());
    TEST_ASSERT_EQUAL_UINT8(0x33, packet.command);
}

// ── Neue Tests ──────────────────────────────────────────────────────────────

void test_parse_zero_payload() {
    // Minimales gültiges Paket: 5 Bytes, kein Payload
    Packet packet = {};
    Parser parser(packet);

    uint8_t data[5] = {0xAA, 0xAA, 0x00, 0x01, 0x00};
    data[4] = calcCrc(data, 4);

    TEST_ASSERT_TRUE(parser.parsePacket(data, sizeof(data)));
    TEST_ASSERT_EQUAL_HEX8(0x00, parser.getError());
    TEST_ASSERT_EQUAL_UINT8(0x00, packet.length);
    TEST_ASSERT_EQUAL_UINT8(0x01, packet.command);
}

void test_parse_max_payload() {
    // Exakt MAX_PAYLOAD (32) Bytes Payload — Grenzfall
    Packet packet = {};
    Parser parser(packet);

    const uint8_t payloadLen = MAX_PAYLOAD;
    uint8_t data[MAX_PAYLOAD + 5];
    data[0] = 0xAA;
    data[1] = 0xAA;
    data[2] = payloadLen;
    data[3] = 0x10;
    for (uint8_t i = 0; i < payloadLen; ++i) data[4 + i] = i;
    data[4 + payloadLen] = calcCrc(data, 4 + payloadLen);

    TEST_ASSERT_TRUE(parser.parsePacket(data, sizeof(data)));
    TEST_ASSERT_EQUAL_HEX8(0x00, parser.getError());
    TEST_ASSERT_EQUAL_UINT8(payloadLen, packet.length);
    for (uint8_t i = 0; i < payloadLen; ++i) {
        TEST_ASSERT_EQUAL_UINT8(i, packet.payload[i]);
    }
}

void test_parse_payload_oversize() {
    // length-Feld > MAX_PAYLOAD → ERR_PARSE_BAD_LENGTH
    Packet packet = {};
    Parser parser(packet);

    uint8_t data[6] = {0xAA, 0xAA, MAX_PAYLOAD + 1, 0x10, 0x00, 0x00};
    data[5] = calcCrc(data, 5);

    TEST_ASSERT_FALSE(parser.parsePacket(data, sizeof(data)));
    TEST_ASSERT_EQUAL_HEX8(ERR_PARSE_BAD_LENGTH, parser.getError());
}

void test_parse_leftover_payload_cleared() {
    // Payload-Rest aus vorherigem Parse muss auf 0 gesetzt werden
    Packet packet = {};
    Parser parser(packet);

    // Erster Parse: 3 Payload-Bytes
    {
        uint8_t d[8] = {0xAA, 0xAA, 0x03, 0x10, 0xAA, 0xBB, 0xCC, 0x00};
        d[7] = calcCrc(d, 7);
        TEST_ASSERT_TRUE(parser.parsePacket(d, sizeof(d)));
    }

    // Zweiter Parse: nur 1 Payload-Byte
    {
        uint8_t d[6] = {0xAA, 0xAA, 0x01, 0x10, 0x42, 0x00};
        d[5] = calcCrc(d, 5);
        TEST_ASSERT_TRUE(parser.parsePacket(d, sizeof(d)));
    }

    TEST_ASSERT_EQUAL_UINT8(0x01, packet.length);
    TEST_ASSERT_EQUAL_UINT8(0x42, packet.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, packet.payload[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00, packet.payload[2]);
}

void test_parse_error_cleared_on_success() {
    // Nach einem Fehler-Parse muss ein gültiger Parse getError() auf 0x00 setzen
    Packet packet = {};
    Parser parser(packet);

    // Fehler auslösen
    uint8_t bad[5] = {0xFF, 0xFF, 0x00, 0x01, 0x00};
    TEST_ASSERT_FALSE(parser.parsePacket(bad, sizeof(bad)));
    TEST_ASSERT_NOT_EQUAL(0x00, parser.getError());

    // Gültiger Parse
    uint8_t good[5] = {0xAA, 0xAA, 0x00, 0x01, 0x00};
    good[4] = calcCrc(good, 4);
    TEST_ASSERT_TRUE(parser.parsePacket(good, sizeof(good)));
    TEST_ASSERT_EQUAL_HEX8(0x00, parser.getError());
}

void test_getPayloadLength() {
    Packet packet = {};
    Parser parser(packet);

    const uint8_t payloadLen = 2;
    uint8_t data[7] = {0xAA, 0xAA, payloadLen, 0x10, 0x11, 0x22, 0x00};
    data[6] = calcCrc(data, 6);

    TEST_ASSERT_TRUE(parser.parsePacket(data, sizeof(data)));
    TEST_ASSERT_EQUAL_UINT8(payloadLen, parser.getPayloadLength());
}

void test_maxPacketLength() {
    Packet packet = {};
    Parser parser(packet);
    // 5 Overhead + MAX_PAYLOAD (32) = 37
    TEST_ASSERT_EQUAL_UINT8(MAX_PAYLOAD + 5, parser.maxPacketLength());
}

void setup() {
    UNITY_BEGIN();

    RUN_TEST(test_parse_valid_packet);
    RUN_TEST(test_parse_invalid_header);
    RUN_TEST(test_parse_invalid_length_too_short);
    RUN_TEST(test_parse_invalid_crc);
    RUN_TEST(test_parse_null_data);
    RUN_TEST(test_parse_custom_layout);

    RUN_TEST(test_parse_zero_payload);
    RUN_TEST(test_parse_max_payload);
    RUN_TEST(test_parse_payload_oversize);
    RUN_TEST(test_parse_leftover_payload_cleared);
    RUN_TEST(test_parse_error_cleared_on_success);
    RUN_TEST(test_getPayloadLength);
    RUN_TEST(test_maxPacketLength);

    UNITY_END();
}

void loop() {}
