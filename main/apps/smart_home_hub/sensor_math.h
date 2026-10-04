#ifndef SENSOR_MATH_H
#define SENSOR_MATH_H

// Pure arithmetic behind the M5Stack CoreS3 on-board sensors. No ESP headers,
// no I2C, no globals: the driver in sensor_monitor.cc owns the hardware and
// calls in here for the conversions, which keeps the tricky parts (a three
// segment transfer function, a gain table, an acos that must not see a value
// above 1) checkable on the host. See scripts/tests/sensor_math_test.cc.

#include <cmath>
#include <cstdint>

// --- LTR-553ALS-WA (Lite-On): ambient light + proximity, I2C addr 0x23 -----

// Gain reported in bits[6:4] of ALS_PS_STATUS. Codes 4 and 5 are reserved on
// the part, hence the zeros: they must not reach the divisor.
constexpr float kLtr553GainCount[8] = {1.0f, 2.0f, 4.0f, 8.0f, 0.0f, 0.0f, 48.0f, 96.0f};

// Integration time in ms, indexed by the 3-bit field in MEAS_RATE.
constexpr uint16_t kLtr553IntTimeMs[8] = {100, 50, 200, 400, 150, 250, 300, 350};

// Illuminance from the two 16-bit ALS channels, CH0 (visible + infrared) and
// CH1 (infrared only), for the gain and integration time actually programmed.
// Returns 0 for the saturated and all-zero sentinel patterns so the caller
// never has to special-case them.
inline float Ltr553Lux(uint16_t ch0, uint16_t ch1, float gain, float int_time_ms) {
    if (ch0 == 0xFFFF || ch1 == 0xFFFF || (ch0 == 0 && ch1 == 0)) return 0.0f;
    if (gain <= 0.0f || int_time_ms <= 0.0f) return 0.0f;  // reserved gain code

    // Float throughout: ch0 + ch1 overflows 16 bits for bright readings.
    const float visible = static_cast<float>(ch0);
    const float infrared = static_cast<float>(ch1);
    const float ratio = infrared / (visible + infrared);

    // Datasheet fits three segments over the CH1/(CH0+CH1) ratio.
    float lux = 0.0f;
    if (ratio < 0.45f) {
        lux = 1.7743f * visible + 1.1059f * infrared;
    } else if (ratio < 0.64f) {
        lux = 4.2785f * visible - 1.9548f * infrared;
    } else if (ratio < 0.85f) {
        lux = 0.5926f * visible + 0.1185f * infrared;
    }
    // Above 0.85 there is no calibrated segment left; the room is so dark that
    // the part reads near its noise floor. Reporting 0 beats extrapolating.

    lux /= gain * (int_time_ms / 100.0f);
    return lux > 0.0f ? lux : 0.0f;
}

// --- BMI270 (Bosch): 6-axis IMU, I2C addr 0x69 (SDO tied high) -------------

// Must match the ACC_CONF / GYR_CONF range the driver programs. Getting this
// pair wrong is silent: the chip still reports plausible-looking numbers, just
// scaled by a constant factor.
constexpr float kBmi270AccLsbPerG = 16384.0f;  // +/-2 g
constexpr float kBmi270GyrLsbPerDps = 16.4f;   // +/-2000 dps

inline float Bmi270AccelG(int16_t raw) {
    return static_cast<float>(raw) / kBmi270AccLsbPerG;
}

inline float Bmi270GyroDps(int16_t raw) {
    return static_cast<float>(raw) / kBmi270GyrLsbPerDps;
}

// Angle between the board and the table, 0 when lying flat either way up and
// 90 when standing on an edge.
inline float TiltDegrees(float ax, float ay, float az) {
    const float magnitude = std::sqrt(ax * ax + ay * ay + az * az);
    if (magnitude < 0.1f) return 0.0f;  // free fall, or the sensor is not ready
    const float vertical = std::fabs(az) / magnitude;
    // Rounding can push this a hair above 1, and acos(>1) is NaN.
    return std::acos(vertical < 1.0f ? vertical : 1.0f) * 180.0f / 3.14159265f;
}

inline float RollDegrees(float ax, float ay, float az) {
    (void)ax;
    return std::atan2(ay, az) * 180.0f / 3.14159265f;
}

inline float PitchDegrees(float ax, float ay, float az) {
    return std::atan2(-ax, std::sqrt(ay * ay + az * az)) * 180.0f / 3.14159265f;
}

// Which face is pointing down, in the wording the dashboard shows.
inline const char* PostureFace(float ax, float ay, float az) {
    const float x = std::fabs(ax);
    const float y = std::fabs(ay);
    const float z = std::fabs(az);
    if (y >= x && y >= z) return ay > 0.0f ? "mặt trước" : "mặt sau";
    if (x >= z) return ax > 0.0f ? "cạnh phải" : "cạnh trái";
    return az > 0.0f ? "mặt lưng úp xuống" : "mặt kính úp xuống";
}

#endif  // SENSOR_MATH_H
