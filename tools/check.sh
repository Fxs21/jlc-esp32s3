#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

echo "== git diff whitespace check =="
git diff --check

echo
echo "== old BSP symbol residue check =="

python3 - <<'PY'
from pathlib import Path
import sys

scan_roots = [
    Path("components/bsp"),
    Path("main"),
]

old_symbols = [
    "bsp_imu_new",
    "bsp_sdcard_card",
    "bsp_display_draw_bitmap",
    "bsp_display_register_flush_done_cb",
    "bsp_display_flush_done_cb_t",
    "BSP_AUDIO_PROFILE_",
    "BSP_AUDIO_DIR_",
    "bsp_audio_start",
    "bsp_audio_stop",
    "esp_lcd_touch_ft5x06",
    "ft5x06",
]

bad_public_types = [
    "sdmmc_card_t",
    "camera_fb_t",
    "esp_lcd_touch_handle_t",
    "esp_lcd_panel_handle_t",
    "i2s_chan_handle_t",
    "qmi8658_",
    "pca9557_",
]

text_suffixes = {
    ".c",
    ".cc",
    ".cpp",
    ".h",
    ".hpp",
    ".md",
    ".txt",
    ".yml",
    ".yaml",
    ".cmake",
}

skip_dirs = {".git", "build", "managed_components"}


def iter_text_files(root: Path):
    if not root.exists():
        return
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if any(part in skip_dirs for part in path.parts):
            continue
        if path.suffix.lower() in text_suffixes or path.name == "CMakeLists.txt":
            yield path


failed = False

for root in scan_roots:
    for path in iter_text_files(root):
        text = path.read_text(errors="ignore")
        for symbol in old_symbols:
            if symbol in text:
                print(f"old BSP symbol: {path}: {symbol}")
                failed = True

public_headers = Path("components/bsp/include")
for path in iter_text_files(public_headers):
    text = path.read_text(errors="ignore")
    for symbol in bad_public_types:
        if symbol in text:
            print(f"public BSP API leaks external/private type: {path}: {symbol}")
            failed = True

if failed:
    sys.exit(1)

print("BSP residue check passed")
PY

echo
echo "== component dependency check =="

python3 - <<'PY'
from pathlib import Path
import sys

manifests = [
    Path("components/bsp/idf_component.yml"),
    Path("main/idf_component.yml"),
]

forbidden_dependencies = [
    "esp_lcd_touch_ft5x06",
    "esp_lcd_touch",
    "esp_codec_dev",
    "waveshare/qmi8658",
    "esp-idf-lib/pca9557",
    "esp-idf-lib/i2cdev",
    "esp-idf-lib/esp_idf_lib_helpers",
]

failed = False
for path in manifests:
    if not path.exists():
        continue
    text = path.read_text(errors="ignore")
    for dependency in forbidden_dependencies:
        if dependency in text:
            print(f"forbidden dependency: {path}: {dependency}")
            failed = True

if failed:
    sys.exit(1)

print("dependency check passed")
PY

if [ "${CHECK_BUILD:-0}" = "1" ]; then
    echo
    echo "== idf.py build =="
    if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
        # shellcheck disable=SC1090
        source "$HOME/esp/esp-idf/export.sh"
    fi
    idf.py build
fi

echo
echo "All checks passed."
