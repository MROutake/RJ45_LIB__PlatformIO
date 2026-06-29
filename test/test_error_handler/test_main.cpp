#include <Arduino.h>
#include <unity.h>
#include <Errors.h>

static bool     g_called = false;
static ErrorSource g_src;
static uint8_t  g_code  = 0;

void onError(ErrorSource src, uint8_t code) {
    g_called = true;
    g_src    = src;
    g_code   = code;
}

void setUp() {
    g_called = false;
    g_code   = 0;
}

// ── Kein Crash ohne registrierten Callback ──────────────────────────────────

void test_no_crash_without_callback() {
    ErrorHandler handler;
    handler.report(ErrorSource::Network, ERR_NET_NO_HARDWARE);
    TEST_ASSERT_FALSE(g_called);
}

// ── Code 0x00 löst nie einen Callback aus ───────────────────────────────────

void test_zero_code_never_triggers_callback() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Network,        0x00);
    handler.report(ErrorSource::Parser,         0x00);
    handler.report(ErrorSource::Manager,        0x00);
    handler.report(ErrorSource::CommandHandler, 0x00);
    TEST_ASSERT_FALSE(g_called);
}

// ── Parser-Fehler sind immer intern (kein Callback) ─────────────────────────

void test_parser_errors_are_internal() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Parser, ERR_PARSE_NULL_INPUT);
    handler.report(ErrorSource::Parser, ERR_PARSE_BAD_HEADER);
    handler.report(ErrorSource::Parser, ERR_PARSE_BAD_LENGTH);
    handler.report(ErrorSource::Parser, ERR_PARSE_BAD_CRC);
    TEST_ASSERT_FALSE(g_called);
}

// ── Netzwerk: intern ─────────────────────────────────────────────────────────

void test_net_disconnected_is_internal() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Network, ERR_NET_DISCONNECTED);
    TEST_ASSERT_FALSE(g_called);
}

// ── Netzwerk: extern ─────────────────────────────────────────────────────────

void test_net_no_hardware_is_external() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Network, ERR_NET_NO_HARDWARE);
    TEST_ASSERT_TRUE(g_called);
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(ErrorSource::Network),
                      static_cast<uint8_t>(g_src));
    TEST_ASSERT_EQUAL_HEX8(ERR_NET_NO_HARDWARE, g_code);
}

void test_net_no_cable_is_external() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Network, ERR_NET_NO_CABLE);
    TEST_ASSERT_TRUE(g_called);
}

void test_net_connect_failed_is_external() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Network, ERR_NET_CONNECT_FAILED);
    TEST_ASSERT_TRUE(g_called);
}

// ── Manager: intern ───────────────────────────────────────────────────────────

void test_manager_reconnecting_is_internal() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Manager, ERR_MGR_RECONNECTING);
    TEST_ASSERT_FALSE(g_called);
}

void test_manager_not_connected_is_internal() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Manager, ERR_MGR_NOT_CONNECTED);
    TEST_ASSERT_FALSE(g_called);
}

// ── Manager: extern ───────────────────────────────────────────────────────────

void test_manager_memory_is_external() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Manager, ERR_MGR_MEMORY);
    TEST_ASSERT_TRUE(g_called);
}

void test_manager_not_init_is_external() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Manager, ERR_MGR_NOT_INIT);
    TEST_ASSERT_TRUE(g_called);
}

void test_manager_reconnect_fail_is_external() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Manager, ERR_MGR_RECONNECT_FAIL);
    TEST_ASSERT_TRUE(g_called);
}

void test_manager_bad_params_is_external() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Manager, ERR_MGR_BAD_PARAMS);
    TEST_ASSERT_TRUE(g_called);
}

void test_manager_send_failed_is_external() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::Manager, ERR_MGR_SEND_FAILED);
    TEST_ASSERT_TRUE(g_called);
}

// ── CommandHandler-Fehler sind immer extern ───────────────────────────────────

void test_command_handler_limit_reached_is_external() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::CommandHandler, ERR_CMD_LIMIT_REACHED);
    TEST_ASSERT_TRUE(g_called);
    TEST_ASSERT_EQUAL_HEX8(ERR_CMD_LIMIT_REACHED, g_code);
}

void test_command_handler_bad_length_is_external() {
    ErrorHandler handler;
    handler.setCallback(onError);
    handler.report(ErrorSource::CommandHandler, ERR_CMD_BAD_LENGTH);
    TEST_ASSERT_TRUE(g_called);
}

void setup() {
    UNITY_BEGIN();

    RUN_TEST(test_no_crash_without_callback);
    RUN_TEST(test_zero_code_never_triggers_callback);
    RUN_TEST(test_parser_errors_are_internal);
    RUN_TEST(test_net_disconnected_is_internal);
    RUN_TEST(test_net_no_hardware_is_external);
    RUN_TEST(test_net_no_cable_is_external);
    RUN_TEST(test_net_connect_failed_is_external);
    RUN_TEST(test_manager_reconnecting_is_internal);
    RUN_TEST(test_manager_not_connected_is_internal);
    RUN_TEST(test_manager_memory_is_external);
    RUN_TEST(test_manager_not_init_is_external);
    RUN_TEST(test_manager_reconnect_fail_is_external);
    RUN_TEST(test_manager_bad_params_is_external);
    RUN_TEST(test_manager_send_failed_is_external);
    RUN_TEST(test_command_handler_limit_reached_is_external);
    RUN_TEST(test_command_handler_bad_length_is_external);

    UNITY_END();
}

void loop() {}
