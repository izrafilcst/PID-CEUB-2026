#include <unity.h>
#include <cmath>
#include <cstring>
#include "comm/CommandProtocol.h"
#include "comm/CommandQueue.h"
#include "comm/Telemetry.h"
#include "config/RobotConfig.h"

void setUp() {}
void tearDown() {}

static Command parse(const char* json) {
    Command c;
    parseCommand(json, c);
    return c;
}

void test_command_protocol_parses_set() {
    Command c;
    TEST_ASSERT_TRUE(parseCommand("{\"t\":\"set\",\"k\":\"kp\",\"v\":1.25}", c));
    TEST_ASSERT_EQUAL(CmdType::SET, c.type);
    TEST_ASSERT_EQUAL_UINT8(1, c.count);
    TEST_ASSERT_EQUAL_STRING("kp", c.items[0].key);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.25f, c.items[0].value);
}

// Review Focus 1
void test_command_protocol_set_without_value_fails() {
    Command c;
    TEST_ASSERT_FALSE(parseCommand("{\"t\":\"set\",\"k\":\"kp\"}", c));
    TEST_ASSERT_EQUAL(CmdType::NONE, c.type);
    TEST_ASSERT_FALSE(parseCommand("{\"t\":\"set\",\"k\":\"kp\",\"v\":\"abc\"}", c));
    TEST_ASSERT_FALSE(parseCommand("{\"t\":\"set\",\"k\":\"muitolonga\",\"v\":1}", c));
    TEST_ASSERT_FALSE(parseCommand("{\"t\":\"se", c));
    TEST_ASSERT_FALSE(parseCommand("", c));
    TEST_ASSERT_FALSE(parseCommand(nullptr, c));
}

void test_command_protocol_set_accepts_nan_for_config_to_reject() {
    const Command c = parse("{\"t\":\"set\",\"k\":\"kp\",\"v\":nan}");
    TEST_ASSERT_EQUAL(CmdType::SET, c.type);
    TEST_ASSERT_TRUE(std::isnan(c.items[0].value));
}

void test_command_protocol_simple_commands() {
    TEST_ASSERT_EQUAL(CmdType::GET, parse("{\"t\":\"get\"}").type);
    TEST_ASSERT_EQUAL(CmdType::SAVE, parse("{\"t\":\"save\"}").type);
    TEST_ASSERT_EQUAL(CmdType::LOAD, parse("{\"t\":\"load\"}").type);
    TEST_ASSERT_EQUAL(CmdType::DEFAULTS, parse("{\"t\":\"defaults\"}").type);
    TEST_ASSERT_EQUAL(CmdType::SENSOR_CAL, parse("{\"t\":\"scal\"}").type);
    TEST_ASSERT_EQUAL(CmdType::START, parse("{\"t\":\"start\"}").type);
    TEST_ASSERT_EQUAL(CmdType::STOP, parse("{\"t\":\"stop\"}").type);
    TEST_ASSERT_EQUAL(CmdType::RESET, parse("{\"t\":\"reset\"}").type);
    TEST_ASSERT_EQUAL(CmdType::NONE, parse("{\"t\":\"dance\"}").type);
}

void test_command_protocol_profile_switch() {
    const Command c = parse("{\"t\":\"prof\",\"n\":2,\"f\":1}");
    TEST_ASSERT_EQUAL(CmdType::PROFILE, c.type);
    TEST_ASSERT_EQUAL_UINT8(2, c.profile);
    TEST_ASSERT_TRUE(c.force);
    TEST_ASSERT_FALSE(parse("{\"t\":\"prof\",\"n\":1}").force);
    TEST_ASSERT_EQUAL(CmdType::NONE, parse("{\"t\":\"prof\"}").type);
    TEST_ASSERT_EQUAL(CmdType::NONE, parse("{\"t\":\"prof\",\"n\":1.5}").type);
}

