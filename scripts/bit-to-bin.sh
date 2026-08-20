#!/bin/bash
#
# Тонкая обёртка над external/fpga_loader/scripts/bit-to-bin.sh,
# чтобы .bit -> .bin конвертация была доступна из корневого scripts/
# без дублирования логики bootgen.
#
# Usage: см. $0 --help

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UPSTREAM_SCRIPT="$SCRIPT_DIR/../external/fpga_loader/scripts/bit-to-bin.sh"

if [ ! -f "$UPSTREAM_SCRIPT" ]; then
    echo "Error: $UPSTREAM_SCRIPT not found (submodule external/fpga_loader not initialized?)"
    echo "Run: git submodule update --init --recursive"
    exit 1
fi

exec "$UPSTREAM_SCRIPT" "$@"
