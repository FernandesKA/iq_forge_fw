#!/bin/bash
#
# Собирает деплой-архив из папки конфига (configs/<name>/): iq_forge_app +
# сконвертированные .bit->.bin и .dts->.dtbo + manifest.env. iq_forge_app
# берётся из `cmake --install` (dist/bin/), архив кладётся туда же: dist/<name>.tar.gz.
#
# Для кросс-компиляции под таргет (ARM Zynq) нужно перед запуском задать CC/CXX
# на кросс-тулчейн, например уже собранный buildroot_custom (собран под ровно
# наш zynq_rk7020f_iqforge_defconfig, arm-buildroot-linux-gnueabihf):
#
#   TC=/home/fka/dev_linux/buildroot_custom/buildroot/output/host/bin
#   export CC="$TC/arm-linux-gcc" CXX="$TC/arm-linux-g++"
#
# Компилятор кешируется в build/ при первой конфигурации - если до этого уже
# собирали под хост (или другой тулчейн) в тот же build/, снеси его перед
# переключением (rm -rf build dist), иначе новый CC/CXX не подхватится (та же
# ловушка, что была с CMAKE_INSTALL_PREFIX). Итоговая архитектура бинаря
# печатается в конце - проверяй её перед деплоем на плату.
#
# Usage: см. $0 --help

set -euo pipefail

show_usage() {
    echo "Usage: $0 [options] <config-dir>"
    echo ""
    echo "Options:"
    echo "  -o, --overlay-name <name>  Имя overlay в configfs [по умолчанию: имя папки конфига]"
    echo "  -a, --arch <arch>          Архитектура для bootgen: zynq, zynqmp [по умолчанию: zynq]"
    echo "  --skip-build                Не пересобирать/переустанавливать iq_forge_app, взять готовый из dist/bin/"
    echo "  -h, --help                  Показать эту справку"
    echo ""
    echo "Arguments:"
    echo "  config-dir                  Папка конфига, например configs/rk7020f"
    echo "                               Должна содержать ровно один *.bit и один *.dts"
    echo ""
    echo "Examples:"
    echo "  $0 configs/rk7020f"
    echo "  $0 --overlay-name fpga0 configs/rk7020f"
    exit 0
}

OVERLAY_NAME=""
ARCH="zynq"
SKIP_BUILD=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -o|--overlay-name)
            OVERLAY_NAME="$2"
            shift 2
            ;;
        -a|--arch)
            ARCH="$2"
            shift 2
            ;;
        --skip-build)
            SKIP_BUILD=1
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

if [ $# -lt 1 ]; then
    echo "Error: Missing config-dir"
    echo ""
    show_usage
fi

CONFIG_DIR="$1"
if [ ! -d "$CONFIG_DIR" ]; then
    echo "Error: '$CONFIG_DIR' is not a directory"
    exit 1
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_DIR="$(cd "$CONFIG_DIR" && pwd)"
CONFIG_NAME="$(basename "$CONFIG_DIR")"
DIST_DIR="$REPO_ROOT/dist"
ARCHIVE="$DIST_DIR/${CONFIG_NAME}.tar.gz"

if [ -z "$OVERLAY_NAME" ]; then
    OVERLAY_NAME="$CONFIG_NAME"
fi

mapfile -t BIT_FILES < <(find "$CONFIG_DIR" -maxdepth 1 -name '*.bit')
if [ ${#BIT_FILES[@]} -ne 1 ]; then
    echo "Error: expected exactly one *.bit file in $CONFIG_DIR, found ${#BIT_FILES[@]}"
    exit 1
fi
BIT_FILE="${BIT_FILES[0]}"

mapfile -t DTS_FILES < <(find "$CONFIG_DIR" -maxdepth 1 -name '*.dts')
if [ ${#DTS_FILES[@]} -ne 1 ]; then
    echo "Error: expected exactly one *.dts file in $CONFIG_DIR, found ${#DTS_FILES[@]}"
    exit 1
fi
DTS_FILE="${DTS_FILES[0]}"

BUILD_DIR="$REPO_ROOT/build"
if [ "$SKIP_BUILD" -eq 0 ]; then
    echo "Building and installing iq_forge_app..."
    cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$DIST_DIR" >/dev/null
    cmake --build "$BUILD_DIR" --target iq_forge_app
    cmake --install "$BUILD_DIR" --prefix "$DIST_DIR"
fi

APP_BIN="$DIST_DIR/bin/iq_forge_app"
if [ ! -f "$APP_BIN" ]; then
    echo "Error: $APP_BIN not found. Build+install it first (drop --skip-build) or run:"
    echo "  cmake --build $BUILD_DIR --target iq_forge_app && cmake --install $BUILD_DIR --prefix $DIST_DIR"
    exit 1
fi

echo "iq_forge_app: $(file -b "$APP_BIN")"

mkdir -p "$DIST_DIR"
STAGE_DIR="$(mktemp -d)"
trap 'rm -rf "$STAGE_DIR"' EXIT

BIN_NAME="${CONFIG_NAME}.bin"
DTBO_NAME="${CONFIG_NAME}.dtbo"

echo "Converting bitstream ($ARCH): $BIT_FILE -> $BIN_NAME"
"$REPO_ROOT/scripts/bit-to-bin.sh" --arch "$ARCH" "$BIT_FILE" "$STAGE_DIR/$BIN_NAME"

echo "Compiling overlay: $DTS_FILE -> $DTBO_NAME"
"$REPO_ROOT/scripts/dts-to-dtbo.sh" "$DTS_FILE" "$STAGE_DIR/$DTBO_NAME"

cp "$APP_BIN" "$STAGE_DIR/iq_forge_app"

if [ -f "$CONFIG_DIR/spi.json" ]; then
    echo "Including spi.json"
    cp "$CONFIG_DIR/spi.json" "$STAGE_DIR/spi.json"
fi

GPIO_BASE_LINE=""
if [ -f "$CONFIG_DIR/ad9361_ctrl_gpio_base" ]; then
    GPIO_BASE="$(tr -d '[:space:]' < "$CONFIG_DIR/ad9361_ctrl_gpio_base")"
    echo "Including AD9361 control GPIO base: $GPIO_BASE"
    GPIO_BASE_LINE="AD9361_CTRL_GPIO_BASE=$GPIO_BASE"
fi

cat > "$STAGE_DIR/manifest.env" << EOF
BITSTREAM=$BIN_NAME
DTBO=$DTBO_NAME
OVERLAY_NAME=$OVERLAY_NAME
$GPIO_BASE_LINE
EOF

tar -czf "$ARCHIVE" -C "$STAGE_DIR" .

echo ""
echo "Deploy archive ready: $ARCHIVE"
echo "  iq_forge_app, $BIN_NAME, $DTBO_NAME, manifest.env (overlay-name=$OVERLAY_NAME)"
