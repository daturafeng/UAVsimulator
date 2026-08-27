## Context

当前 `Content/` 已存在 Blueprints、Drones、Materials、Props、UI、Maps 一级目录（见 proposal.md - Why），但缺少 Models/Textures，且 UE 编辑器自动生成的小写 `Content/map/main.umap` 与规范的大写 `Content/Maps/` 并存。本变更只做目录级整理，不涉及代码与配置改动。

## Goals / Non-Goals

**Goals:**

- 补齐 Models、Textures 两个资产分类目录。
- 将默认关卡统一到 `Content/Maps/`，消除大小写目录并存。
- 更新 `project-structure` 规范中"资产目录分类"需求。

**Non-Goals:**

- 不移动 `Content/Collections`、`Content/Developers`（UE 自动生成目录）。
- 不调整 C++ 模块、.uproject、Config 配置。
- 不创建任何实际模型/贴图资产，仅建立目录骨架。

## Decisions

- **关卡统一放入 `Content/Maps/`**：规范已定义大写 Maps 分类，小写 `map` 是 UE 新建项目时的默认产物，两者并存会导致资产浏览器中出现两个关卡目录。移动 `main.umap` 后删除空的 `Content/map`。备选方案（保留小写 map 并删除大写 Maps）会违背既有规范，不采用。
- **新增目录命名 `Models`、`Textures`**：与 UE 社区通用资产命名一致（复数名词、首字母大写），与现有 Blueprints/Drones/Materials 风格统一。备选（Assets/、Meshes/）不如 Models/Textures 直观，不采用。
- **保留 `Collections`、`Developers`**：由 UE 编辑器按用户生成，属于个人工作区，删除可能影响编辑器状态，故保留。

## Risks / Trade-offs

- [移动关卡文件后，`.uproject` 或编辑器内的默认关卡引用可能指向旧路径] → `main.umap` 由编辑器自动引用，属纯文件移动；提示用户在编辑器打开项目后确认默认关卡正常加载，必要时在 Project Settings 重新指定 `/Game/Maps/main`。
- [删除 `Content/map` 目录属于破坏性操作] → 移动完成后该目录为空才删除；移动前保留原文件直到确认新位置存在。

## Open Questions

无。
