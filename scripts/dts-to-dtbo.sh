#!/bin/bash
#
# Компилирует device-tree overlay source (.dts) в binary overlay (.dtbo)
# для последующего наложения через project::iq_forge::apply_fpga_overlay()
# (configfs, /sys/kernel/config/device-tree/overlays/).
#
# Источник сначала прогоняется через C-препроцессор (поддержка #include/#define
# в .dts, как принято в Linux/Xilinx device-tree файлах), затем через dtc -@
# (symbols для overlay resolution).

set -euo pipefail

show_usage() {
    echo "Usage: $0 [options] <input.dts> [output.dtbo]"
    echo ""
    echo "Options:"
    echo "  -I, --include <dir>   Директория для #include (можно указывать несколько раз)"
    echo "  -h, --help            Показать эту справку"
    echo ""
    echo "Arguments:"
    echo "  input.dts             Исходный device-tree overlay"
    echo "  output.dtbo           (опционально) Имя выходного файла (по умолчанию: <input>.dtbo)"
    echo ""
    echo "Examples:"
    echo "  $0 fpga.dts"
    echo "  $0 -I ./dt-include fpga.dts fpga.dtbo"
    exit 0
}

INCLUDE_DIRS=()

while [[ $# -gt 0 ]]; do
    case $1 in
        -I|--include)
            INCLUDE_DIRS+=("$2")
            shift 2
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
    echo "Error: Missing input file"
    echo ""
    show_usage
fi

INPUT_DTS="$1"

if [ ! -f "$INPUT_DTS" ]; then
    echo "Error: File '$INPUT_DTS' not found"
    exit 1
fi

if [ $# -ge 2 ]; then
    OUTPUT_DTBO="$2"
else
    OUTPUT_DTBO="${INPUT_DTS%.dts}.dtbo"
fi

if ! command -v dtc &> /dev/null; then
    echo "Error: dtc (device tree compiler) not found in PATH"
    echo "It ships with the Linux kernel source tree or a Xilinx/Vitis install"
    exit 1
fi

CPP_INCLUDE_ARGS=()
for dir in "${INCLUDE_DIRS[@]}"; do
    CPP_INCLUDE_ARGS+=(-I "$dir")
done

echo "Compiling device-tree overlay:"
echo "  Input:  $INPUT_DTS"
echo "  Output: $OUTPUT_DTBO"

PREPROCESSED="$(mktemp --suffix=.dts)"
trap 'rm -f "$PREPROCESSED"' EXIT

cpp -nostdinc -undef -x assembler-with-cpp "${CPP_INCLUDE_ARGS[@]}" "$INPUT_DTS" -o "$PREPROCESSED"

if ! dtc -@ -I dts -O dtb -o "$OUTPUT_DTBO" "$PREPROCESSED"; then
    echo "Error: dtc compilation failed"
    exit 1
fi

echo "Compilation completed successfully: $OUTPUT_DTBO"
