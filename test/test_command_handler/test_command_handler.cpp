#include <unity.h>
#include <cstring>
#include <string>
#include <vector>
#include "comm/CommandHandler.h"
#include "comm/CommandProtocol.h"
#include "comm/Mailbox.h"
#include "config/ConfigStore.h"
#include "storage/InMemoryStore.h"

class VectorSink : public ResponseSink {
public:
    void send(const char* json) override { msgs.emplace_back(json); }
    bool contains(const char* fragment) const {
        for (const std::string& m : msgs) {
            if (m.find(fragment) != std::string::npos) return true;
        }
        return false;
    }
    std::vector<std::string> msgs;
};

static InMemoryStore*  mem = nullptr;
static ConfigStore*    store = nullptr;
static RobotConfig     live;
static CommandHandler* h = nullptr;
static VectorSink      sink;

static void send(const char* json, bool running = false) {
    Command c;
    TEST_ASSERT_TRUE_MESSAGE(parseCommand(json, c), json);
    ControlMsg msg;
    h->handle(c, running, sink, msg);
}

static ControlMsg sendForMsg(const char* json, bool running = false) {
    Command c;
    parseCommand(json, c);
    ControlMsg msg;
    h->handle(c, running, sink, msg);
    return msg;
}

void setUp() {
    mem = new InMemoryStore();
    mem->begin("lfr");
    store = new ConfigStore(*mem);
    live = robotcfg::factoryConfig();
    h = new CommandHandler(live, *store, "4.0.0");
    sink.msgs.clear();
}

void tearDown() {
    delete h;
    delete store;
    delete mem;
}

void test_command_handler_set_applies_and_marks_dirty() {
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":1.5}");
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.5f, live.tune.kp);
    TEST_ASSERT_TRUE(h->dirty());
    TEST_ASSERT_TRUE(h->consumeConfigChanged());
    TEST_ASSERT_FALSE(h->consumeConfigChanged());
    TEST_ASSERT_TRUE(sink.contains("\"k\":\"kp\",\"ok\":1"));
}

void test_command_handler_set_unknown_key_is_not_dirty() {
    send("{\"t\":\"set\",\"k\":\"zz\",\"v\":1}");
    TEST_ASSERT_FALSE(h->dirty());
    TEST_ASSERT_TRUE(sink.contains("\"err\":\"key\""));
}

void test_command_handler_set_nan_is_rejected() {
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":nan}");
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, live.tune.kp);
    TEST_ASSERT_TRUE(sink.contains("\"err\":\"range\""));
}

void test_command_handler_hardware_key_locked_while_running() {
    send("{\"t\":\"set\",\"k\":\"invL\",\"v\":1}", true);
    TEST_ASSERT_EQUAL_UINT8(0, live.hw.invL);
    TEST_ASSERT_TRUE(sink.contains("\"err\":\"running\""));
}

void test_command_handler_legacy_spd_lowers_both_limits() {
    // vmax 0,2 < vmin 0,25 atual: só passa na 2ª passada, depois de vmin descer
    send("{\"t\":\"spd\",\"min\":25,\"max\":51}");
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.2f, live.tune.vmax);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.098f, live.tune.vmin);
}

void test_command_handler_save_refused_while_running() {
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":2}");
    send("{\"t\":\"save\"}", true);
    TEST_ASSERT_TRUE(sink.contains("\"k\":\"save\",\"ok\":0,\"err\":\"running\""));
    TEST_ASSERT_FALSE(mem->exists("prof0"));
    TEST_ASSERT_TRUE(h->dirty());
}

void test_command_handler_save_persists_and_clears_dirty() {
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":2}");
    send("{\"t\":\"save\"}");
    TEST_ASSERT_FALSE(h->dirty());
    RobotConfig back{};
    store->load(back);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, back.tune.kp);
}

void test_command_handler_load_discards_changes() {
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":2}");
    send("{\"t\":\"save\"}");
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":3}");
    send("{\"t\":\"load\"}");
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, live.tune.kp);
    TEST_ASSERT_FALSE(h->dirty());
}

void test_command_handler_defaults_restore_factory_live() {
    send("{\"t\":\"set\",\"k\":\"kd\",\"v\":0.5}");
    send("{\"t\":\"defaults\"}");
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.03f, live.tune.kd);
    TEST_ASSERT_TRUE(h->dirty());
}

