#include <unity.h>
#include <cstring>
#include "config/ConfigStore.h"
#include "config/RobotConfig.h"
#include "sensors/Calibration.h"
#include "storage/InMemoryStore.h"

static InMemoryStore* mem = nullptr;
static ConfigStore*   cs  = nullptr;

void setUp() {
    mem = new InMemoryStore();
    mem->begin("lfr");
    cs = new ConfigStore(*mem);
}
void tearDown() {
    delete cs;  cs = nullptr;
    delete mem; mem = nullptr;
}

// Monta um blob no formato do ConfigStore (para forjar casos de erro)
static size_t forgeBlob(uint8_t* buf, uint16_t version, const void* payload, uint16_t size) {
    buf[0] = 0x46; buf[1] = 0x4C;
    buf[2] = version & 0xFF; buf[3] = version >> 8;
    buf[4] = size & 0xFF; buf[5] = size >> 8;
    std::memcpy(buf + 6, payload, size);
    const uint32_t crc = ConfigStore::crc32(buf, 6 + size);
    for (int i = 0; i < 4; i++) buf[6 + size + i] = static_cast<uint8_t>(crc >> (8 * i));
    return 6 + size + 4;
}

void test_config_store_crc32_known_vector() {
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, ConfigStore::crc32(data, sizeof data));
}

void test_config_store_in_memory_blob_semantics() {
    const uint8_t data[4] = {1, 2, 3, 4};
    TEST_ASSERT_TRUE(mem->putBlob("b", data, sizeof data));
    TEST_ASSERT_TRUE(mem->exists("b"));
    uint8_t small[2] = {0, 0};
    TEST_ASSERT_EQUAL_UINT32(4, mem->getBlob("b", small, sizeof small));
    TEST_ASSERT_EQUAL_UINT8(0, small[0]);  // não coube: nada copiado
    uint8_t big[8] = {};
    TEST_ASSERT_EQUAL_UINT32(4, mem->getBlob("b", big, sizeof big));
    TEST_ASSERT_EQUAL_UINT8(4, big[3]);
    TEST_ASSERT_EQUAL_UINT32(0, mem->getBlob("none", big, sizeof big));
}

void test_config_store_first_boot_returns_factory_with_warning() {
    RobotConfig c{};
    c.tune.kp = 9.0f;
    const ConfigStore::LoadReport r = cs->load(c);
    TEST_ASSERT_FALSE(r.hardwareOk);
    TEST_ASSERT_FALSE(r.profileOk);
    TEST_ASSERT_EQUAL_UINT8(0, c.profile);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, c.tune.kp);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.35f, c.tune.vmax);
}

void test_config_store_save_load_roundtrip() {
    RobotConfig c = robotcfg::factoryConfig();
    c.profile = 2;
    c.tune = robotcfg::factoryProfile(2);
    c.tune.kp = 2.25f;
    c.hw.invR = 1;
    TEST_ASSERT_TRUE(cs->saveActive(c));
    RobotConfig back{};
    const ConfigStore::LoadReport r = cs->load(back);
    TEST_ASSERT_TRUE(r.hardwareOk);
    TEST_ASSERT_TRUE(r.profileOk);
    TEST_ASSERT_EQUAL_UINT8(2, back.profile);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.25f, back.tune.kp);
    TEST_ASSERT_EQUAL_UINT8(1, back.hw.invR);
}

void test_config_store_profiles_are_independent() {
    RobotConfig c = robotcfg::factoryConfig();
    c.profile = 1;
    c.tune.kp = 2.0f;
    cs->saveActive(c);
    c.profile = 0;
    c.tune.kp = 1.5f;
    cs->saveActive(c);
    TuningProfile p{};
    TEST_ASSERT_TRUE(cs->loadProfile(1, p));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, p.kp);
    TEST_ASSERT_FALSE(cs->loadProfile(2, p));  // nunca salvo → fábrica do perfil 2
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.70f, p.vmax);
}

void test_config_store_rejects_corrupted_blob() {
    RobotConfig c = robotcfg::factoryConfig();
    c.tune.kp = 3.0f;
    cs->saveActive(c);
    uint8_t buf[128];
    const size_t len = mem->getBlob("prof0", buf, sizeof buf);
    buf[10] ^= 0xFF;
    mem->putBlob("prof0", buf, len);
    TuningProfile p{};
    TEST_ASSERT_FALSE(cs->loadProfile(0, p));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, p.kp);
}

