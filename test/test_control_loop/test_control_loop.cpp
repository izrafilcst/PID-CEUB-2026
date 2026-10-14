#include <unity.h>
#include <cmath>
#include <cstdlib>
#include "config/RobotConfig.h"
#include "sensors/Calibration.h"
#include "strategy/ControlLoop.h"

static Calibration* cal = nullptr;
static ControlLoop* loop = nullptr;
static RobotConfig  cfg;
static constexpr float DT = 0.002f;
static constexpr int PWM_MAX = 1023;

// Linha BRANCA sobre fundo preto: o QTR-8A lê baixo sobre o branco (pol = 1, fábrica)
static const int CENTER[8]  = {1000, 1000, 1000, 100, 100, 1000, 1000, 1000};
static const int RIGHT[8]   = {1000, 1000, 1000, 1000, 1000, 300, 200, 1000};
static const int NO_LINE[8] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};

static void calibrateFlat(Calibration& c) {
    int lo[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int hi[8] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    c.update(lo);
    c.update(hi);
}

static ControlInputs in(const int* raw, float rpmL = 0.0f, float rpmR = 0.0f,
                        float dt = DT) {
    ControlInputs i;
    i.raw = raw;
    i.rpmL = rpmL;
    i.rpmR = rpmR;
    i.vbat = 7.4f;
    i.dtS = dt;
    return i;
}

static ControlOutputs run(const int* raw, int steps) {
    ControlOutputs o{};
    for (int k = 0; k < steps; k++) o = loop->step(in(raw));
    return o;
}

void setUp() {
    cal = new Calibration(8);
    calibrateFlat(*cal);
    cfg = robotcfg::factoryConfig();
    cfg.tune.vcomp = 0;  // compensação é testada na OutputStage
    loop = new ControlLoop(*cal, PWM_MAX);
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
}

void tearDown() {
    delete loop;
    delete cal;
}

void test_control_loop_centered_line_drives_straight_at_vmax() {
    const ControlOutputs o = run(CENTER, 1000);
    TEST_ASSERT_EQUAL_INT(o.duties.a, o.duties.b);
    TEST_ASSERT_EQUAL_INT(358, o.duties.a);  // 0,35 × 1023
}

void test_control_loop_ramp_limits_start() {
    const ControlOutputs o = run(CENTER, 1);  // 2,0/s × 2 ms = 0,004
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.004f, o.base);
}

// F-01
void test_control_loop_start_keeps_calibration() {
    TEST_ASSERT_TRUE(cal->isValid());
    const ControlOutputs o = run(RIGHT, 10);
    TEST_ASSERT_TRUE(o.duties.a > o.duties.b);
}

// F-03
void test_control_loop_white_line_on_right_turns_right() {
    run(CENTER, 200);
    const ControlOutputs o = loop->step(in(RIGHT));
    TEST_ASSERT_TRUE(o.error > 0.0f);
    TEST_ASSERT_TRUE(o.duties.a > o.duties.b);
}

// F-05: a base cai na curva, nos dois modos
void test_control_loop_speed_drops_in_curves_pwm_mode() {
    run(CENTER, 1000);
    const ControlOutputs o = loop->step(in(RIGHT));
    TEST_ASSERT_TRUE(o.base < 0.30f);
}

void test_control_loop_speed_drops_in_curves_cascade_mode() {
    cfg.tune.mode = 1;
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
    run(CENTER, 1000);
    const ControlOutputs o = loop->step(in(RIGHT));
    TEST_ASSERT_TRUE((o.spL + o.spR) / 2.0f < 0.30f * cfg.tune.rpmMax);
}

// F-08: com o motor no RPM pedido, o duty é o do feedforward
void test_control_loop_cascade_feedforward_holds_duty() {
    cfg.tune.mode = 1;
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
    ControlOutputs o{};
    float rpmL = 0.0f;
    float rpmR = 0.0f;
    for (int k = 0; k < 1000; k++) {
        o = loop->step(in(CENTER, rpmL, rpmR));
        rpmL = o.spL;  // motor ideal: segue o setpoint
        rpmR = o.spR;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.35f, o.uL);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.35f, o.uR);
}

void test_control_loop_encoder_inversion_flips_measured_rpm() {
    cfg.tune.mode = 1;
    cfg.tune.kiv = 0.0f;
    cfg.hw.einvL = 1;
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
    run(CENTER, 1000);  // base chega a 0,35 → setpoint 245 RPM dos dois lados
    // Os dois estimadores informam +245; com einvL o esquerdo vira -245: erro de 490 RPM
    const ControlOutputs o = loop->step(in(CENTER, 245.0f, 245.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.35f, o.uR);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, o.uL);
}

// F-14 + Review Focus 5
void test_control_loop_start_off_line_times_out() {
    loop->start(NO_LINE, 0.0f, 0.0f);
    ControlOutputs o{};
    for (int k = 0; k < 149; k++) o = loop->step(in(NO_LINE));
    TEST_ASSERT_TRUE(o.lost);
    TEST_ASSERT_FALSE(o.lostTimeout);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, o.error);
    TEST_ASSERT_EQUAL_INT(o.duties.a, o.duties.b);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, cfg.tune.vmin, o.base);  // linha perdida: anda devagar
    o = loop->step(in(NO_LINE));
    TEST_ASSERT_TRUE(o.lostTimeout);
}

