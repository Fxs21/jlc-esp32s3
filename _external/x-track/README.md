# 移植任务(X-TRACK)
    * 源码
        * 模拟器: /mnt/e/project/lvgl_win/lv_port_pc_vscode/
        * X-TRACK: /mnt/e/project/X-TRACK/
        * 模拟器: /mnt/d/project/lvgl/lv_port_pc_vscode/
        * X-TRACK: /mnt/d/private/X-TRACK-main/
        * xtrack-app: https://github.com/Fxs21/xtrack-app
    * 目标
        * 以 pc 模拟器为业务层基础
        * 以当前项目 component 为驱动基础
        * 将 x-track 项目移植到 esp32s3 开发板运行
    * 要求
        * 对于模拟器代码. x-track 中的代码分为 APP\ 和 HAL\
        * APP\ 中的代码应当与模拟器完全保持一致(即使用 xtrack-app 仓库)
        * HAL\ 中的代码应该根据平台不同自行实现