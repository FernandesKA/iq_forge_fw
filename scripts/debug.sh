#!/bin/bash
#
# Собирает iq_forge_app в Debug (-g, без оптимизаций) кросс-тулчейном,
# подменяет им бинарь в уже задеплоенной директории на плате (см.
# scripts/load.sh --start — там уже лежат manifest.env/spi.json/bitstream/dtbo,
# так что достаточно перезалить только исполняемый файл) и стартует
# gdbserver в этой директории для удалённой отладки из VS Code
# (.vscode/launch.json подключается через miDebuggerServerAddress).
#
# Кросс-тулчейн для pluto_sky зафиксирован в
# /home/fka/toolchain/pluto_sky/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin
# (тот же, которым deploy.sh собирает Release для этого конфига) -
# переопредели через CC/CXX, если тулчейн другой.
#
# Использует отдельную build-debug/ (не build/), чтобы не сбивать кеш
# Release-сборки в build/ (та же ловушка с CC/CXX, что описана в deploy.sh).
#
# Usage: см. $0 --help

set -euo pipefail

DEFAULT_TC="/home/fka/toolchain/pluto_sky/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin"
CC="${CC:-$DEFAULT_TC/arm-linux-gcc}"
CXX="${CXX:-$DEFAULT_TC/arm-linux-g++}"

show_usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --host <ip>       Адрес платы [по умолчанию: 192.168.0.7]"
    echo "  --user <user>     SSH-пользователь [по умолчанию: root]"
    echo "  --port <port>     SSH-порт [по умолчанию: 22]"
    echo "  --dir <path>      Директория на плате с уже задеплоенным архивом"
    echo "                    (см. scripts/load.sh --start) [по умолчанию: /tmp/iq_forge]"
    echo "  --gdb-port <port> Порт gdbserver на плате [по умолчанию: 2345]"
    echo "  -h, --help        Показать эту справку"
    echo ""
    echo "Перед первым запуском залей архив на плату:"
    echo "  ./scripts/deploy.sh ./configs/pluto_sky/"
    echo "  ./scripts/load.sh --host 192.168.0.7 --start dist/pluto_sky.tar.gz"
    exit 0
}

HOST="192.168.0.7"
SSH_USER="root"
SSH_PORT="22"
REMOTE_DIR="/tmp/iq_forge"
GDB_PORT="2345"

while [[ $# -gt 0 ]]; do
    case $1 in
        --host)
            HOST="$2"
            shift 2
            ;;
        --user)
            SSH_USER="$2"
            shift 2
            ;;
        --port)
            SSH_PORT="$2"
            shift 2
            ;;
        --dir)
            REMOTE_DIR="$2"
            shift 2
            ;;
        --gdb-port)
            GDB_PORT="$2"
            shift 2
            ;;
        -h|--help)
            show_usage
            ;;
        *)
            echo "Error: Unknown option $1"
            show_usage
            ;;
    esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-debug"
SSH_TARGET="${SSH_USER}@${HOST}"

echo "Configuring Debug build ($BUILD_DIR)..."
CC="$CC" CXX="$CXX" cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug >/dev/null

echo "Building iq_forge_app (Debug)..."
cmake --build "$BUILD_DIR" --target iq_forge_app

APP_BIN="$BUILD_DIR/iq_forge_app"
echo "iq_forge_app: $(file -b "$APP_BIN")"

echo "Stopping any previous iq_forge_app/gdbserver on target..."
ssh -p "$SSH_PORT" "$SSH_TARGET" "pkill -f 'gdbserver.*iq_forge_app' 2>/dev/null; pkill -f './iq_forge_app' 2>/dev/null; true"

echo "Uploading debug binary to $SSH_TARGET:$REMOTE_DIR/iq_forge_app ..."
scp -P "$SSH_PORT" "$APP_BIN" "$SSH_TARGET:$REMOTE_DIR/iq_forge_app"
ssh -p "$SSH_PORT" "$SSH_TARGET" "chmod +x '$REMOTE_DIR/iq_forge_app'"

echo "Starting gdbserver :$GDB_PORT on target (cwd=$REMOTE_DIR) - waiting for debugger to attach..."
exec ssh -p "$SSH_PORT" "$SSH_TARGET" "cd '$REMOTE_DIR' && exec gdbserver :$GDB_PORT ./iq_forge_app"
