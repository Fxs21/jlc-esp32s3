# xtrack-app

X-TRACK 骑行码表应用,用 C 重写。组织方式:**平台无关的 App 层 + 每个平台一份 HAL 实现**。

目标是同一份 App 代码同时跑在两个后端:

| 后端 | 构建入口 | 产物 |
|---|---|---|
| PC 模拟器(SDL2) | 本仓库根 `CMakeLists.txt` | `build/lvgl-sim` |
| ESP32-S3 开发板 | 另一个仓库 `jlc-esp32s3`(BSP + 板级工程) | `main.bin` |

App 层在本仓库以 IDF 组件的形式暴露,见 `components/xtrack_app/`;板子工程通过组件管理器引用它,所以两边编的是**同一份源码**,不存在拷贝。

## 目录职责

| 路径 | 归属 | 说明 |
|---|---|---|
| `app.c` `app.h` | App | 初始化顺序:DataCenter -> DataProc -> StatusBar -> PageManager |
| `page_manager/` | App | 页面栈、状态机、动画、拖动返回(对应上游的 PageManager) |
| `pages/` | App | startup / dialplate / livemap / system_infos / status_bar |
| `data_center/` | App | 账号式发布订阅总线(含双缓冲) |
| `data_proc/` | App | 中间层节点:clock / gps / power,把 HAL 数据推给 DataCenter |
| `resource/` | App | `resource_pool.c` + `image/*.c`(图标,生成物) |
| `utils/` | App | `log.h`、`ds/`(vector、dbl_buf) |
| `hal/hal.h` `hal_clock.h` `hal_gps.h` `hal_power.h` | HAL 接口 | 平台无关的接口声明 |
| `hal/hal_clock.c` `hal_gps.c` `hal_power.c` `hal_init.c` | HAL 实现 | **PC 侧**实现,目前是模拟数据 |
| `main.c` `hal/hal_sdl.c` `hal/hal_sdl.h` `lv_conf.h` | PC 专用 | SDL 显示/输入、PC 侧 LVGL 配置 |
| `components/xtrack_app/` | 构建 | IDF 组件包装(源码清单 + include 路径),不含逻辑 |
| `assets/` | 资源源文件 | 图标 PNG/AI/TTF + `convert_images.sh` |
| `third_party/` | 依赖 | `lvgl`(submodule)、`uthash`(vendored) |

板子侧的 HAL 实现在 `jlc-esp32s3/components/xtrack_hal_board/`,目前同样是模拟数据,每一处都标了 `TODO(real)` 指向将来要接的 BSP 能力(GNSS / PMU / RTC)。

## 边界规则

- **App 层不许出现 SDL**:`main.c`、`hal/hal_sdl.*` 是 PC 专用,App 层不 include 它们。
- **板子侧 App 不许 include BSP 私有头**:板子 HAL 适配层负责把 BSP public API 翻译成 `hal_*.h` 的接口。
- 平台差异全部由 HAL 实现消化,App 层只依赖 `hal_*.h` 和 LVGL。

## 构建

### PC 模拟器(WSL 下已验证)

```sh
git submodule update --init --depth 1     # 首次
cmake -S . -B build -G Ninja
cmake --build build
./build/lvgl-sim                          # 需要图形环境(WSLg 可用)
```

### ESP32-S3 板子

在 `jlc-esp32s3` 仓库里(见该仓库 `main/idf_component.yml` 与本仓库 `components/xtrack_app/` 的对应关系):

```sh
cd ~/esp/jlc-esp32s3
source ~/esp/esp-idf/export.sh
idf.py -p /dev/ttyACM0 flash monitor
```

板子选择走 `sdkconfig.defaults` 的 `CONFIG_BSP_BOARD_*`;换板后要删掉 `sdkconfig` 再编。

## 依赖和配置的两个来源(容易踩)

| | PC 模拟器 | 板子 |
|---|---|---|
| LVGL 来源 | `third_party/lvgl`(submodule,9.5) | IDF 组件管理器拉的 `lvgl/lvgl` 9.5 |
| LVGL 配置 | `lv_conf.h` | `sdkconfig`(Kconfig) |
| 字体开关 | `lv_conf.h` 里的 `LV_FONT_MONTSERRAT_*` | `jlc-esp32s3/sdkconfig.defaults`(已开 App 用到的 14/16/32/48) |

所以"PC 上能编、板子上编不过"的第一嫌疑永远是**字体/功能开关不同源**。

### 两侧的 LVGL 版本必须一起升

