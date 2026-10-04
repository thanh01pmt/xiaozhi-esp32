#include "sensor_monitor.h"
#include "sensor_math.h"
#include "board.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <esp_netif.h>
// bmi270_api.h exposes the Bosch 8 KB configuration blob (bmi270_config_file)
// that every BMI270 needs after power-up, plus the register map.
#include <bmi270_api.h>
#include <cstring>

#define TAG "SensorMonitor"

// ---------------------------------------------------------------------------
// M5Stack CoreS3 on-board sensors (I2C port 1, SDA=GPIO12, SCL=GPIO11)
//   LTR-553ALS-WA  ambient light + proximity, addr 0x23
//   BMI270         6-axis IMU, addr 0x69 (SDO tied high on CoreS3)
// ---------------------------------------------------------------------------

namespace {

// --- LTR-553ALS-WA (Lite-On) -------------------------------------------------
constexpr uint8_t kLtr553Addr = 0x23;
constexpr uint8_t kLtrRegAlsContr = 0x80;  // bit0 active, bit1 sw_reset, bits[4:2] gain
constexpr uint8_t kLtrRegPsContr = 0x81;   // bit0 mode_b, bit1 active
constexpr uint8_t kLtrRegMeasRate = 0x85;  // bits[5:3] repeat rate, bits[2:0] integration time
constexpr uint8_t kLtrRegStatus = 0x8C;    // bit2 als_new_data, bit7 invalid, bits[6:4] gain
constexpr uint8_t kLtrRegCh1L = 0x88;      // CH1 = infrared only
constexpr uint8_t kLtrRegPsL = 0x8D;       // proximity data (11 bit)

// --- BMI270 (Bosch) ----------------------------------------------------------
constexpr uint8_t kBmi270Addr = 0x69;
constexpr uint8_t kBmi270ChipId = 0x24;
constexpr uint8_t kBmiRegAccConf = BMI2_ACC_CONF_ADDR;   // 0x40
constexpr uint8_t kBmiRegGyrConf = BMI2_GYR_CONF_ADDR;   // 0x42
constexpr uint8_t kBmiRegDataStart = 0x0C;               // ACC_X_LSB .. GYR_Z_MSB (12 bytes)
constexpr size_t kBmiConfigFileSize = 8192;
constexpr size_t kBmiChunkSize = 32;
// ACC_CONF / GYR_CONF = range(0b00) << 6 | bwp(0b10) << 4 | odr(0x08)
//   accel  -> +/-2 g     -> 16384 LSB/g
//   gyroscope -> +/-2000 dps -> 16.4 LSB/dps
constexpr uint8_t kBmiAccGyrConf = 0x28;

}  // namespace

SensorMonitor& SensorMonitor::GetInstance() {
    static SensorMonitor instance;
    return instance;
}

bool SensorMonitor::WriteRegs(i2c_master_dev_handle_t dev, uint8_t reg, const uint8_t* data,
                              size_t len, int timeout_ms) {
    // Single transaction: send the register address followed by the payload.
    uint8_t buf[64];
    if (len + 1 > sizeof(buf)) {
        ESP_LOGE(TAG, "i2c write 0x%02X len=%u rejected: payload too big", reg,
                 static_cast<unsigned int>(len));
        return false;
    }
    buf[0] = reg;
    memcpy(buf + 1, data, len);
    // A bare false here told us nothing on the first hardware run, so name the
    // failing transfer: register, length and the driver error.
    const esp_err_t err = i2c_master_transmit(dev, buf, len + 1, timeout_ms);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c write reg 0x%02X len=%u failed: %s", reg,
                 static_cast<unsigned int>(len), esp_err_to_name(err));
        return false;
    }
    return true;
}

bool SensorMonitor::ReadRegs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t* data, size_t len,
                             int timeout_ms) {
    const esp_err_t err = i2c_master_transmit_receive(dev, &reg, 1, data, len, timeout_ms);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c read reg 0x%02X len=%u failed: %s", reg,
                 static_cast<unsigned int>(len), esp_err_to_name(err));
        return false;
    }
    return true;
}

