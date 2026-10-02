// Host test for main/apps/smart_home_hub/sensor_math.h. Compiled and run by
// scripts/tests/test_sensor_math.py with the host compiler, so the sensor
// conversions are checked without a CoreS3 on the desk.
//
// No test framework: the repo has none, and a counter plus exit code is all
// this needs.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "sensor_math.h"

namespace {

int g_failures = 0;
int g_checks = 0;

void Check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        std::printf("  FAIL  %s\n", what);
        ++g_failures;
    }
}

void CheckNear(float got, float want, float tolerance, const char* what) {
    ++g_checks;
    if (!(std::fabs(got - want) <= tolerance)) {
        std::printf("  FAIL  %s: got %.4f, want %.4f (+/-%.4f)\n", what, got, want, tolerance);
        ++g_failures;
    }
}

void CheckText(const char* got, const char* want, const char* what) {
    ++g_checks;
    if (std::strcmp(got, want) != 0) {
        std::printf("  FAIL  %s: got '%s', want '%s'\n", what, got, want);
        ++g_failures;
    }
}

void TestLtr553InvalidReadings() {
    std::printf("LTR-553: sentinel and reserved gain\n");
    // Saturated channels and total darkness must not become a huge number.
    CheckNear(Ltr553Lux(0xFFFF, 100, 1.0f, 100.0f), 0.0f, 0.0f, "saturated CH0");
    CheckNear(Ltr553Lux(100, 0xFFFF, 1.0f, 100.0f), 0.0f, 0.0f, "saturated CH1");
    CheckNear(Ltr553Lux(0, 0, 1.0f, 100.0f), 0.0f, 0.0f, "all zero");
    // Gain codes 4 and 5 are reserved: dividing by them used to give infinity.
    for (int code = 4; code <= 5; code++) {
        CheckNear(Ltr553Lux(1000, 100, kLtr553GainCount[code], 100.0f), 0.0f, 0.0f,
                  "reserved gain code does not divide by zero");
    }
    CheckNear(Ltr553Lux(1000, 100, 1.0f, 0.0f), 0.0f, 0.0f, "zero integration time");
}

void TestLtr553Segments() {
    std::printf("LTR-553: the three calibrated ratio segments\n");
    // Segment 1, ratio 0.0: 1.7743*1000 + 1.1059*0, over gain 1 and 100 ms.
    CheckNear(Ltr553Lux(1000, 0, 1.0f, 100.0f), 1774.3f, 0.1f, "segment 1 (no infrared)");
    // Segment 2, ratio 0.5: 4.2785*1000 - 1.9548*1000.
    CheckNear(Ltr553Lux(1000, 1000, 1.0f, 100.0f), 2323.7f, 0.1f, "segment 2 (ratio 0.50)");
    // Segment 3, ratio 1000/1500 = 0.667: 0.5926*500 + 0.1185*1000.
    CheckNear(Ltr553Lux(500, 1000, 1.0f, 100.0f), 414.8f, 0.1f, "segment 3 (ratio 0.67)");
    // Past the last segment there is no calibration left.
    CheckNear(Ltr553Lux(100, 1000, 1.0f, 100.0f), 0.0f, 0.0f, "beyond segment 3");
}

void TestLtr553Scaling() {
    std::printf("LTR-553: gain and integration time scale the result\n");
    const float base = Ltr553Lux(1000, 0, 1.0f, 100.0f);
    CheckNear(Ltr553Lux(1000, 0, 2.0f, 100.0f), base / 2.0f, 0.1f, "gain 2x halves lux");
    CheckNear(Ltr553Lux(1000, 0, 8.0f, 100.0f), base / 8.0f, 0.1f, "gain 8x divides by 8");
    CheckNear(Ltr553Lux(1000, 0, 1.0f, 200.0f), base / 2.0f, 0.1f, "200 ms halves lux");
    // More light must never read as less light.
    Check(Ltr553Lux(4000, 0, 1.0f, 100.0f) > Ltr553Lux(2000, 0, 1.0f, 100.0f), "brighter is more lux");
    // The channel sum must be taken in float: 0xFFFE + 0xFFFE wraps to 0xFFFC
    // in 16-bit arithmetic and used to poison the ratio.
    const float bright = Ltr553Lux(0xFFFE, 0xFFFE, 1.0f, 100.0f);
    Check(std::isfinite(bright) && bright > 0.0f, "saturated-but-valid reading stays finite");
}

void TestBmi270Scale() {
    std::printf("BMI270: LSB scaling for +/-2 g and +/-2000 dps\n");
    CheckNear(Bmi270AccelG(16384), 1.0f, 0.0001f, "1 g");
    CheckNear(Bmi270AccelG(-16384), -1.0f, 0.0001f, "-1 g");
    CheckNear(Bmi270AccelG(0), 0.0f, 0.0f, "no acceleration");
    CheckNear(Bmi270GyroDps(164), 10.0f, 0.01f, "10 dps");
    CheckNear(Bmi270GyroDps(-3280), -200.0f, 0.01f, "-200 dps");
}

void TestTilt() {
    std::printf("BMI270: tilt away from the table\n");
    CheckNear(TiltDegrees(0.0f, 0.0f, 1.0f), 0.0f, 0.1f, "flat, screen up");
    CheckNear(TiltDegrees(0.0f, 0.0f, -1.0f), 0.0f, 0.1f, "flat, screen down");
    CheckNear(TiltDegrees(1.0f, 0.0f, 0.0f), 90.0f, 0.1f, "upright on the X edge");
    CheckNear(TiltDegrees(0.0f, 1.0f, 0.0f), 90.0f, 0.1f, "upright on the Y edge");
    // Only the z share of the total matters, so a 45 degree tilt needs the
    // horizontal component too, not a unit vector along z (which is flat).
    CheckNear(TiltDegrees(0.7071f, 0.0f, 0.7071f), 45.0f, 0.2f, "tilted 45 degrees");
    // Free fall, and the window right after the sensor is enabled.
    CheckNear(TiltDegrees(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, "free fall is not NaN");
    CheckNear(TiltDegrees(0.0f, 0.0f, 0.05f), 0.0f, 0.0f, "below the magnitude floor");
    Check(std::isfinite(TiltDegrees(0.6f, 0.6f, 0.6f)), "diagonal read is finite");
}

void TestPosture() {
    std::printf("BMI270: which face is down\n");
    CheckText(PostureFace(0.0f, 1.0f, 0.0f), "mặt trước", "Y up");
    CheckText(PostureFace(0.0f, -1.0f, 0.0f), "mặt sau", "Y down");
    CheckText(PostureFace(1.0f, 0.0f, 0.0f), "cạnh phải", "X up");
    CheckText(PostureFace(-1.0f, 0.0f, 0.0f), "cạnh trái", "X down");
    CheckText(PostureFace(0.0f, 0.0f, 1.0f), "mặt lưng úp xuống", "Z up");
    CheckText(PostureFace(0.0f, 0.0f, -1.0f), "mặt kính úp xuống", "Z down");
    // Gravity is never exactly on one axis at rest on a tilted desk, so the
    // dominant axis has to win on magnitude alone.
    CheckText(PostureFace(0.1f, 0.0f, 0.9f), "mặt lưng úp xuống", "mostly flat, tilted");
}

}  // namespace

int main() {
    std::printf("sensor_math host tests\n");
    TestLtr553InvalidReadings();
    TestLtr553Segments();
    TestLtr553Scaling();
    TestBmi270Scale();
    TestTilt();
    TestPosture();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