void test_command_handler_profile_switch_refused_when_dirty() {
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":2}");
    send("{\"t\":\"prof\",\"n\":2}");
    TEST_ASSERT_EQUAL_UINT8(0, live.profile);
    TEST_ASSERT_TRUE(sink.contains("\"err\":\"dirty\""));
    send("{\"t\":\"prof\",\"n\":2,\"f\":1}");
    TEST_ASSERT_EQUAL_UINT8(2, live.profile);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.70f, live.tune.vmax);
    TEST_ASSERT_FALSE(h->dirty());
}

void test_command_handler_profile_out_of_range() {
    send("{\"t\":\"prof\",\"n\":5}");
    TEST_ASSERT_TRUE(sink.contains("\"err\":\"range\""));
    TEST_ASSERT_EQUAL_UINT8(0, live.profile);
}

void test_command_handler_get_sends_all_groups_within_limit() {
    send("{\"t\":\"get\"}");
    TEST_ASSERT_EQUAL_UINT32(robotcfg::GROUP_COUNT + 2, sink.msgs.size());  // info + grupos + meta
    for (const std::string& m : sink.msgs) TEST_ASSERT_TRUE(m.size() <= BLE_MAX_MSG);
    TEST_ASSERT_TRUE(sink.contains("\"g\":\"meta\""));
}

void test_command_handler_state_commands_become_control_requests() {
    TEST_ASSERT_EQUAL(ControlRequest::START, sendForMsg("{\"t\":\"start\"}").req);
    TEST_ASSERT_EQUAL(ControlRequest::STOP, sendForMsg("{\"t\":\"stop\"}").req);
    TEST_ASSERT_EQUAL(ControlRequest::RESET, sendForMsg("{\"t\":\"reset\"}").req);
    TEST_ASSERT_EQUAL(ControlRequest::SENSOR_CAL, sendForMsg("{\"t\":\"scal\"}").req);
}

void test_command_handler_encoder_calibration_request() {
    const ControlMsg m = sendForMsg("{\"t\":\"cal\",\"op\":\"finish\",\"side\":\"R\",\"rot\":20}");
    TEST_ASSERT_EQUAL(ControlRequest::ENC_CAL_FINISH, m.req);
    TEST_ASSERT_FALSE(m.leftSide);
    TEST_ASSERT_EQUAL_INT(20, m.rotations);
    const ControlMsg r = sendForMsg("{\"t\":\"cal\",\"op\":\"start\"}", true);
    TEST_ASSERT_EQUAL(ControlRequest::NONE, r.req);
    TEST_ASSERT_TRUE(sink.contains("\"k\":\"cal\",\"ok\":0,\"err\":\"running\""));
}

void test_mailbox_delivers_latest_value_once() {
    Mailbox<int> box;
    uint32_t seen = 0;
    int v = 0;
    TEST_ASSERT_FALSE(box.fetch(v, seen));
    box.post(1);
    box.post(2);
    TEST_ASSERT_TRUE(box.fetch(v, seen));
    TEST_ASSERT_EQUAL_INT(2, v);
    TEST_ASSERT_FALSE(box.fetch(v, seen));
}

// E-1 (F-33): modo competição
static void lockOn() {
    send("{\"t\":\"set\",\"k\":\"lock\",\"v\":1}");
    sink.msgs.clear();
}

void test_command_handler_lock_refuses_everything_while_running() {
    lockOn();
    const char* cmds[] = {
        "{\"t\":\"stop\"}", "{\"t\":\"start\"}", "{\"t\":\"reset\"}", "{\"t\":\"get\"}",
        "{\"t\":\"scal\"}", "{\"t\":\"save\"}", "{\"t\":\"prof\",\"n\":1}",
        "{\"t\":\"set\",\"k\":\"kp\",\"v\":2}", "{\"t\":\"pid\",\"kp\":2,\"ki\":0,\"kd\":0}"};
    for (const char* c : cmds) {
        sink.msgs.clear();
        const ControlMsg m = sendForMsg(c, true);
        TEST_ASSERT_EQUAL_MESSAGE(ControlRequest::NONE, m.req, c);
        TEST_ASSERT_TRUE_MESSAGE(sink.contains("\"ok\":0,\"err\":\"locked\""), c);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, h->live().tune.kp);
}