void SensorMonitor::Initialize(Axp2101* pmic, i2c_master_bus_handle_t i2c_bus) {
    pmic_ = pmic;
    i2c_bus_ = i2c_bus;
    initialized_ = true;
    i2c_mutex_ = xSemaphoreCreateMutex();

    if (i2c_bus_ != nullptr) {
        i2c_device_config_t axp_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x34,
            .scl_speed_hz = 400000,
        };
        if (i2c_master_bus_add_device(i2c_bus_, &axp_cfg, &axp2101_dev_) == ESP_OK) {
            uint8_t adc_en = 0b111111;
            WriteRegs(axp2101_dev_, 0x30, &adc_en, 1, 100);
        }
        ltr553_available_ = InitLtr553();
        bmi270_available_ = InitBmi270();

        peripherals_.pmic_ok = (pmic_ != nullptr);
        peripherals_.light_ok = ltr553_available_;
        peripherals_.imu_ok = bmi270_available_;
        peripherals_.touch_ok = (i2c_master_probe(i2c_bus_, 0x38, pdMS_TO_TICKS(20)) == ESP_OK);
        peripherals_.amp_ok = (i2c_master_probe(i2c_bus_, 0x36, pdMS_TO_TICKS(20)) == ESP_OK);
        peripherals_.mic_adc_ok = (i2c_master_probe(i2c_bus_, 0x40, pdMS_TO_TICKS(20)) == ESP_OK);
        peripherals_.io_exp_ok = (i2c_master_probe(i2c_bus_, 0x58, pdMS_TO_TICKS(20)) == ESP_OK);
        peripherals_.rtc_ok = (i2c_master_probe(i2c_bus_, 0x51, pdMS_TO_TICKS(20)) == ESP_OK);

        int count = 0;
        if (peripherals_.pmic_ok) count++;
        if (peripherals_.imu_ok) count++;
        if (peripherals_.light_ok) count++;
        if (peripherals_.touch_ok) count++;
        if (peripherals_.amp_ok) count++;
        if (peripherals_.mic_adc_ok) count++;
        if (peripherals_.io_exp_ok) count++;
        if (peripherals_.rtc_ok) count++;
        peripherals_.total_online = count;
    }
    ESP_LOGI(TAG, "SensorMonitor ready: PMIC=%s LTR-553ALS=%s BMI270=%s (Total ICs online: %d/8)",
             pmic_ != nullptr ? "yes" : "no", ltr553_available_ ? "yes" : "no",
             bmi270_available_ ? "yes" : "no", peripherals_.total_online);
}

bool SensorMonitor::InitLtr553() {
    // i2c_master_bus_add_device() succeeds even for an address that never ACKs,
    // so probe first - otherwise a missing sensor looks initialised.
    if (i2c_master_probe(i2c_bus_, kLtr553Addr, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGW(TAG, "LTR-553ALS not found at 0x%02X", kLtr553Addr);
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = kLtr553Addr,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &ltr553_dev_) != ESP_OK) {
        ltr553_dev_ = nullptr;
        return false;
    }

    // Datasheet: the part needs ~100 ms after power-on before it answers I2C.
    vTaskDelay(pdMS_TO_TICKS(150));

    uint8_t contr = 0x02;  // software reset
    WriteRegs(ltr553_dev_, kLtrRegAlsContr, &contr, 1, 100);
    vTaskDelay(pdMS_TO_TICKS(20));

    contr = 0x01;  // active mode, gain 1x
    WriteRegs(ltr553_dev_, kLtrRegAlsContr, &contr, 1, 100);

    // repeat rate 200 ms, integration time 100 ms: fast enough that the 1 Hz
    // dashboard always reads a freshly measured sample.
    uint8_t meas_rate = (2 << 3) | 0;
    WriteRegs(ltr553_dev_, kLtrRegMeasRate, &meas_rate, 1, 100);

    uint8_t ps_contr = 0x03;  // proximity active
    WriteRegs(ltr553_dev_, kLtrRegPsContr, &ps_contr, 1, 100);

    ESP_LOGI(TAG, "LTR-553ALS ready at 0x%02X", kLtr553Addr);
    return true;
}

