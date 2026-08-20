#!/bin/bash
#
# Заливает архив от scripts/deploy.sh на плату по scp и применяет его
# (fpga load + overlay apply) через ssh, запуская iq_forge_app из архива.
#
# Режимы:
#   --start       -> /tmp/iq_forge, применяется сразу, теряется после reboot.
#                     Для быстрой итерации при отладке.
#   --persistent  -> /opt/iq_forge/current, применяется сразу И переживает
#                     reboot: на плате должен быть установлен init-скрипт
#                     S99iq_forge (buildroot_custom board/zynq/RK-ZYNQ7020-F-IQFORGE/
#                     rootfs_overlay/etc/init.d/), который на старте просто
#                     запускает ./iq_forge_app из /opt/iq_forge/current.
#
# На таргете iq_forge_app без аргументов сам подхватывает manifest.env +
# spi.json из текущей директории и прогоняет всю цепочку (load-fpga +
# apply-overlay + vendor-id) - ни load.sh, ни init-скрипту не нужно знать
# про содержимое манифеста.
#
# Usage: см. $0 --help

set -euo pipefail

show_usage() {
    echo "Usage: $0 --host <ip> [--user <user>] [--port <port>] (--start|--persistent) <archive.tar.gz>"
    echo ""
    echo "Options:"
    echo "  --host <ip>       Адрес платы (обязательно)"
    echo "  --user <user>     SSH-пользователь [по умолчанию: root]"
    echo "  --port <port>     SSH-порт [по умолчанию: 22]"
    echo "  --start           Залить в /tmp и сразу применить (не переживает reboot)"
    echo "  --persistent      Залить в /opt/iq_forge/current и сразу применить (переживает reboot)"
    echo "  -h, --help        Показать эту справку"
    echo ""
    echo "Examples:"
    echo "  $0 --host 192.168.1.50 --start dist/rk7020f.tar.gz"
    echo "  $0 --host 192.168.1.50 --user root --persistent dist/rk7020f.tar.gz"
    exit 0
}

HOST=""
SSH_USER="root"
PORT="22"
MODE=""

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
            PORT="$2"
            shift 2
            ;;
        --start)
            MODE="start"
            shift
            ;;
        --persistent)
            MODE="persistent"
            shift
            ;;
        -h|--help)
            show_usage
            ;;
        -*)
            echo "Error: Unknown option $1"
            show_usage
            ;;
        *)
            break
            ;;
    esac
done

if [ -z "$HOST" ]; then
    echo "Error: --host is required"
    show_usage
fi
if [ -z "$MODE" ]; then
    echo "Error: --start or --persistent is required"
    show_usage
fi
if [ $# -lt 1 ]; then
    echo "Error: Missing archive path"
    show_usage
fi

ARCHIVE="$1"
if [ ! -f "$ARCHIVE" ]; then
    echo "Error: archive '$ARCHIVE' not found"
    exit 1
fi

SSH_TARGET="${SSH_USER}@${HOST}"

case "$MODE" in
    start)
        REMOTE_DIR="/tmp/iq_forge"
        REMOTE_ARCHIVE="/tmp/iq_forge.tar.gz"
        ;;
    persistent)
        REMOTE_DIR="/opt/iq_forge/current"
        REMOTE_ARCHIVE="/opt/iq_forge/current.tar.gz"
        ;;
esac

echo "Uploading $ARCHIVE to $SSH_TARGET:$REMOTE_ARCHIVE ..."
ssh -p "$PORT" "$SSH_TARGET" "mkdir -p '$(dirname "$REMOTE_ARCHIVE")'"
scp -P "$PORT" "$ARCHIVE" "$SSH_TARGET:$REMOTE_ARCHIVE"

echo "Extracting and applying on target ($MODE, dir=$REMOTE_DIR) ..."
ssh -p "$PORT" "$SSH_TARGET" bash -s -- "$REMOTE_DIR" "$REMOTE_ARCHIVE" << 'REMOTE_SCRIPT'
set -e
DIR="$1"
ARCHIVE="$2"

rm -rf "$DIR"
mkdir -p "$DIR"
tar -xzf "$ARCHIVE" -C "$DIR"
rm -f "$ARCHIVE"

cd "$DIR"
chmod +x iq_forge_app
./iq_forge_app
REMOTE_SCRIPT

if [ "$MODE" = "start" ]; then
    echo ""
    echo "Applied from $REMOTE_DIR (transient - lost on reboot)."
else
    echo ""
    echo "Applied from $REMOTE_DIR and persisted."
    echo "Auto-apply on boot requires the S99iq_forge init script on the target"
    echo "(buildroot_custom board/zynq/RK-ZYNQ7020-F-IQFORGE/rootfs_overlay/etc/init.d/)."
fi
