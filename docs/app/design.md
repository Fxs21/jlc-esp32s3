# xtrack App 层设计

xtrack 是本仓库的正式承载应用 (骑行码表), 代码在 `xtrack/`. 本文档说明 App 层的分层, 平台边界和构建方式; 进度和已知限制见 `docs/app/status.md`, 代码风格见 `docs/app/coding-conventions.md`, BSP 侧规则见 `AGENTS.md` §3, §4 和 `docs/bsp/design.md`.

## 1. 三层结构

同一份 App 源码有两个后端, 三个编译场景:

| 场景 | 入口 | 产物 |
|---|---|---|
| PC 模拟器 | `cmake -S sim -B sim/build` | `sim/build/lvgl-sim` |
| root 固件 (不带板侧逻辑) | root `idf.py build` | `build/main.bin` |
| 板侧应用 (待接入) | 同上 | 同上 |

- App 层 `xtrack/`: 平台无关. 只 include LVGL, C 标准库和 `xtrack/hal/*.h`;不 include SDL, ESP-IDF 或 BSP 头文件.
- 后端: PC 侧 `sim/` (SDL2 显示/输入, 模拟 HAL, `lv_conf.h`);板侧 `main/` (板侧 HAL + 调 BSP public API 的 UI 接线).
- BSP 层 `components/bsp`: 板级差异的唯一消化处, 对板侧后端暴露 `bsp_xxx` public API.

App 层源清单的唯一出处是 `xtrack/app_sources.cmake`: `xtrack/CMakeLists.txt` (IDF 组件包装) 和 `sim/CMakeLists.txt` 都 include 它, 两边编的是同一份源码, 不存在拷贝.

## 2. 目录职责

| 路径 | 归属 | 说明 |
|---|---|---|
| `xtrack/app.c` `app.h` | App | 初始化顺序: DataCenter -> DataProc -> StatusBar -> PageManager |
| `xtrack/page_manager/` | App | 页面栈, 状态机, 动画, 拖动返回 (对应上游 X-TRACK 的 PageManager) |
| `xtrack/pages/` | App | startup / dialplate / livemap / system_infos / status_bar |
| `xtrack/data_center/` | App | 账号式发布订阅总线 (含双缓冲) |
| `xtrack/data_proc/` | App | 中间层节点: clock / gps / power, 把 HAL 数据推给 DataCenter |
| `xtrack/resource/` | App | `resource_pool.c` + `image/*.c` (图标, 生成物) |
| `xtrack/utils/` | App | `log.h`, `ds/` (vector, dbl_buf) |
| `xtrack/hal/` | HAL 接口 | `hal.h`, `hal_clock.h`, `hal_gps.h`, `hal_power.h` 接口声明 + `hal_init.c` 统一入口 |
| `xtrack/assets/` | 资源源文件 | 图标 PNG/AI/TTF + `convert_images.sh` |
| `xtrack/third_party/` | 依赖 | `uthash` (vendored) |
| `sim/` | PC 后端 | `main.c` 入口, `hal/hal_sdl.*`, `hal/hal_clock.c`, `hal_gps.c`, `hal_power.c` (模拟数据), `lv_conf.h`, `third_party/lvgl` (submodule) |
| `main/` | 板侧后端 | 应用入口;板侧 HAL 和 UI 接线待接入 |

## 3. 平台边界

- App 层不许出现 SDL / ESP-IDF / BSP 头文件;`tools/check.sh` 的 xtrack 边界检查强制, 同时拦截 sim 专用的 `hal_sdl.h`.
- 平台差异全部由 HAL 实现消化: App 只依赖 `xtrack/hal/*.h` 的接口.
- 板侧 HAL 只调 BSP public API,不 include board port 或私有 driver 头文件.
- `hal_init()` 是所有 HAL 模块的统一初始化入口;新增模块时写 `hal_<module>.h/.c` 并挂进 `hal_init.c`.

## 4. 构建

PC 模拟器 (Linux, 需要 SDL2 和图形环境, WSLg 可用):

```sh
git submodule update --init --depth 1   # 首次
cmake -S sim -B sim/build -G Ninja
cmake --build sim/build
./sim/build/lvgl-sim
```

板侧:

```sh
source ~/esp/esp-idf/export.sh
idf.py build
```

板子选择走 `sdkconfig.defaults` 的 `CONFIG_BSP_BOARD_*`;换板后要删掉 `sdkconfig` 再编.

## 5. 依赖和配置的两个来源

| | PC 模拟器 | 板子 |
|---|---|---|
| LVGL 来源 | `sim/third_party/lvgl` (submodule) | IDF 组件管理器拉的 `lvgl/lvgl` |
| LVGL 配置 | `sim/lv_conf.h` | `sdkconfig` (Kconfig) |
| 字体开关 | `lv_conf.h` 的 `LV_FONT_MONTSERRAT_*` | `sdkconfig.defaults` (已开 App 用到的 14/16/32/48) |

所以"PC 上能编, 板子上编不过"的第一嫌疑永远是字体/功能开关不同源;App 用到的 LVGL 功能两边都要开.

### LVGL 版本纪律

实测 (2026-09-17): PC 侧 submodule 与板侧 registry 包 (`lvgl/lvgl` 9.5.0) 的 `src/` 源码树逐字节相同, 差异只在打包外围 (`.github`, `CHECKSUMS.json` 等), 不参与编译.

- 升 LVGL 时两侧一起升, 并把 submodule 钉到与 registry 相同的那一个 tag.
- `git describe` 不能当版本依据: LVGL 的 `v9.5.0` 标签不在 master 的祖先链里, submodule 的 `git describe` 会报 `v9.3.0-992-g85aa60d18`, 实际内容就是 9.5.0.

## 6. 资源流水线

`xtrack/assets/` 放图标源文件 (PNG + AI/TTF),`xtrack/assets/convert_images.sh` 调 LVGL 的 `LVGLImage.py` 生成 `xtrack/resource/image/*.c` (ARGB8888).

- 生成物不要手改: 换图标改 `assets/`, 再重跑脚本.
- 生成文件头部的 `#if defined(LV_LVGL_H_INCLUDE_SIMPLE)` 是生成器模板, PC 和板子两端都能正确解析, 不要动.

## 7. 相关文档

- 进度, 已知限制和下一步: `docs/app/status.md`
- 代码风格: `docs/app/coding-conventions.md`
- BSP API 和 board port: `docs/bsp/design.md`, `docs/bsp/porting-guide.md`