bool SensorMonitor::ReadLtr553(LightSensorData& data) {
    if (ltr553_dev_ == nullptr) return false;
    data.available = true;

    uint8_t status = 0;
    if (!ReadRegs(ltr553_dev_, kLtrRegStatus, &status, 1, 100)) {
        return false;
    }
    // bit2: new sample available, bit7: sample invalid (saturated / blacked out)
    if ((status & 0x04) == 0 || (status & 0x80) != 0) {
        return false;
    }

    uint8_t buf[4] = {0};
    if (!ReadRegs(ltr553_dev_, kLtrRegCh1L, buf, sizeof(buf), 100)) {
        return false;
    }
    uint16_t ch1 = buf[0] | (buf[1] << 8);  // infrared only
    uint16_t ch0 = buf[2] | (buf[3] << 8);  // visible + infrared

    if (ch0 == 0xFFFF || ch1 == 0xFFFF || (ch0 == 0 && ch1 == 0)) {
        data.lux = 0.0f;
        return false;
    }

    // Gain and integration time are whatever InitLtr553 programmed, not a
    // default: the conversion is only correct for the settings in force.
    data.lux = Ltr553Lux(ch0, ch1, kLtr553GainCount[(status >> 4) & 0x07],
                        static_cast<float>(kLtr553IntTimeMs[0]));

    uint8_t ps[2] = {0};
    if (ReadRegs(ltr553_dev_, kLtrRegPsL, ps, sizeof(ps), 100)) {
        data.proximity = ps[0] | ((ps[1] & 0x07) << 8);
    }
    return true;
}

static struct bmi2_dev s_bmi2_dev;

static BMI2_INTF_RETURN_TYPE Bmi2I2cRead(uint8_t reg_addr, uint8_t* reg_data, uint32_t len, void* intf_ptr) {
    if (!intf_ptr || !reg_data || len == 0) return BMI2_E_NULL_PTR;
    auto dev = static_cast<i2c_master_dev_handle_t>(intf_ptr);
    esp_err_t err = i2c_master_transmit_receive(dev, &reg_addr, 1, reg_data, len, 200);
    return err == ESP_OK ? BMI2_INTF_RET_SUCCESS : BMI2_E_COM_FAIL;
}

static BMI2_INTF_RETURN_TYPE Bmi2I2cWrite(uint8_t reg_addr, const uint8_t* reg_data, uint32_t len, void* intf_ptr) {
    if (!intf_ptr || !reg_data || len == 0) return BMI2_E_NULL_PTR;
    auto dev = static_cast<i2c_master_dev_handle_t>(intf_ptr);
    uint8_t buf[256];
    if (len + 1 > sizeof(buf)) return BMI2_E_COM_FAIL;
    buf[0] = reg_addr;
    memcpy(buf + 1, reg_data, len);
    esp_err_t err = i2c_master_transmit(dev, buf, len + 1, 300);
    return err == ESP_OK ? BMI2_INTF_RET_SUCCESS : BMI2_E_COM_FAIL;
}

static void Bmi2DelayUs(uint32_t period, void* intf_ptr) {
    esp_rom_delay_us(period);
}

