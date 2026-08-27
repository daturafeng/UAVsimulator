# AGENTS

## 1. 项目定位

本仓库是 UE 5.7 无人机飞控模拟器，模拟接收 `D:\WebCode\dock` 下发的 DJI 上云 API 飞控/航线指令，驱动无人机与相机移动，并把相机画面通过 RTMP 推送给 dock。

## 2. 强制流程

1. 所有软件设计与实现必须使用 OpenSpec（`openspec/`），OpenSpec 是需求、规格、设计、任务状态和变更记录的唯一事实来源。
2. 新变更必须先创建完整的 OpenSpec 提案、规格、设计和任务工件，再开始实现。
3. 需求或技术决策变化时，先更新 OpenSpec 工件保持一致，再继续实现；不得先改代码后补 OpenSpec。
4. 实现过程中按 OpenSpec 任务逐项执行并同步任务状态；完成后按流程归档。

## 3. 模块职责

- `UAVsimulator`：游戏入口模块。
- `UAVCore`：公共类型、工具、模块依赖基座。
- `UAVDroneSim`：无人机本体模拟。
- `UAVFlightControl`：飞控指令状态机与执行。
- `UAVCameraStream`：相机载荷模拟与 RTMP 推流。
- `UAVMqttBridge`：与 dock 后端 MQTT 的上云 API 协议桥接。

## 4. 目录与编码约定

- 每个 C++ 模块使用 `Public/` 与 `Private/` 分离头文件与实现。
- `Content/` 按用途分类（Maps、Drones、Props、UI、Materials、Blueprints），禁止散落资产。
- 配置沿用 UE 5.7 默认 `Config/Default*.ini`。
- 代码注释与日志统一使用中文。
- 未经明确要求，不得自动提交推送代码。

## 5. 参考

- 协议依据 DJI 上云 API：https://developer.dji.com/doc/cloud-api-tutorial/cn/
- 对接方仓库：`D:\WebCode\dock`
