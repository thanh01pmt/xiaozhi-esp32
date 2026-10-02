#!/bin/bash
set -e

# ==============================================================================
# Script tự động hoá Build, Flash, Kiểm tra và Monitor cho M5Stack CoreS3 (XiaoZhi)
# ==============================================================================

# Tìm cổng USB mặc định nếu không truyền tham số
PORT="${1:-/dev/cu.usbmodem2101}"
ACTION="${2:-all}" # all | build | flash | verify | monitor

# Ngôn ngữ không nằm trong config.json của board (repo cấm CONFIG_LANGUAGE_*
# trong sdkconfig_append) mà là tham số lúc build. Xem --list-languages.
LANGUAGE="${LANGUAGE:-vi-VN}"

# Bao lâu chờ dòng "SensorMonitor ready" sau khi reset (giây)
VERIFY_TIMEOUT="${VERIFY_TIMEOUT:-25}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=========================================================="
echo " XiaoZhi ESP32 - M5Stack CoreS3 Toolchain Helper"
echo " Project: $PROJECT_ROOT"
echo " Port:    $PORT"
echo " Action:  $ACTION"
echo " Language: $LANGUAGE"
echo "=========================================================="

# 1. Kích hoạt môi trường ESP-IDF nếu chưa nạp
if ! command -v idf.py &> /dev/null; then
    if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
        echo "[INFO] Sourcing ESP-IDF environment from ~/esp/esp-idf/export.sh..."
        source "$HOME/esp/esp-idf/export.sh"
    else
        echo "[ERROR] Không tìm thấy ~/esp/esp-idf/export.sh. Vui lòng kiểm tra lại đường dẫn cài đặt ESP-IDF."
        exit 1
    fi
fi

cd "$PROJECT_ROOT"

# 2. Thực hiện Build
if [ "$ACTION" = "all" ] || [ "$ACTION" = "build" ]; then
    echo ""
    echo "[1/4] Biên dịch firmware cho M5Stack CoreS3..."
    python3 scripts/build.py m5stack/core-s3 --name m5stack-core-s3 --language "$LANGUAGE"
    echo "[SUCCESS] Biên dịch thành công!"
fi

# 3. Thực hiện Flash
if [ "$ACTION" = "all" ] || [ "$ACTION" = "flash" ]; then
    echo ""
    echo "[2/4] Nạp firmware xuống thiết bị qua cổng $PORT..."
    python3 -m esptool --chip esp32s3 -p "$PORT" -b 460800 \
        --before default-reset --after hard-reset write-flash \
        --flash-mode dio --flash-size 16MB --flash-freq 80m \
        0x0 build/bootloader/bootloader.bin \
        0x8000 build/partition_table/partition-table.bin \
        0xd000 build/ota_data_initial.bin \
        0x20000 build/xiaozhi.bin \
        0x800000 build/generated_assets.bin
    echo "[SUCCESS] Nạp firmware thành công!"
fi

# 4. Đọc log serial và đối chiếu kết quả khởi tạo cảm biến
#
# SensorMonitor::Initialize() chạy trong Board constructor, tức là vài giây sau
# khi boot (sau khi dựng PSRAM, màn hình, camera). Dòng cần tìm do chính driver
# in ra: "SensorMonitor ready: PMIC=yes LTR-553ALS=yes BMI270=yes".
if [ "$ACTION" = "all" ] || [ "$ACTION" = "verify" ]; then
    echo ""
    echo "[3/4] Đặt máy về chế độ chạy để lấy log khởi động..."
    python3 -m esptool --chip esp32s3 -p "$PORT" \
        --before default-reset --after hard-reset chip_id > /dev/null

    echo "[3/4] Dò log cảm biến (tối đa ${VERIFY_TIMEOUT}s)..."
    python3 - "$PORT" "$VERIFY_TIMEOUT" <<'PY'
import re, sys, time
import serial

port, timeout = sys.argv[1], float(sys.argv[2])

# Một dòng duy nhất cho biết trạng thái cả ba cảm biến.
READY = re.compile(r"SensorMonitor ready: PMIC=(\S+) LTR-553ALS=(\S+) BMI270=(\S+)")
# Các dòng lỗi để giải thích vì sao fail.
HINTS = ("not found at", "chip id mismatch", "config upload failed", "config load failed")
# Bỏ mức độ + timestamp của ESP_LOG: "W (3100) SensorMonitor: BMI270 not found"
NOISE = re.compile(r"^[EWIDV] \(\d+\) ")


def explain(line):
    return NOISE.sub("", line)


text = ""
with serial.Serial(port, 115200, timeout=0.5) as link:
    deadline = time.time() + timeout
    while time.time() < deadline:
        chunk = link.read(256)
        if not chunk:
            continue
        text += chunk.decode("utf-8", "replace")
        if READY.search(text):
            break

match = READY.search(text)
print("=" * 58)
print(" Kết quả khởi tạo cảm biến")
print("-" * 58)
if match is None:
    print(f" FAIL  không thấy dòng 'SensorMonitor ready' sau {timeout:.0f}s")
    print("-" * 58)
    for line in text.splitlines():
        if any(hint in line for hint in HINTS):
            print("  •", explain(line))
    print("\n  %d dòng log cuối:" % len(text.splitlines()))
    for line in text.splitlines()[-20:]:
        print("   ", explain(line))
    sys.exit(1)

failed = []
for name, state in zip(("PMIC (AXP2101)", "LTR-553ALS (light)", "BMI270 (imu)"), match.groups()):
    ok = state == "yes"
    failed += [] if ok else [name]
    print(f" {'OK  ' if ok else 'FAIL'}  {name:<20} : {state}")

if failed:
    print("-" * 58)
    for line in text.splitlines():
        if any(hint in line for hint in HINTS):
            print("  •", explain(line))
print("=" * 58)
print(" Dòng gốc:", match.group(0))
sys.exit(1 if failed else 0)
PY
    echo "[SUCCESS] Cảm biến đã sẵn sàng."
fi

# 5. Thực hiện Monitor
if [ "$ACTION" = "monitor" ]; then
    echo ""
    echo "[4/4] Khởi động Serial Monitor (Nhấn Ctrl + ] để thoát)..."
    idf.py -p "$PORT" monitor
elif [ "$ACTION" = "all" ]; then
    echo ""
    echo "[Gợi ý] Mở monitor thủ công: ./docs/setup/flash_cores3.sh $PORT monitor"
fi
