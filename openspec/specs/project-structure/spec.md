# project-structure Specification

## Purpose

为 UAVsimulator 建立工程化的目录与模块划分规范，使 UE 项目具备清晰的分层、可扩展与可测试结构，作为后续无人机飞控、航线与相机推流功能落地的骨架。

## Requirements

### Requirement: 领域模块划分
UAVsimulator 的 C++ 代码 MUST 按领域拆分为独立 Runtime 模块，每个模块拥有独立的 `Source/<Module>/` 目录，并在 `.uproject` 的 `Modules` 列表登记。至少划分出 核心、无人机模拟、飞控指令、相机推流、MQTT 桥接 等职责边界，单模块不得承担全部业务。

#### Scenario: 项目按领域模块布局
- **WHEN** 检查项目 `Source/` 目录与 `.uproject` 的 `Modules` 列表
- **THEN** 存在多个按领域命名的模块目录，且每个模块都登记在 `.uproject` 中

#### Scenario: 单模块边界
- **WHEN** 新增飞控或推流相关功能
- **THEN** 该功能应归属到对应领域模块，而非堆叠在核心模板模块中

### Requirement: 头文件与实现分离
每个模块 MUST 遵循 UE 标准约定，将公开头文件放在 `Source/<Module>/Public/`，实现文件放在 `Source/<Module>/Private/`。

#### Scenario: 目录分离
- **WHEN** 查看任一领域模块
- **THEN** 该模块下存在 `Public/` 与 `Private/` 两个子目录，且实现文件位于 `Private/`

### Requirement: 资产目录分类
`Content/` MUST 按用途分类组织资产目录，至少包含 蓝图、模型、贴图、关卡、无人机、道具、材质、界面 等分类，禁止资产散落在 `Content/` 根目录；默认关卡 MUST 存放于 `Content/Maps/` 下。

#### Scenario: 资产分类
- **WHEN** 查看 `Content/` 目录
- **THEN** 存在按用途命名的一级子目录（如 Blueprints、Models、Textures、Maps、Drones、Props、UI、Materials），且资产按类别归档

#### Scenario: 默认关卡位置
- **WHEN** 检查项目默认关卡位置
- **THEN** 关卡文件位于 `Content/Maps/` 下，且不存在冗余的小写 `Content/map/` 目录

### Requirement: 配置与文档约定
项目 MUST 提供顶层 `README.md`、`AGENTS.md` 与 `Docs/` 目录，说明模块划分、构建方式与开发约定；`Config/` 沿用 UE 默认 `Default*.ini` 配置。

#### Scenario: 顶层文档
- **WHEN** 查看项目根目录
- **THEN** 存在 `README.md`、`AGENTS.md` 与 `Docs/` 目录

#### Scenario: 配置基线
- **WHEN** 查看 `Config/` 目录
- **THEN** 保留 UE 生成的 `Default*.ini` 配置，且版本仍为 UE 5.7
