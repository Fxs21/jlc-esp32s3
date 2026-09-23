# 板级自检工程

`test/` 下每个模块一个独立 IDF 工程, 用统一入口 `bsp.sh` 构建和烧写.

## 约定

- 每个 app 只测一个 BSP 模块; 判据写在测试代码里, 阈值是 app 顶部的常量.
- 只使用 BSP public API, 不 include board port 或私有 driver 头文件.
- 上电即自检, 不需要人工敲命令; 结论由程序判定, 不靠人读日志判断.
- 模块在该板上不存在时输出 `result=SKIP`, 不算失败.
- 大缓冲不要放栈上: `app_main` 任务栈只有几 KB, KB 级数组会先冲掉堆内存再以
  `Interrupt wdt timeout` panic 收场; 放 `static` 或用堆分配.
- `sdkconfig.defaults` 自足: 每个 app 需要的配置全部写在自己文件里, 不从根工程继承;
  项目基线 (target, Flash 大小, 分区表, CPU 频率, 主任务栈, FATFS) 要抄全,
  漏项会造成与 BSP 无关的"配置型假失败".
- `sha` 是构建期注入的 git 指纹; 带 `-dirty` 后缀说明构建时工作区有未提交改动, 此时日志不能单独作为验证证据.

## 输出标记

机器可读的固定格式, 便于以后用脚本汇总或直接贴进 truth table:

```text
SELFTEST start module=imu board=AuraS3 sha=1a2b3c4d
SELFTEST end module=imu board=AuraS3 sha=1a2b3c4d tests=4 failed=0 result=PASS
SELFTEST end module=camera board=AuraS3 sha=1a2b3c4d result=SKIP reason=no camera
SELFTEST human module=display board=AuraS3 item=ghosting result=yes|no|pending
```

人工项由程序提示, 人只做物理动作和回答 `y` / `n`; 超时未回答记 `pending`, 既不算通过也不算失败.

## 构建

```sh
cd test
./bsp.sh <app> <auras3|aura|doers3|doer> build flash monitor
```

## 目录

- `bsp.sh`, `project.cmake`: 统一构建入口和公共 CMake 脚手架.
- `selftest/`: 自检公共骨架 (指纹行, 汇总行, SKIP 语义, 人工确认输入).
- 其余每个目录是一个模块的自检 app.
