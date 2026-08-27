## 1. 项目骨架与文档

- [x] 1.1 顶层新增 `README.md`，说明项目定位、模块划分与构建方式
- [x] 1.2 顶层新增 `AGENTS.md`，记录开发约定与用户级规则
- [x] 1.3 顶层新增 `Docs/` 目录，建立技术文档归档位置

## 2. 领域模块划分

- [x] 2.1 新建 `Source/UAVCore` 模块（Public/Private 结构、`UAVCore.Build.cs`）
- [x] 2.2 新建 `Source/UAVDroneSim` 模块（无人机模拟）
- [x] 2.3 新建 `Source/UAVFlightControl` 模块（飞控指令状态机）
- [x] 2.4 新建 `Source/UAVCameraStream` 模块（相机推流）
- [x] 2.5 新建 `Source/UAVMqttBridge` 模块（上云 API MQTT 桥接）
- [x] 2.6 在 `UAVsimulator.uproject` 的 `Modules` 列表登记新增模块，并按 `UAVMqttBridge → UAVFlightControl → UAVDroneSim → UAVCore` 声明模块依赖

## 3. 资产目录与配置

- [x] 3.1 在 `Content/` 下建立 `Maps`、`Drones`、`Props`、`UI`、`Materials`、`Blueprints` 一级目录
- [x] 3.2 确认 `Config/` 保留 UE 5.7 默认 `Default*.ini`，未改动版本