void test_control_loop_lost_after_right_keeps_turning_right() {
    run(RIGHT, 5);
    const ControlOutputs o = loop->step(in(NO_LINE));
    TEST_ASSERT_TRUE(o.lost);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, o.error);
    TEST_ASSERT_TRUE(o.duties.a > o.duties.b);
}

void test_control_loop_line_found_resets_lost_timer() {
    loop->start(NO_LINE, 0.0f, 0.0f);
    run(NO_LINE, 140);
    run(CENTER, 1);
    const ControlOutputs o = run(NO_LINE, 140);
    TEST_ASSERT_FALSE(o.lostTimeout);
}

void test_control_loop_sensor_invert_flips_turn() {
    cfg.hw.sinv = 1;
    loop->applyConfig(cfg);
    run(CENTER, 200);
    const ControlOutputs o = loop->step(in(RIGHT));
    TEST_ASSERT_TRUE(o.duties.a < o.duties.b);
}

void test_control_loop_black_line_polarity() {
    cfg.hw.pol = 0;
    loop->applyConfig(cfg);
    const int blackRight[8] = {0, 0, 0, 0, 0, 700, 800, 0};
    run(blackRight, 5);
    TEST_ASSERT_TRUE(loop->step(in(blackRight)).error > 0.0f);
}

// F-06/F-07: o termo D usa o dt real informado
void test_control_loop_uses_real_dt() {
    cfg.tune.kp = 0.0f;
    cfg.tune.kd = 0.001f;  // pequeno o bastante para não saturar em ±1
    cfg.tune.dlp = 1.0f;
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
    loop->step(in(CENTER, 0.0f, 0.0f, 0.002f));
    const float uFast = loop->step(in(RIGHT, 0.0f, 0.0f, 0.002f)).u;
    loop->start(CENTER, 0.0f, 0.0f);
    loop->step(in(CENTER, 0.0f, 0.0f, 0.004f));
    const float uSlow = loop->step(in(RIGHT, 0.0f, 0.0f, 0.004f)).u;
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.0f * uSlow, uFast);
}

// E-4: cascata com RPM ≈ 0 e duty alto por 200 ms contínuos → encoderFault
static bool dutyHigh(const ControlOutputs& o) {
    return std::abs(o.duties.a) >= 0.6f * PWM_MAX || std::abs(o.duties.b) >= 0.6f * PWM_MAX;
}

void test_control_loop_cascade_zero_rpm_raises_encoder_fault() {
    cfg.tune.mode = 1;
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
    int first = -1;
    bool fired = false;
    for (int k = 0; k < 1000 && !fired; k++) {
        const ControlOutputs o = loop->step(in(CENTER, 0.0f, 0.0f));
        if (first < 0 && dutyHigh(o)) first = k;
        if (first < 0 || k < first + 99) {
            TEST_ASSERT_FALSE_MESSAGE(o.encoderFault, "disparou antes de 200 ms");
        } else {
            TEST_ASSERT_TRUE_MESSAGE(o.encoderFault, "não disparou em 200 ms");  // 100 × 2 ms
            fired = true;
        }
    }
    TEST_ASSERT_TRUE(first >= 0);
    TEST_ASSERT_TRUE(fired);
    loop->start(CENTER, 0.0f, 0.0f);  // o start zera o tempo
    TEST_ASSERT_FALSE(loop->step(in(CENTER, 0.0f, 0.0f)).encoderFault);
}

void test_control_loop_encoder_fault_needs_continuous_condition() {
    cfg.tune.mode = 1;
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
    for (int k = 0; k < 1000; k++) {
        const float rpm = (k % 2 == 0) ? 0.0f : 300.0f;  // leitura volta a cada 2 ciclos
        TEST_ASSERT_FALSE(loop->step(in(CENTER, rpm, rpm)).encoderFault);
    }
}

void test_control_loop_pwm_mode_never_raises_encoder_fault() {
    cfg.tune.vmax = 0.9f;
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
    bool high = false;
    for (int k = 0; k < 1000; k++) {
        const ControlOutputs o = loop->step(in(CENTER, 0.0f, 0.0f));
        high = high || dutyHigh(o);
        TEST_ASSERT_FALSE(o.encoderFault);
    }
    TEST_ASSERT_TRUE(high);  // o duty alto existiu: só o modo impede o disparo
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_control_loop_centered_line_drives_straight_at_vmax);
    RUN_TEST(test_control_loop_ramp_limits_start);
    RUN_TEST(test_control_loop_start_keeps_calibration);
    RUN_TEST(test_control_loop_white_line_on_right_turns_right);
    RUN_TEST(test_control_loop_speed_drops_in_curves_pwm_mode);
    RUN_TEST(test_control_loop_speed_drops_in_curves_cascade_mode);
    RUN_TEST(test_control_loop_cascade_feedforward_holds_duty);
    RUN_TEST(test_control_loop_encoder_inversion_flips_measured_rpm);
    RUN_TEST(test_control_loop_start_off_line_times_out);
    RUN_TEST(test_control_loop_lost_after_right_keeps_turning_right);
    RUN_TEST(test_control_loop_line_found_resets_lost_timer);
    RUN_TEST(test_control_loop_sensor_invert_flips_turn);
    RUN_TEST(test_control_loop_black_line_polarity);
    RUN_TEST(test_control_loop_uses_real_dt);
    RUN_TEST(test_control_loop_cascade_zero_rpm_raises_encoder_fault);
    RUN_TEST(test_control_loop_encoder_fault_needs_continuous_condition);
    RUN_TEST(test_control_loop_pwm_mode_never_raises_encoder_fault);
    return UNITY_END();
}