实测(2026-09-17):PC 侧子模块(commit `85aa60d`)与板子侧 registry 包(`lvgl/lvgl` 9.5.0)的 `src/` 源码树**逐字节相同**(1130 个文件,0 个不同),`lvgl.h` 与 `lv_conf_template.h` 的 md5 也相同,版本宏两边都是 9.5.0。差异只在打包外围:板子侧多 `CHECKSUMS.json` / `.component_hash`,PC 侧多 `.github`,`idf_component.yml` 两边不同 —— 这些都不参与编译。

**`git describe` 不能当版本依据**:LVGL 的 `v9.5.0` 标签打在 release 分支上,不在 master 那批 commit 的祖先链里,所以子模块的 `git describe` 会报 `v9.3.0-992-g85aa60d18`,看起来像"落后两个版本",实际内容就是 9.5.0。

规则:**升 LVGL 时两侧一起升,并把 PC 侧子模块钉到与 registry 相同的那一个 tag**。否则上面说的 `"lvgl/lvgl.h"` 写法会让 App 的编译单元静默地用上另一份头文件,变成"不报错但行为诡异"。


还有一条:`uint32_t` 在 xtensa 上是 `unsigned long`,日志里的格式符一律用 `<inttypes.h>` 的 `PRIu32` 系列,不要用 `%u`/`%d`(IDF 默认 `-Werror=all`,直接编不过)。

## 资源流水线

`assets/` 里放图标源文件(PNG 用中文名 + AI/TTF 源),`assets/convert_images.sh` 调用 LVGL 的 `LVGLImage.py` 生成 `resource/image/*.c`(ARGB8888)。

**生成物不要手改**:要换图标改 `assets/`,再重跑脚本。生成文件头部那段 `#if defined(LV_LVGL_H_INCLUDE_SIMPLE)` 是生成器的模板,PC 和板子两端都能正确解析,不需要也不应该动它。

## 代码风格与笔记

- 风格约定见 `CODING_CONVENTIONS.md`。
- 开发笔记(架构理解、LVGL 用法、踩坑)在 `~/notebook/_posts/Embedded/LVGL.md`,不在本仓库。

## 已知限制 / 下一步

- **布局**:按 PC 的 480x320 横屏设计,而目标是 **AuraS3 的圆形 AMOLED(直径 466,内接安全区只有约 330x330,四周各留约 68px)**。可用宽度随高度剧变(y=0 处 `0` px、y=25 处约 `210` px、y=106 处约 `391` px、中心行 `466` px),所以现在这些"全宽横条"在圆屏上会被裁,顶部最严重:`pages/status_bar` 的 25px 全宽条、`pages/dialplate` 的顶部与底部数据带。需要适配的还有 `pages/dialplate` 的 `LV_ALIGN_TOP_MID` 偏移与信息卡尺寸、`pages/system_infos` 的 `ITEM_PAD = (LV_VER_RES - 100) / 2` 与 `CARD_W` / `ICON_W`。注意 `LV_HOR_RES` 在圆屏上只是"最宽处"的宽度,不代表任意高度的可用宽度。
- **输入模型**:PC 是鼠标拖动 + 滚轮(滚轮被映射成 encoder,用于组内焦点导航);板子是电容触摸,没有旋钮编码器。需要定案:触摸点击可用,拖动返回需要实测阈值,滚轮/键盘导航在板子上不可达。
- **拖动返回**:`pm_root_enable_drag()` 目前**没有任何页面启用**。`dialplate` 曾经启用,但它是栈底页(startup `pm_replace` 进来的),没有可返回的页面,`pm_pop()` 也会以 "only root page remains" 拒绝,所以调用已移除。待定的问题:该把拖动挂到 `livemap` / `system_infos`(它们下面有 `dialplate`),以及用什么轴。
  注意轴向不是独立配置:`pm_drag.c` 的 `pm_get_drag_axis()` 由页面动画类型推出(LEFT/RIGHT → 横向,TOP/BOTTOM → 纵向),而全局动画目前是 `app.c` 的 `LOAD_ANIM_OVER_TOP`(纵向)。要做"左右滑动返回",必须同时把全局动画改成横向,否则会出现"往右拖、页面往下退出"。
  另外:挂手势的页面要注意 LVGL 的按压目标选择(`lv_indev_search_obj` 取最上层命中对象)与冒泡(`EVENT_BUBBLE`),装饰性容器默认带 `CLICKABLE`,会吞掉按压,导致根对象收不到手势。
- **板子 HAL 还是模拟数据**:`hal_gps` / `hal_power` 用的是 `esp_timer` 造的假数据(`hal_clock` 用编译时间兜底),真实 BSP 能力(GNSS / PMU / RTC)尚未接入。