bool SensorMonitor::InitBmi270() {
    if (i2c_master_probe(i2c_bus_, kBmi270Addr, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGW(TAG, "BMI270 not found at 0x%02X", kBmi270Addr);
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = kBmi270Addr,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &bmi270_dev_) != ESP_OK) {
        bmi270_dev_ = nullptr;
        return false;
    }

    memset(&s_bmi2_dev, 0, sizeof(s_bmi2_dev));
    s_bmi2_dev.chip_id = BMI270_CHIP_ID;
    s_bmi2_dev.intf = BMI2_I2C_INTF;
    s_bmi2_dev.read = Bmi2I2cRead;
    s_bmi2_dev.write = Bmi2I2cWrite;
    s_bmi2_dev.delay_us = Bmi2DelayUs;
    s_bmi2_dev.intf_ptr = bmi270_dev_;
    s_bmi2_dev.read_write_len = 30;
    s_bmi2_dev.config_file_ptr = bmi270_config_file;
    s_bmi2_dev.config_size = BMI270_CONFIG_FILE_SIZE;

    int8_t rslt = bmi2_sec_init(&s_bmi2_dev);
    if (rslt != BMI2_OK) {
        ESP_LOGE(TAG, "BMI270 bmi2_sec_init failed: %d", rslt);
        i2c_master_bus_rm_device(bmi270_dev_);
        bmi270_dev_ = nullptr;
        return false;
    }

    struct bmi2_sens_config config[2];
    config[0].type = BMI2_ACCEL;
    config[0].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
    config[0].cfg.acc.bwp = BMI2_ACC_OSR2_AVG2;
    config[0].cfg.acc.odr = BMI2_ACC_ODR_100HZ;
    config[0].cfg.acc.range = BMI2_ACC_RANGE_2G;

    config[1].type = BMI2_GYRO;
    config[1].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;
    config[1].cfg.gyr.noise_perf = BMI2_GYR_RANGE_2000;
    config[1].cfg.gyr.bwp = BMI2_GYR_OSR2_MODE;
    config[1].cfg.gyr.odr = BMI2_GYR_ODR_100HZ;
    config[1].cfg.gyr.range = BMI2_GYR_RANGE_2000;
    config[1].cfg.gyr.ois_range = BMI2_GYR_OIS_2000;

    bmi2_set_sensor_config(config, 2, &s_bmi2_dev);

    const uint8_t sens_list[2] = {BMI2_ACCEL, BMI2_GYRO};
    bmi2_sensor_enable(sens_list, 2, &s_bmi2_dev);

    ESP_LOGI(TAG, "BMI270 ready at 0x%02X (accel +/-2g, gyro +/-2000dps, 100 Hz)", kBmi270Addr);
    return true;
}

bool SensorMonitor::ReadBmi270(MotionSensorData& data) {
    if (bmi270_dev_ == nullptr) return false;
    data.available = true;

    struct bmi2_sens_data sens_data = {};
    int8_t rslt = bmi2_get_sensor_data(&sens_data, &s_bmi2_dev);
    if (rslt != BMI2_OK) {
        return false;
    }

    data.accel_x = static_cast<float>(sens_data.acc.x) / kBmi270AccLsbPerG;
    data.accel_y = static_cast<float>(sens_data.acc.y) / kBmi270AccLsbPerG;
    data.accel_z = static_cast<float>(sens_data.acc.z) / kBmi270AccLsbPerG;

    data.gyro_x = static_cast<float>(sens_data.gyr.x) / kBmi270GyrLsbPerDps;
    data.gyro_y = static_cast<float>(sens_data.gyr.y) / kBmi270GyrLsbPerDps;
    data.gyro_z = static_cast<float>(sens_data.gyr.z) / kBmi270GyrLsbPerDps;

    data.roll_deg = RollDegrees(data.accel_x, data.accel_y, data.accel_z);
    data.pitch_deg = PitchDegrees(data.accel_x, data.accel_y, data.accel_z);
    data.tilt_degrees = TiltDegrees(data.accel_x, data.accel_y, data.accel_z);
    data.posture = std::string(PostureFace(data.accel_x, data.accel_y, data.accel_z)) +
                   " - nghiêng " + std::to_string(static_cast<int>(data.tilt_degrees)) + "°";
    return true;
}

