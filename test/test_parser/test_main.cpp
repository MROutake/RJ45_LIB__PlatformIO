#include <Arduino.h>
#include <unity.h>
#include <Parser_H.h>

namespace {
uint8_t calcCrc(const uint8_t* data, size_t length) {
  uint8_t crc = 0;
  for (size_t i = 0; i < length; ++i) {
    crc = static_cast<uint8_t>(crc + data[i]);
  }
  return crc;
}
}

void test_parse_valid_packet() {
  Packet packet = {};
  Parser parser(packet);

  const uint8_t payloadLength = 3;
  uint8_t data[8] = {0xAA, 0xAA, payloadLength, 0x10, 0x01, 0x02, 0x03, 0x00};
  data[7] = calcCrc(data, 7);

  bool ok = parser.parsePacket(data, sizeof(data));
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_HEX8(0x00, parser.get_error());
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

  bool ok = parser.parsePacket(data, sizeof(data));
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_HEX8(0x02, parser.get_error());
}

void test_parse_invalid_length_too_short() {
  Packet packet = {};
  Parser parser(packet);

  uint8_t data[4] = {0xAA, 0xAA, 0x00, 0x10};

  bool ok = parser.parsePacket(data, sizeof(data));
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_HEX8(0x03, parser.get_error());
}

void test_parse_invalid_crc() {
  Packet packet = {};
  Parser parser(packet);

  uint8_t data[6] = {0xAA, 0xAA, 0x01, 0x22, 0x99, 0x00};
  data[5] = static_cast<uint8_t>(calcCrc(data, 5) + 1);

  bool ok = parser.parsePacket(data, sizeof(data));
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_HEX8(0x04, parser.get_error());
}

void test_parse_null_data() {
  Packet packet = {};
  Parser parser(packet);

  bool ok = parser.parsePacket(nullptr, 0);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_HEX8(0x01, parser.get_error());
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

  bool ok = parser.parsePacket(data, sizeof(data));
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_HEX8(0x00, parser.get_error());
  TEST_ASSERT_EQUAL_UINT8(0x33, packet.command);
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_valid_packet);
  RUN_TEST(test_parse_invalid_header);
  RUN_TEST(test_parse_invalid_length_too_short);
  RUN_TEST(test_parse_invalid_crc);
  RUN_TEST(test_parse_null_data);
  RUN_TEST(test_parse_custom_layout);
  UNITY_END();
}

void loop() {
}