void test_command_protocol_legacy_pid_maps_present_fields() {
    const Command c = parse("{\"t\":\"pid\",\"kp\":1.5,\"kd\":0.04}");
    TEST_ASSERT_EQUAL(CmdType::SET, c.type);
    TEST_ASSERT_EQUAL_UINT8(2, c.count);
    TEST_ASSERT_EQUAL_STRING("kp", c.items[0].key);
    TEST_ASSERT_EQUAL_STRING("kd", c.items[1].key);
}

void test_command_protocol_legacy_pidv_maps_to_inner_keys() {
    const Command c = parse("{\"t\":\"pidv\",\"kp\":0.003,\"ki\":0.01,\"kd\":0}");
    TEST_ASSERT_EQUAL_UINT8(3, c.count);
    TEST_ASSERT_EQUAL_STRING("kpv", c.items[0].key);
    TEST_ASSERT_EQUAL_STRING("kiv", c.items[1].key);
    TEST_ASSERT_EQUAL_STRING("kdv", c.items[2].key);
}

void test_command_protocol_legacy_spd_scales_from_255() {
    const Command c = parse("{\"t\":\"spd\",\"base\":160,\"min\":51,\"max\":204,\"thrs\":0.5}");
    TEST_ASSERT_EQUAL_UINT8(3, c.count);
    TEST_ASSERT_EQUAL_STRING("vmax", c.items[0].key);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.8f, c.items[0].value);
    TEST_ASSERT_EQUAL_STRING("vmin", c.items[1].key);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.2f, c.items[1].value);
    TEST_ASSERT_EQUAL_STRING("thr", c.items[2].key);
}

void test_command_protocol_legacy_rpm_maps_max_only() {
    const Command c = parse("{\"t\":\"rpm\",\"max\":800,\"base\":400}");
    TEST_ASSERT_EQUAL_UINT8(1, c.count);
    TEST_ASSERT_EQUAL_STRING("rpmMax", c.items[0].key);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 800.0f, c.items[0].value);
}

void test_command_protocol_legacy_pid_without_fields_fails() {
    TEST_ASSERT_EQUAL(CmdType::NONE, parse("{\"t\":\"pid\"}").type);
}

void test_command_protocol_encoder_calibration() {
    Command c = parse("{\"t\":\"cal\",\"op\":\"finish\",\"side\":\"R\",\"rot\":20}");
    TEST_ASSERT_EQUAL(CmdType::ENC_CAL_FINISH, c.type);
    TEST_ASSERT_FALSE(c.leftSide);
    TEST_ASSERT_EQUAL_INT(20, c.rotations);
    c = parse("{\"t\":\"cal\",\"op\":\"start\",\"side\":\"L\"}");
    TEST_ASSERT_EQUAL(CmdType::ENC_CAL_START, c.type);
    TEST_ASSERT_TRUE(c.leftSide);
    TEST_ASSERT_EQUAL(CmdType::NONE, parse("{\"t\":\"cal\",\"op\":\"x\"}").type);
}

void test_command_protocol_format_ack() {
    char buf[96];
    TEST_ASSERT_TRUE(formatAck(buf, sizeof buf, "kp", SetResult::OK, 1.2f));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"k\":\"kp\",\"ok\":1,\"v\":1.2}", buf);
    formatAck(buf, sizeof buf, "kp", SetResult::OUT_OF_RANGE, 0.0f);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"k\":\"kp\",\"ok\":0,\"err\":\"range\"}", buf);
    formatAck(buf, sizeof buf, "zz", SetResult::UNKNOWN_KEY, 0.0f);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"k\":\"zz\",\"ok\":0,\"err\":\"key\"}", buf);
    formatAck(buf, sizeof buf, "invL", SetResult::LOCKED_WHILE_RUNNING, 0.0f);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"k\":\"invL\",\"ok\":0,\"err\":\"running\"}", buf);
}

void test_command_protocol_format_ack_simple() {
    char buf[96];
    formatAckSimple(buf, sizeof buf, "save", true, nullptr);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"k\":\"save\",\"ok\":1}", buf);
    formatAckSimple(buf, sizeof buf, "prof", false, "dirty");
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"k\":\"prof\",\"ok\":0,\"err\":\"dirty\"}", buf);
}

