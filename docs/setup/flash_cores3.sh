#!/bin/bash
set -e

# ==============================================================================
# Script tự động hoá Build, Flash và Monitor cho M5Stack CoreS3 (XiaoZhi)
# ==============================================================================

# Tìm cổng USB mặc định nếu không truyền tham số
PORT="${1:-/dev/cu.usbmodem2101}"
ACTION="${2:-all}" # all | build | flash | monitor

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=========================================================="
echo " XiaoZhi ESP32 - M5Stack CoreS3 Toolchain Helper"
echo " Project: $PROJECT_ROOT"
echo " Port:    $PORT"
echo " Action:  $ACTION"
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
    echo "[1/3] Biên dịch firmware cho M5Stack CoreS3..."
    python3 scripts/build.py m5stack/core-s3 --name m5stack-core-s3
    echo "[SUCCESS] Biên dịch thành công!"
fi

# 3. Thực hiện Flash
if [ "$ACTION" = "all" ] || [ "$ACTION" = "flash" ]; then
    echo ""
    echo "[2/3] Nạp firmware xuống thiết bị qua cổng $PORT..."
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

# 4. Thực hiện Monitor
if [ "$ACTION" = "all" ] || [ "$ACTION" = "monitor" ]; then
    echo ""
    echo "[3/3] Khởi động Serial Monitor (Nhấn Ctrl + ] để thoát)..."
    idf.py -p "$PORT" monitor
fi