CoreS3SensorSnapshot SensorMonitor::GetSnapshot() {
    CoreS3SensorSnapshot snapshot;
    if (i2c_mutex_ != nullptr) {
        xSemaphoreTake(i2c_mutex_, pdMS_TO_TICKS(200));
    }

    // 1. Doc du lieu tu AXP2101 PMIC
    if (pmic_ != nullptr) {
        snapshot.power.battery_level = pmic_->GetBatteryLevel();
        snapshot.power.is_charging = pmic_->IsCharging();
        snapshot.power.is_discharging = pmic_->IsDischarging();
        snapshot.power.temperature_c = pmic_->GetTemperature();
    }
    if (axp2101_dev_ != nullptr) {
        uint8_t data[2] = {0};
        // 0x34: VBAT voltage
        if (ReadRegs(axp2101_dev_, 0x34, data, 2, 50)) {
            snapshot.power.vbat_mv = ((data[0] & 0x3F) << 8) | data[1];
        }
        // 0x38: VBUS voltage
        if (ReadRegs(axp2101_dev_, 0x38, data, 2, 50)) {
            snapshot.power.vbus_mv = ((data[0] & 0x3F) << 8) | data[1];
        }
        // 0x3A: VSYS voltage
        if (ReadRegs(axp2101_dev_, 0x3A, data, 2, 50)) {
            snapshot.power.vsys_mv = ((data[0] & 0x3F) << 8) | data[1];
        }
        // 0x3C: TDIE (internal temperature)
        if (ReadRegs(axp2101_dev_, 0x3C, data, 2, 50)) {
            snapshot.power.temperature_c = 22.0f + ((7274.0f - static_cast<float>((data[0] << 8) | data[1])) / 20.0f);
        }
        // 0x00: VBUS present bit 5
        uint8_t reg0 = 0;
        if (ReadRegs(axp2101_dev_, 0x00, &reg0, 1, 50)) {
            snapshot.power.vbus_present = (reg0 & 0b00100000) != 0;
            if (!snapshot.power.vbus_present || snapshot.power.vbus_mv >= 16375) {
                snapshot.power.vbus_mv = 0;
            }
        }
    }

    // 2. Doc du lieu tu LTR-553ALS va BMI270
    ReadLtr553(snapshot.light);
    ReadBmi270(snapshot.motion);

    if (i2c_mutex_ != nullptr) {
        xSemaphoreGive(i2c_mutex_);
    }

    // 3. Doc du lieu Mang Wi-Fi
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        snapshot.network.ssid = reinterpret_cast<char*>(ap_info.ssid);
        snapshot.network.rssi_dbm = ap_info.rssi;
        snapshot.network.channel = ap_info.primary;
    } else {
        snapshot.network.ssid = "Disconnected";
        snapshot.network.rssi_dbm = -100;
        snapshot.network.channel = 0;
    }

    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif != nullptr) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            char ip_str[32];
            esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
            snapshot.network.ip_address = ip_str;
        }
    }

    // 4. Doc du lieu He thong & Bo nho
    snapshot.system.free_sram_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    snapshot.system.free_psram_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    snapshot.system.min_sram_bytes = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    snapshot.system.uptime_seconds = static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);
    // CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ is the clock the firmware was built for;
    // there is no esp_clk_cpu_freq() in this IDF.
    snapshot.system.cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    snapshot.peripherals = peripherals_;

    return snapshot;
}

