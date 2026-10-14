#!/bin/bash
# Convert PNG icons from assets/ to C files in resource/image/
# Uses LVGL v9's LVGLImage.py script.

LVGL_CONV="lvgl/scripts/LVGLImage.py"
ASSETS_DIR="assets"
OUTPUT_DIR="resource/image"
CF="ARGB8888"

# Mapping: Chinese filename -> English resource name
declare -A MAP
MAP[自行车.png]=bicycle
MAP[地图定位.png]=map_location
MAP[指南针.png]=compass
MAP[陀螺仪.png]=gyroscope
MAP[时间.png]=time_info
MAP[电池信息.png]=battery_info
MAP[储存.png]=storage
MAP[系统.png]=system_info
MAP[闹钟.png]=alarm
MAP[路程.png]=trip
MAP[arrow.png]=gps_arrow_default
MAP[arrow2.png]=gps_arrow_dark
MAP[arrow3.png]=gps_arrow_light
MAP[卫星.png]=satellite
MAP[SD卡.png]=sd_card
MAP[电池.png]=battery
MAP[定位.png]=locate
MAP[开始.png]=start
MAP[菜单.png]=menu
MAP[暂停.png]=pause
MAP[停止.png]=stop
MAP[原点.png]=origin_point
MAP[信息.png]=info

mkdir -p "$OUTPUT_DIR"

converted=0
skipped=0
for cn_name in "${!MAP[@]}"; do
    en_name="${MAP[$cn_name]}"
    input="$ASSETS_DIR/$cn_name"

    if [ ! -f "$input" ]; then
        echo "  [SKIP] $cn_name -> $en_name (not found)"
        skipped=$((skipped + 1))
        continue
    fi

    echo "  [CONV] $cn_name -> $en_name"
    python3 "$LVGL_CONV" \
        --ofmt C \
        --cf "$CF" \
        --output "$OUTPUT_DIR" \
        --name "$en_name" \
        "$input" > /dev/null 2>&1

    if [ $? -eq 0 ]; then
        converted=$((converted + 1))
    else
        echo "  [FAIL] $cn_name"
    fi
done

echo ""
echo "Done: $converted converted, $skipped skipped."
