## Context

UAVsimulator 是 UE 5.7 C++ 项目，当前为空白模板：`Source/UAVsimulator` 单一 Runtime 模块，无业务划分；`.uproject` 仅登记该模块与 `ModelingToolsEditorMode` 插件。项目目标是模拟接收 `D:\WebCode\dock`（DJI 上云 API，机场 dock3、机型 M4TD）下发的飞控/航线指令，驱动无人机与相机移动，并把相机画面通过 RTMP 推送回 dock。本轮只做工程化目录与模块骨架，不实现具体飞控逻辑（见 proposal）。

## Goals / Non-Goals

**Goals:**
- 按领域拆分 C++ 模块，形成清晰分层，作为后续功能的骨架。
- 建立 UE 标准 `Public/Private` 目录约定、`Content` 资产分类、顶层文档与 `Config` 基线。

**Non-Goals:**
- 不实现 MQTT 通信、飞控指令解析、相机推流等具体业务逻辑（后续单独变更）。
- 不调整引擎版本、不引入第三方库依赖。
- 不创建可运行的关卡资产（仅建目录骨架）。

## Decisions

- **按领域拆模块**：将单一 `UAVsimulator` 拆为以下 Runtime 模块，各司其职：
  - `UAVCore`：公共类型、工具、模块依赖基座。
  - `UAVDroneSim`：无人机本体模拟（飞行状态、物理/逻辑移动）。
  - `UAVFlightControl`：飞控指令（飞行权、起飞到点、航线执行）的状态机与执行。
  - `UAVCameraStream`：相机载荷模拟与推流（RTMP）。
  - `UAVMqttBridge`：与 dock 后端 MQTT 的上云 API 协议桥接。
  - 原 `UAVsimulator` 保留为游戏入口模块。
  理由：单一模块无法容纳多领域且易耦合；拆分后每个模块可独立编译、测试与演进。备选"全部放单模块"被否决，因为会重蹈当前模板无结构问题。

- **沿用 UE 标准目录**：每个模块使用 `Public/`、`Private/`，`*.Build.cs` 声明依赖。理由：UE 生态惯例，IDE、编译与打包均按此约定工作。

- **资产目录分类**：`Content/` 下建 `Maps`、`Drones`、`Props`、`UI`、`Materials`、`Blueprints` 一级目录。理由：按用途归类便于查找与权限管理；不做深层子目录避免过度设计。

- **顶层文档**：新增 `README.md`（项目介绍与构建）、`AGENTS.md`（开发约定）、`Docs/`（技术文档归档）。理由：与其他工作区（如 dock 的 AGENTS 约定）保持一致，便于 Agent 与人类协作。

## Risks / Trade-offs

- [多模块增加初始复杂度] → 模块职责边界清晰，文档中明确各模块归属，后续按此扩展。
- [模块间依赖顺序需谨慎] → 采用分层依赖：`UAVMqttBridge → UAVFlightControl → UAVDroneSim → UAVCore`，推流模块独立依赖无人机状态，避免循环依赖。
- [重构可能影响现有模板编译] → 本轮仅新增模块与目录骨架，保留原 `UAVsimulator` 入口模块，降低回归风险。