std::string SensorMonitor::GetAllSensorsJson() {
    CoreS3SensorSnapshot s = GetSnapshot();
    cJSON* root = cJSON_CreateObject();

    // Power & Battery
    cJSON* pwr = cJSON_CreateObject();
    cJSON_AddNumberToObject(pwr, "battery_level_percent", s.power.battery_level);
    cJSON_AddNumberToObject(pwr, "vbat_mv", s.power.vbat_mv);
    cJSON_AddNumberToObject(pwr, "vbus_mv", s.power.vbus_mv);
    cJSON_AddNumberToObject(pwr, "vsys_mv", s.power.vsys_mv);
    cJSON_AddBoolToObject(pwr, "vbus_present", s.power.vbus_present);
    cJSON_AddBoolToObject(pwr, "is_charging", s.power.is_charging);
    cJSON_AddBoolToObject(pwr, "is_discharging", s.power.is_discharging);
    cJSON_AddNumberToObject(pwr, "board_temperature_celsius", s.power.temperature_c);
    cJSON_AddItemToObject(root, "power_and_battery", pwr);

    // Network
    cJSON* net = cJSON_CreateObject();
    cJSON_AddStringToObject(net, "ssid", s.network.ssid.c_str());
    cJSON_AddNumberToObject(net, "rssi_dbm", s.network.rssi_dbm);
    cJSON_AddNumberToObject(net, "channel", s.network.channel);
    cJSON_AddStringToObject(net, "ip_address", s.network.ip_address.c_str());
    cJSON_AddItemToObject(root, "network", net);

    // Ambient Light & Proximity (LTR-553ALS)
    cJSON* light = cJSON_CreateObject();
    cJSON_AddBoolToObject(light, "available", s.light.available);
    if (s.light.available) {
        cJSON_AddNumberToObject(light, "ambient_lux", s.light.lux);
        cJSON_AddNumberToObject(light, "proximity_raw", s.light.proximity);
    }
    cJSON_AddItemToObject(root, "ambient_light", light);

    // Motion & Orientation (BMI270 IMU)
    cJSON* motion = cJSON_CreateObject();
    cJSON_AddBoolToObject(motion, "available", s.motion.available);
    if (s.motion.available) {
        cJSON* accel = cJSON_CreateObject();
        cJSON_AddNumberToObject(accel, "x_g", s.motion.accel_x);
        cJSON_AddNumberToObject(accel, "y_g", s.motion.accel_y);
        cJSON_AddNumberToObject(accel, "z_g", s.motion.accel_z);
        cJSON_AddItemToObject(motion, "accelerometer", accel);

        cJSON* gyro = cJSON_CreateObject();
        cJSON_AddNumberToObject(gyro, "x_dps", s.motion.gyro_x);
        cJSON_AddNumberToObject(gyro, "y_dps", s.motion.gyro_y);
        cJSON_AddNumberToObject(gyro, "z_dps", s.motion.gyro_z);
        cJSON_AddItemToObject(motion, "gyroscope", gyro);

        cJSON_AddNumberToObject(motion, "roll_deg", s.motion.roll_deg);
        cJSON_AddNumberToObject(motion, "pitch_deg", s.motion.pitch_deg);
        cJSON_AddNumberToObject(motion, "tilt_degrees", s.motion.tilt_degrees);
        cJSON_AddStringToObject(motion, "posture", s.motion.posture.c_str());
    }
    cJSON_AddItemToObject(root, "motion_imu", motion);

    // Ports and Peripherals
    cJSON* peri = cJSON_CreateObject();
    cJSON_AddNumberToObject(peri, "total_online_ics", s.peripherals.total_online);
    cJSON* ics = cJSON_CreateObject();
    cJSON_AddBoolToObject(ics, "axp2101_pmic", s.peripherals.pmic_ok);
    cJSON_AddBoolToObject(ics, "bmi270_imu", s.peripherals.imu_ok);
    cJSON_AddBoolToObject(ics, "ltr553_light", s.peripherals.light_ok);
    cJSON_AddBoolToObject(ics, "ft6336_touch", s.peripherals.touch_ok);
    cJSON_AddBoolToObject(ics, "aw88298_amp", s.peripherals.amp_ok);
    cJSON_AddBoolToObject(ics, "es7210_mic_adc", s.peripherals.mic_adc_ok);
    cJSON_AddBoolToObject(ics, "aw9523_io_exp", s.peripherals.io_exp_ok);
    cJSON_AddBoolToObject(ics, "bm8563_rtc", s.peripherals.rtc_ok);
    cJSON_AddItemToObject(peri, "internal_bus", ics);

    cJSON* ports = cJSON_CreateObject();
    cJSON_AddStringToObject(ports, "port_a", "I2C [GPIO 1 SCL, GPIO 2 SDA]");
    cJSON_AddStringToObject(ports, "port_b", "GPIO [GPIO 8, GPIO 9]");
    cJSON_AddStringToObject(ports, "port_c", "UART [GPIO 18 TX, GPIO 17 RX]");
    cJSON_AddItemToObject(peri, "grove_ports", ports);
    cJSON_AddItemToObject(root, "ports_and_peripherals", peri);

    // System Telemetry
    cJSON* sys = cJSON_CreateObject();
    cJSON_AddNumberToObject(sys, "free_sram_kb", s.system.free_sram_bytes / 1024);
    cJSON_AddNumberToObject(sys, "free_psram_mb", s.system.free_psram_bytes / (1024 * 1024));
    cJSON_AddNumberToObject(sys, "uptime_seconds", s.system.uptime_seconds);
    cJSON_AddNumberToObject(sys, "cpu_freq_mhz", s.system.cpu_freq_mhz);
    cJSON_AddItemToObject(root, "system", sys);

    char* str = cJSON_PrintUnformatted(root);
    std::string res = str ? str : "{}";
    cJSON_free(str);
    cJSON_Delete(root);
    return res;
}