void test_command_handler_lock_keeps_commands_outside_run() {
    lockOn();
    const ControlMsg m = sendForMsg("{\"t\":\"start\"}", false);
    TEST_ASSERT_EQUAL(ControlRequest::START, m.req);
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":2}", false);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, h->live().tune.kp);
}

void test_command_handler_unlocked_stop_still_works_while_running() {
    const ControlMsg m = sendForMsg("{\"t\":\"stop\"}", true);
    TEST_ASSERT_EQUAL(ControlRequest::STOP, m.req);
}

// A-WP1-01: o info (fw, cfgerr, stk) também sai no get, antes dos cfg
static DeviceInfo fakeInfo() {
    DeviceInfo d;
    d.name = "LFR-TEST";
    d.cfgErr = true;
    d.stkControl = 1234;
    d.stkComm = 5678;
    return d;
}

void test_command_handler_get_sends_info_before_config() {
    h->setInfoSource(fakeInfo);
    send("{\"t\":\"get\"}");
    TEST_ASSERT_EQUAL_UINT32(robotcfg::GROUP_COUNT + 2, sink.msgs.size());
    TEST_ASSERT_EQUAL_STRING(
        "{\"t\":\"info\",\"name\":\"LFR-TEST\",\"fw\":\"4.0.0\",\"mode\":\"RUN\","
        "\"cfgerr\":1,\"stk\":[1234,5678]}", sink.msgs[0].c_str());
    TEST_ASSERT_NOT_NULL(std::strstr(sink.msgs[1].c_str(), "\"g\":\"hw\""));
    TEST_ASSERT_NOT_NULL(std::strstr(sink.msgs.back().c_str(), "\"g\":\"meta\""));
}

// A-WP1-01: o mesmo par info + cfg que o comm manda quando o cliente se inscreve
void test_command_handler_info_and_config_on_subscribe() {
    h->setInfoSource(fakeInfo);
    h->sendInfoAndConfig(sink);
    TEST_ASSERT_EQUAL_UINT32(robotcfg::GROUP_COUNT + 2, sink.msgs.size());
    TEST_ASSERT_NOT_NULL(std::strstr(sink.msgs[0].c_str(), "\"t\":\"info\""));
    TEST_ASSERT_NOT_NULL(std::strstr(sink.msgs[0].c_str(), "\"cfgerr\":1"));
}

void test_command_handler_info_without_source_has_safe_defaults() {
    send("{\"t\":\"get\"}");
    TEST_ASSERT_NOT_NULL(std::strstr(sink.msgs[0].c_str(), "\"cfgerr\":0,\"stk\":[0,0]"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_command_handler_set_applies_and_marks_dirty);
    RUN_TEST(test_command_handler_set_unknown_key_is_not_dirty);
    RUN_TEST(test_command_handler_set_nan_is_rejected);
    RUN_TEST(test_command_handler_hardware_key_locked_while_running);
    RUN_TEST(test_command_handler_legacy_spd_lowers_both_limits);
    RUN_TEST(test_command_handler_save_refused_while_running);
    RUN_TEST(test_command_handler_save_persists_and_clears_dirty);
    RUN_TEST(test_command_handler_load_discards_changes);
    RUN_TEST(test_command_handler_defaults_restore_factory_live);
    RUN_TEST(test_command_handler_profile_switch_refused_when_dirty);
    RUN_TEST(test_command_handler_profile_out_of_range);
    RUN_TEST(test_command_handler_get_sends_all_groups_within_limit);
    RUN_TEST(test_command_handler_state_commands_become_control_requests);
    RUN_TEST(test_command_handler_encoder_calibration_request);
    RUN_TEST(test_mailbox_delivers_latest_value_once);
    RUN_TEST(test_command_handler_lock_refuses_everything_while_running);
    RUN_TEST(test_command_handler_lock_keeps_commands_outside_run);
    RUN_TEST(test_command_handler_unlocked_stop_still_works_while_running);
    RUN_TEST(test_command_handler_get_sends_info_before_config);
    RUN_TEST(test_command_handler_info_and_config_on_subscribe);
    RUN_TEST(test_command_handler_info_without_source_has_safe_defaults);
    return UNITY_END();
}
