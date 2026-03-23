# AuraS3

## 状态

- 预留板型
- 当前已接入编译期选择，UI 相关 backend selector 也已就位，但真实硬件 truth / backend 仍未实现
- 后续补充 AuraS3 的真实 board 描述和组件接线差异

## 规划

- 复用同一套 BSP 组件
- 补齐 AuraS3 truth table，并按需要接入新的芯片 driver
- 若某些控制脚改为直连 GPIO，继续在组件 backend 内部处理，不新增特殊 API
