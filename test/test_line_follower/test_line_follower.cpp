#include <unity.h>
#include "strategy/LineFollower.h"
#include "sensors/Calibration.h"
#include "control/PIDController.h"
#include "control/SpeedProfile.h"
#include "motors/DifferentialDrive.h"

static Calibration* cal;
static PIDController* pid;
static SpeedProfile* speed;
static DifferentialDrive* drive;
static LineFollower* lf;

// Calibra para range 0-1000 em todos os sensores
static void calibrateFlat() {
    int rawMin[8] = {0,0,0,0,0,0,0,0};
    int rawMax[8] = {1000,1000,1000,1000,1000,1000,1000,1000};
    cal->update(rawMin);
    cal->update(rawMax);
}

void setUp() {
    cal   = new Calibration(8);
    pid   = new PIDController(2.0f, 0.0f, 0.0f, -255.0f, 255.0f);
    speed = new SpeedProfile(60, 160, 230, 0.6f);
    drive = new DifferentialDrive(255);
    lf    = new LineFollower(*cal, *pid, *speed, *drive);
    calibrateFlat();
}

void tearDown() {
    delete lf; delete drive; delete speed; delete pid; delete cal;
}

void test_straight_line_equal_high_speeds() {
    // Sensores centrais ativos → posição ≈ 0 → correção ≈ 0 → speeds iguais e altas
    int raw[8] = {0, 0, 0, 700, 700, 0, 0, 0};
    int leftPwm, rightPwm;
    lf->update(raw, leftPwm, rightPwm);
    TEST_ASSERT_EQUAL(rightPwm, leftPwm);
    TEST_ASSERT_TRUE(leftPwm > 150);
}

void test_slight_deviation_proportional_differential() {
    // Linha levemente à direita → diferencial proporcional
    int raw[8] = {0, 0, 0, 0, 700, 800, 0, 0};
    int leftPwm, rightPwm;
    lf->update(raw, leftPwm, rightPwm);
    TEST_ASSERT_TRUE(leftPwm > rightPwm);
}

void test_sharp_curve_reduced_speed() {
    // Linha no extremo direito → velocidade reduzida + forte diferencial
    int raw[8] = {0, 0, 0, 0, 0, 0, 100, 900};
    int leftPwm, rightPwm;
    lf->update(raw, leftPwm, rightPwm);
    TEST_ASSERT_TRUE(leftPwm > rightPwm);
    // Velocidade base deve ser reduzida (média L+R < base normal)
    int avg = (leftPwm + rightPwm) / 2;
    TEST_ASSERT_TRUE(avg < 200);
}

void test_line_lost_maintains_last_direction() {
    // Primeiro update com linha à direita
    int raw_right[8] = {0, 0, 0, 0, 0, 0, 100, 900};
    int lp, rp;
    lf->update(raw_right, lp, rp);
    (void)lp; (void)rp;

    // Segundo update: linha perdida (todos zeros)
    int raw_lost[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    lf->update(raw_lost, lp, rp);

    // Deve manter direção: leftPwm > rightPwm (última direção conhecida)
    TEST_ASSERT_TRUE(lp > rp);
}

void test_pid_update_changes_behavior() {
    int raw[8] = {0, 0, 200, 800, 0, 0, 0, 0};
    int lp1, rp1, lp2, rp2;

    lf->update(raw, lp1, rp1);
    lf->setPID(5.0f, 0.0f, 0.0f);  // Kp maior → diferencial maior
    lf->update(raw, lp2, rp2);

    TEST_ASSERT_TRUE(abs(lp2 - rp2) >= abs(lp1 - rp1));
}

void test_sequential_derivative_smooths() {
    // Com Kd, segunda leitura na mesma posição deve ter menor diferencial que a primeira
    delete pid;
    pid = new PIDController(2.0f, 0.0f, 8.0f, -255.0f, 255.0f);
    delete lf;
    lf = new LineFollower(*cal, *pid, *speed, *drive);

    int raw[8] = {0, 0, 0, 0, 0, 500, 900, 100};
    int lp1, rp1, lp2, rp2;
    lf->update(raw, lp1, rp1);
    lf->update(raw, lp2, rp2);  // mesma posição: derivada = 0, D não contribui
    // Segundo update deve ter diferencial igual ou menor (D amortecer)
    TEST_ASSERT_TRUE(abs(lp2 - rp2) <= abs(lp1 - rp1) + 1);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_straight_line_equal_high_speeds);
    RUN_TEST(test_slight_deviation_proportional_differential);
    RUN_TEST(test_sharp_curve_reduced_speed);
    RUN_TEST(test_line_lost_maintains_last_direction);
    RUN_TEST(test_pid_update_changes_behavior);
    RUN_TEST(test_sequential_derivative_smooths);
    return UNITY_END();
}
