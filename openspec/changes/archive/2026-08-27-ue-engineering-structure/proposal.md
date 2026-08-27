## Why

UAVsimulator 目前是 UE 5.7 的空白 C++ 模板，只有一个 Runtime 模块，`Source/UAVsimulator` 下没有任何业务划分。项目要承担"接收 DJI 上云 API 飞控/航线指令 → 驱动无人机与相机在 UE 世界移动 → 把相机画面推送到 dock 系统"的完整模拟职责。如果没有先定好工程化目录规范，后续功能会堆叠在单模块里，难以扩展、测试与多人协作。需要在本轮就把工程目录结构按 UE 工程规范建立起来，作为后续所有功能的骨架。

## What Changes

- 将单一的 `UAVsimulator` Runtime 模块按领域拆分为职责清晰的多个模块（例如 Core、DroneSim、FlightControl、CameraStream、MqttBridge、Waypoint 等），各自挂载到 `.uproject` 的 `Modules` 列表。
- 采用 UE 标准目录约定：`Source/<Module>/Public` 与 `Source/<Module>/Private` 分离头文件与实现。
- 建立 UE 标准资产目录结构：`Content/` 下按 `Maps`、`Drones`、`Props`、`UI`、`Materials`、`Blueprints` 等划分，供美术与关卡使用。
- 新增 `Config/` 下的模块化配置约定（默认沿用 UE 生成的 Default*.ini），以及 `Docs/` 顶层目录承载项目技术文档。
- 补上顶层 `README.md` 与 `AGENTS.md`，说明模块划分、构建方式与开发约定。
- 保持现有引擎版本 5.7 与 Build.cs 依赖基线不变，仅做目录与模块组织层面的调整。

## Capabilities

### New Capabilities
- `project-structure`: 定义 UAVsimulator 项目的工程化目录与模块划分规范，包括 `Source` 领域模块、`Content` 资产分类、配置与文档约定。

### Modified Capabilities
<!-- 当前无既有 spec，暂无修改项 -->

## Impact

- `Source/UAVsimulator`：从单一模块改为按领域拆分的多模块布局（新增若干模块目录与 `*.Build.cs`）。
- `UAVsimulator.uproject`：`Modules` 列表增加新增模块条目。
- `Config/`：新增或调整各模块配置（沿用 UE 默认 ini）。
- 顶层新增 `README.md`、`AGENTS.md`、`Docs/`。
- 不影响 `D:\WebCode\dock` 仓库；本次仅为 UE 工程目录规范化。