void test_command_protocol_format_cfg_group_pid() {
    char buf[200];
    const RobotConfig cfg = robotcfg::factoryConfig();
    TEST_ASSERT_TRUE(formatCfgGroup(buf, sizeof buf, cfg, "pid"));
    TEST_ASSERT_EQUAL_STRING(
        "{\"t\":\"cfg\",\"g\":\"pid\",\"kp\":1,\"ki\":0,\"kd\":0.03,\"dlp\":0.5}", buf);
}

void test_command_protocol_cfg_groups_fit_ble_limit() {
    char buf[256];
    RobotConfig cfg = robotcfg::factoryConfig();
    cfg.hw.vcut = 6.4321f;
    cfg.hw.pprL = 9999.99f;
    cfg.hw.pprR = 9999.99f;
    for (size_t g = 0; g < robotcfg::GROUP_COUNT; g++) {
        TEST_ASSERT_TRUE(formatCfgGroup(buf, sizeof buf, cfg, robotcfg::GROUPS[g]));
        TEST_ASSERT_TRUE_MESSAGE(std::strlen(buf) <= BLE_MAX_MSG, robotcfg::GROUPS[g]);
    }
}

void test_command_protocol_format_meta_and_info() {
    char buf[128];
    formatCfgMeta(buf, sizeof buf, 1, true, "4.0.0");
    TEST_ASSERT_EQUAL_STRING(
        "{\"t\":\"cfg\",\"g\":\"meta\",\"prof\":1,\"dirty\":1,\"fw\":\"4.0.0\"}", buf);
    formatInfo(buf, sizeof buf, "LFR-RACER-01", "4.0.0", true, 3120, 4870);
    TEST_ASSERT_EQUAL_STRING(
        "{\"t\":\"info\",\"name\":\"LFR-RACER-01\",\"fw\":\"4.0.0\",\"mode\":\"RUN\","
        "\"cfgerr\":1,\"stk\":[3120,4870]}", buf);
}

// E-2: menor folga de pilha das tasks control e comm, em bytes
void test_command_protocol_info_with_stack_fits_180_bytes() {
    char buf[256];
    const char* name = "LFR-RACER-0123456789-ABCDEFGHIJ";  // 31 caracteres
    TEST_ASSERT_EQUAL_UINT32(31, std::strlen(name));
    TEST_ASSERT_TRUE(formatInfo(buf, sizeof buf, name, "4.0.0", true, 65535, 65535));
    TEST_ASSERT_TRUE(std::strlen(buf) <= BLE_MAX_MSG);
    TEST_ASSERT_NOT_NULL(std::strstr(buf, "\"stk\":[65535,65535]}"));
}

void test_command_protocol_format_calibration_results() {
    char buf[96];
    CalReport ok{};
    ok.ok = true;
    formatCalResult(buf, sizeof buf, ok);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"calres\",\"ok\":1}", buf);
    CalReport bad{};
    bad.bad[0] = 3;
    bad.bad[1] = 4;
    bad.nBad = 2;
    formatCalResult(buf, sizeof buf, bad);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"calres\",\"ok\":0,\"ch\":[3,4]}", buf);
    CalReport enc{};
    enc.kind = CalReport::Kind::ENCODER;
    enc.ok = true;
    enc.left = false;
    enc.ppr = 612.5f;
    formatEncCal(buf, sizeof buf, enc);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ecal\",\"side\":\"R\",\"ok\":1,\"ppr\":612.5}", buf);
}

void test_command_protocol_worst_case_telemetry_fits() {
    TelemetryFrame f{};
    f.pos = -1.0f;
    f.corr = -1.0f;
    f.vL = -1023;
    f.vR = -1023;
    f.dtUs = 65535;
    f.rpmL = f.rpmR = f.spL = f.spR = -32000;  // formatador limita a ±9999
    for (int i = 0; i < 8; i++) f.s[i] = 100;
    f.st = 5;
    f.lost = 1;
    char buf[256];
    TEST_ASSERT_TRUE(formatTelemetry(buf, sizeof buf, f));
    TEST_ASSERT_TRUE(std::strlen(buf) <= BLE_MAX_MSG);
    TEST_ASSERT_NOT_NULL(std::strstr(buf, "\"rpmL\":-9999"));
}

