#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: ./bsp.sh <app> <auras3|aura|doers3|doer> [build|flash|monitor|menuconfig|clean|reconfigure ...]"
}

APP="${1:-}"
BOARD="${2:-}"
ACTIONS=("${@:3}")

if [ -z "$APP" ] || [ -z "$BOARD" ]; then
    usage
    exit 2
fi

if [ ${#ACTIONS[@]} -eq 0 ]; then
    ACTIONS=(build)
fi

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$ROOT/../../.." && pwd)
APP_DIR="$ROOT/$APP"

if [ ! -d "$APP_DIR" ]; then
    echo "unknown app: $APP" >&2
    exit 2
fi

case "$BOARD" in
    auras3|aura)
        BOARD_NAME="auras3"
        BOARD_CONFIG="CONFIG_BSP_BOARD_AURAS3=y"
        EXTRA_CONFIG="CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y"
        ;;
    doers3|doer)
        BOARD_NAME="doers3"
        BOARD_CONFIG="CONFIG_BSP_BOARD_DOERS3=y"
        EXTRA_CONFIG=""
        ;;
    *)
        echo "unknown board: $BOARD" >&2
        usage
        exit 2
        ;;
esac

cd "$APP_DIR"

if ! command -v idf.py >/dev/null 2>&1; then
    IDF_EXPORT="$HOME/esp/esp-idf/export.sh"
    if [ -f "$IDF_EXPORT" ]; then
        source "$IDF_EXPORT" >/dev/null
    fi
fi

current_board() {
    if [ ! -f sdkconfig ]; then
        return 0
    fi
    if grep -q '^CONFIG_BSP_BOARD_AURAS3=y$' sdkconfig; then
        echo "auras3"
    elif grep -q '^CONFIG_BSP_BOARD_DOERS3=y$' sdkconfig; then
        echo "doers3"
    fi
}

write_sdkconfig() {
    cp sdkconfig.defaults sdkconfig
    {
        echo ""
        echo "# Board selected by ../bsp.sh"
        echo "$BOARD_CONFIG"
        if [ -n "$EXTRA_CONFIG" ]; then
            echo "$EXTRA_CONFIG"
        fi
    } >> sdkconfig
}

ensure_sdkconfig() {
    LAST_BOARD=$(current_board)
    if [ -n "$LAST_BOARD" ] && [ "$LAST_BOARD" != "$BOARD_NAME" ]; then
        echo "board changed: $LAST_BOARD -> $BOARD_NAME, resetting sdkconfig/build"
        rm -f sdkconfig
        rm -rf build
    fi

    if [ ! -f sdkconfig ]; then
        if [ -f build/CMakeCache.txt ] && grep -q 'sdkconfig.generated' build/CMakeCache.txt; then
            echo "legacy generated config detected, resetting build"
            rm -rf build
        fi
        write_sdkconfig
    fi
}

ensure_managed_components_link() {
    mkdir -p "$REPO_ROOT/managed_components"
    if [ -L managed_components ]; then
        return 0
    fi
    if [ -e managed_components ]; then
        echo "resetting app-local managed_components to repo-root link"
        rm -rf managed_components
    fi
    ln -s "$REPO_ROOT/managed_components" managed_components
}

run_idf() {
    ensure_managed_components_link
    idf.py -B build "${ACTIONS[@]}"
}

if [ ${#ACTIONS[@]} -eq 1 ] && [ "${ACTIONS[0]}" = "clean" ]; then
    rm -f sdkconfig sdkconfig.generated sdkconfig.board.generated .bsp_board dependencies.lock
    rm -rf build managed_components
    exit 0
fi

for action in "${ACTIONS[@]}"; do
    case "$action" in
        build|flash|monitor|menuconfig|reconfigure|fullclean|erase-flash|size|size-components|size-files)
            ;;
        clean)
            echo "clean must be used alone" >&2
            exit 2
            ;;
        *)
            echo "unknown action: $action" >&2
            usage
            exit 2
            ;;
    esac
done

ensure_sdkconfig
run_idf
