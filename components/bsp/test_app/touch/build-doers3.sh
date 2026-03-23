#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$SCRIPT_DIR"

exec idf.py \
    -D SDKCONFIG=sdkconfig.board_doers3 \
    -D SDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.board_doers3.defaults' \
    -B build.board_doers3 \
    "$@"