void test_command_protocol_worst_case_status_fits() {
    StatusFrame s{};
    s.bat = 100;
    s.vb = 8.4f;
    s.lap = 123.45f;
    for (int i = 0; i < 5; i++) s.laps[i] = 123.45f;
    s.nLaps = 5;
    s.err = 4;
    s.prof = 2;
    s.dirty = 1;
    s.ovr = 65535;
    char buf[256];
    TEST_ASSERT_TRUE(formatStatus(buf, sizeof buf, s));
    TEST_ASSERT_TRUE(std::strlen(buf) <= BLE_MAX_MSG);
    TEST_ASSERT_NOT_NULL(std::strstr(buf, "\"t\":\"sta\""));
}

void test_command_protocol_formatter_reports_overflow() {
    char tiny[10];
    TEST_ASSERT_FALSE(formatAckSimple(tiny, sizeof tiny, "save", true, nullptr));
}

void test_command_queue_fifo_and_capacity() {
    CommandQueue q;
    TEST_ASSERT_TRUE(q.push("a", 1));
    TEST_ASSERT_TRUE(q.push("bb", 2));
    TEST_ASSERT_TRUE(q.push("ccc", 3));
    TEST_ASSERT_TRUE(q.push("dddd", 4));
    TEST_ASSERT_FALSE(q.push("e", 1));  // cheia
    char out[16];
    TEST_ASSERT_TRUE(q.pop(out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("a", out);
    TEST_ASSERT_EQUAL_UINT32(3, q.size());
}

void test_command_queue_rejects_oversized_and_empty() {
    CommandQueue q;
    char big[CommandQueue::MAX_LEN + 1];
    std::memset(big, 'x', sizeof big);
    TEST_ASSERT_FALSE(q.push(big, sizeof big));
    TEST_ASSERT_FALSE(q.push("", 0));
    char out[4];
    TEST_ASSERT_FALSE(q.pop(out, sizeof out));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_command_protocol_parses_set);
    RUN_TEST(test_command_protocol_set_without_value_fails);
    RUN_TEST(test_command_protocol_set_accepts_nan_for_config_to_reject);
    RUN_TEST(test_command_protocol_simple_commands);
    RUN_TEST(test_command_protocol_profile_switch);
    RUN_TEST(test_command_protocol_legacy_pid_maps_present_fields);
    RUN_TEST(test_command_protocol_legacy_pidv_maps_to_inner_keys);
    RUN_TEST(test_command_protocol_legacy_spd_scales_from_255);
    RUN_TEST(test_command_protocol_legacy_rpm_maps_max_only);
    RUN_TEST(test_command_protocol_legacy_pid_without_fields_fails);
    RUN_TEST(test_command_protocol_encoder_calibration);
    RUN_TEST(test_command_protocol_format_ack);
    RUN_TEST(test_command_protocol_format_ack_simple);
    RUN_TEST(test_command_protocol_format_cfg_group_pid);
    RUN_TEST(test_command_protocol_cfg_groups_fit_ble_limit);
    RUN_TEST(test_command_protocol_format_meta_and_info);
    RUN_TEST(test_command_protocol_info_with_stack_fits_180_bytes);
    RUN_TEST(test_command_protocol_format_calibration_results);
    RUN_TEST(test_command_protocol_worst_case_telemetry_fits);
    RUN_TEST(test_command_protocol_worst_case_status_fits);
    RUN_TEST(test_command_protocol_formatter_reports_overflow);
    RUN_TEST(test_command_queue_fifo_and_capacity);
    RUN_TEST(test_command_queue_rejects_oversized_and_empty);
    return UNITY_END();
}
