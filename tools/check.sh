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
    "bsp_pmu_config_t",
    "BSP_PMU_CONFIG_DEFAULT",
    "vbus_present",
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
    "pcf85063_",
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

echo
echo "== xtrack app boundary check =="

python3 - <<'PY'
from pathlib import Path
import re
import sys

# xtrack/ 是平台无关的应用层: 不允许出现 SDL, ESP-IDF 或 BSP 头文件;
# 平台差异只允许存在于 sim/ (PC 后端) 和 main/ (板侧后端).

forbidden = [
    (re.compile(r'#\s*include\s*[<"]SDL'), "SDL"),
    (re.compile(r'#\s*include\s*[<"]esp_'), "ESP-IDF (esp_*)"),
    (re.compile(r'#\s*include\s*[<"]driver/'), "ESP-IDF driver/*"),
    (re.compile(r'#\s*include\s*[<"]freertos/'), "FreeRTOS"),
    (re.compile(r'#\s*include\s*[<"]soc/'), "ESP-IDF soc/*"),
    (re.compile(r'#\s*include\s*[<"]bsp_'), "BSP (bsp_*)"),
    (re.compile(r'#\s*include\s*[<"]hal_sdl'), "sim-only hal_sdl"),
]

text_suffixes = {".c", ".h", ".inc"}
skip_dirs = {"third_party", "build"}

failed = False
for path in sorted(Path("xtrack").rglob("*")):
    if not path.is_file() or path.suffix not in text_suffixes:
        continue
    if any(part in skip_dirs for part in path.parts):
        continue
    text = path.read_text(errors="ignore")
    for regex, name in forbidden:
        for match in regex.finditer(text):
            print(f"xtrack boundary violation: {path}: {name}: {match.group(0).strip()}")
            failed = True

if failed:
    sys.exit(1)

print("xtrack app boundary check passed")
PY

echo
echo "== retired name check =="

python3 - <<'PY'
from pathlib import Path
import sys

# 已被替换的旧名字, 不允许再出现在仓库文本里 (check.sh 自身的列表除外).
retired = ["xtrack_app", "xtrack_hal_board"]

text_suffixes = {
    ".c",
    ".h",
    ".inc",
    ".md",
    ".txt",
    ".sh",
    ".yml",
    ".yaml",
    ".cmake",
    ".json",
}
skip_dirs = {".git", ".cache", "build", "managed_components", "third_party", "tmp"}
skip_prefixes = ("docs/code",)

failed = False
for path in sorted(Path(".").rglob("*")):
    if not path.is_file():
        continue
    if any(part in skip_dirs for part in path.parts):
        continue
    if str(path).startswith(skip_prefixes):
        continue
    if path.name == "check.sh":
        continue
    if path.suffix not in text_suffixes and path.name != "CMakeLists.txt":
        continue
    text = path.read_text(errors="ignore")
    for name in retired:
        if name in text:
            print(f"retired name: {path}: {name}")
            failed = True

if failed:
    sys.exit(1)

print("retired name check passed")
PY

echo
echo "== public header include boundary check =="

python3 - <<'PY'
from pathlib import Path
import re
import sys

public_headers = Path("components/bsp/include")
header_names = {path.name for path in public_headers.glob("*.h")}

# Always allowed in a public BSP header: esp_err.h and the C standard library.
allowed_global = {"esp_err.h"}
stdlib_prefixes = ("std", "sys/")

# The only named escape hatches allowed to pull in an external type header.
allowed_external = {
    "bsp_i2c.h": {"driver/i2c_master.h"},
    "bsp_ui.h": {"lvgl.h"},
}

include_re = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)

failed = False
for path in sorted(public_headers.glob("*.h")):
    allowed_here = allowed_global | allowed_external.get(path.name, set())
    for include in include_re.findall(path.read_text(errors="ignore")):
        if include in allowed_here:
            continue
        if include.startswith(stdlib_prefixes) or include in header_names:
            continue
        print(f"public header include not allowed: {path}: {include}")
        failed = True

if failed:
    sys.exit(1)

print("public header include boundary check passed")
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

if [ "${CHECK_SIM:-0}" = "1" ]; then
    echo
    echo "== sim build =="
    cmake -S sim -B sim/build -G Ninja
    cmake --build sim/build
fi

echo
echo "All checks passed."
