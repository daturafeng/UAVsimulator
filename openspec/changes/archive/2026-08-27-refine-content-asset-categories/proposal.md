## Why

当前 `Content/` 目录已按 `project-structure` 规范建好 Blueprints、Drones、Materials、Props、UI、Maps 等一级分类，但缺少模型（Models）与贴图（Textures）两个核心资产分类；同时 UE 编辑器自动生成的小写 `Content/map/main.umap` 与规范要求的大写 `Content/Maps/` 并存，造成关卡位置歧义。

## What Changes

- 新增一级资产目录 `Content/Models`（模型资产）与 `Content/Textures`（贴图资产）。
- 将默认关卡 `Content/map/main.umap` 迁入 `Content/Maps/main.umap`，删除冗余的小写 `Content/map` 目录，统一关卡存放位置。
- 保留 UE 自动生成的 `Content/Collections`、`Content/Developers` 目录不动。
- 更新 `project-structure` 规范中"资产目录分类"需求：资产分类扩充为 蓝图、模型、贴图、关卡、无人机、道具、材质、界面，并明确默认关卡位于 `Content/Maps/`。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `project-structure`: 扩充"资产目录分类"需求，新增 Models/Textures 分类并约束默认关卡位于 Maps/。

## Impact

- `Content/` 目录结构（纯资产文件移动，无代码改动）。
- `openspec/specs/project-structure/spec.md` 需求更新。
- 不涉及 C++ 模块、.uproject 配置与第三方依赖。