std::string SensorMonitor::GetSensorDataJson(const std::string& sensor_type) {
    CoreS3SensorSnapshot s = GetSnapshot();
    cJSON* root = cJSON_CreateObject();

    if (sensor_type == "battery" || sensor_type == "power") {
        cJSON_AddNumberToObject(root, "battery_level_percent", s.power.battery_level);
        cJSON_AddBoolToObject(root, "is_charging", s.power.is_charging);
        cJSON_AddBoolToObject(root, "is_discharging", s.power.is_discharging);
        cJSON_AddStringToObject(root, "status", s.power.is_charging ? "Charging" : (s.power.battery_level > 20 ? "Normal" : "Low Battery"));
    } else if (sensor_type == "temperature") {
        cJSON_AddNumberToObject(root, "board_temperature_celsius", s.power.temperature_c);
        cJSON_AddStringToObject(root, "thermal_status", s.power.temperature_c < 45.0f ? "Cool/Normal" : "Warm");
    } else if (sensor_type == "light" || sensor_type == "lux" || sensor_type == "als") {
        cJSON_AddBoolToObject(root, "available", s.light.available);
        if (s.light.available) {
            cJSON_AddNumberToObject(root, "ambient_lux", s.light.lux);
            cJSON_AddNumberToObject(root, "proximity_raw", s.light.proximity);
            cJSON_AddStringToObject(root, "lighting_condition", s.light.lux < 50.0f ? "Dark/Dim" : (s.light.lux < 500.0f ? "Normal Indoor" : "Bright Indoor/Outdoor"));
        } else {
            cJSON_AddStringToObject(root, "status", "Sensor not detected or initializing");
        }
    } else if (sensor_type == "motion" || sensor_type == "imu" || sensor_type == "accel" || sensor_type == "gyro") {
        cJSON_AddBoolToObject(root, "available", s.motion.available);
        if (s.motion.available) {
            cJSON_AddNumberToObject(root, "accel_x", s.motion.accel_x);
            cJSON_AddNumberToObject(root, "accel_y", s.motion.accel_y);
            cJSON_AddNumberToObject(root, "accel_z", s.motion.accel_z);
            cJSON_AddNumberToObject(root, "gyro_x", s.motion.gyro_x);
            cJSON_AddNumberToObject(root, "gyro_y", s.motion.gyro_y);
            cJSON_AddNumberToObject(root, "gyro_z", s.motion.gyro_z);
            cJSON_AddNumberToObject(root, "tilt_degrees", s.motion.tilt_degrees);
            cJSON_AddStringToObject(root, "posture", s.motion.posture.c_str());
        } else {
            cJSON_AddStringToObject(root, "status", "Sensor not detected or initializing");
        }
    } else if (sensor_type == "network" || sensor_type == "wifi") {
        cJSON_AddStringToObject(root, "ssid", s.network.ssid.c_str());
        cJSON_AddNumberToObject(root, "rssi_dbm", s.network.rssi_dbm);
        cJSON_AddNumberToObject(root, "channel", s.network.channel);
        cJSON_AddStringToObject(root, "ip_address", s.network.ip_address.c_str());
        cJSON_AddStringToObject(root, "signal_quality", s.network.rssi_dbm > -60 ? "Excellent" : (s.network.rssi_dbm > -75 ? "Good" : "Fair"));
    } else if (sensor_type == "system" || sensor_type == "memory") {
        cJSON_AddNumberToObject(root, "free_sram_kb", s.system.free_sram_bytes / 1024);
        cJSON_AddNumberToObject(root, "free_psram_mb", s.system.free_psram_bytes / (1024 * 1024));
        cJSON_AddNumberToObject(root, "uptime_seconds", s.system.uptime_seconds);
        cJSON_AddNumberToObject(root, "cpu_freq_mhz", s.system.cpu_freq_mhz);
    } else {
        cJSON_AddStringToObject(root, "error", "Unknown sensor type. Available: battery, temperature, light, motion, network, system");
    }

    char* str = cJSON_PrintUnformatted(root);
    std::string res = str ? str : "{}";
    cJSON_free(str);
    cJSON_Delete(root);
    return res;
}