// Review Focus 3: firmware antigo gravou uma struct de outro tamanho
void test_config_store_rejects_blob_of_other_size() {
    uint8_t payload[20] = {};
    uint8_t buf[64];
    const size_t len = forgeBlob(buf, 1, payload, sizeof payload);
    mem->putBlob("hw", buf, len);
    HardwareConfig hw{};
    hw.invL = 1;
    TEST_ASSERT_FALSE(cs->loadHardware(hw));
    TEST_ASSERT_EQUAL_UINT8(0, hw.invL);
}

void test_config_store_rejects_out_of_range_blob() {
    TuningProfile bad = robotcfg::factoryProfile(0);
    bad.kp = 999.0f;  // CRC válido, valor fora da faixa
    uint8_t buf[128];
    const size_t len = forgeBlob(buf, 1, &bad, sizeof bad);
    mem->putBlob("prof0", buf, len);
    TuningProfile p{};
    TEST_ASSERT_FALSE(cs->loadProfile(0, p));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, p.kp);
}

void test_config_store_ppr_keys_override_hardware_blob() {
    RobotConfig c = robotcfg::factoryConfig();
    cs->saveActive(c);
    mem->putFloat("left_ppr", 1234.0f);
    HardwareConfig hw{};
    TEST_ASSERT_TRUE(cs->loadHardware(hw));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1234.0f, hw.pprL);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 600.0f, hw.pprR);
}

void test_config_store_save_writes_ppr_keys() {
    RobotConfig c = robotcfg::factoryConfig();
    c.hw.pprR = 1400.0f;
    cs->saveActive(c);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1400.0f, mem->getFloat("right_ppr", 0.0f));
}

void test_config_store_refuses_to_save_invalid_config() {
    RobotConfig c = robotcfg::factoryConfig();
    c.tune.kp = 99.0f;
    TEST_ASSERT_FALSE(cs->saveActive(c));
    TEST_ASSERT_FALSE(mem->exists("prof0"));
}

void test_config_store_active_index_out_of_range_falls_back_to_zero() {
    const uint8_t seven = 7;
    uint8_t buf[16];
    const size_t len = forgeBlob(buf, 1, &seven, 1);
    mem->putBlob("prof_act", buf, len);
    TEST_ASSERT_EQUAL_UINT8(0, cs->loadActiveIndex());
}

void test_config_store_calibration_roundtrip() {
    CalibrationData d{};
    for (int i = 0; i < 8; i++) { d.min[i] = static_cast<int16_t>(10 * i); d.max[i] = 900; }
    TEST_ASSERT_TRUE(cs->saveCalibration(d));
    CalibrationData back{};
    TEST_ASSERT_TRUE(cs->loadCalibration(back));
    TEST_ASSERT_EQUAL_INT16(70, back.min[7]);
    TEST_ASSERT_EQUAL_INT16(900, back.max[0]);
}

void test_config_store_missing_calibration() {
    CalibrationData d{};
    TEST_ASSERT_FALSE(cs->loadCalibration(d));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_config_store_crc32_known_vector);
    RUN_TEST(test_config_store_in_memory_blob_semantics);
    RUN_TEST(test_config_store_first_boot_returns_factory_with_warning);
    RUN_TEST(test_config_store_save_load_roundtrip);
    RUN_TEST(test_config_store_profiles_are_independent);
    RUN_TEST(test_config_store_rejects_corrupted_blob);
    RUN_TEST(test_config_store_rejects_blob_of_other_size);
    RUN_TEST(test_config_store_rejects_out_of_range_blob);
    RUN_TEST(test_config_store_ppr_keys_override_hardware_blob);
    RUN_TEST(test_config_store_save_writes_ppr_keys);
    RUN_TEST(test_config_store_refuses_to_save_invalid_config);
    RUN_TEST(test_config_store_active_index_out_of_range_falls_back_to_zero);
    RUN_TEST(test_config_store_calibration_roundtrip);
    RUN_TEST(test_config_store_missing_calibration);
    return UNITY_END();
}
