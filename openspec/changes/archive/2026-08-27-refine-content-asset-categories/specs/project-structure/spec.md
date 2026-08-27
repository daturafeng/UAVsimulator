## MODIFIED Requirements

### Requirement: 资产目录分类
`Content/` MUST 按用途分类组织资产目录，至少包含 蓝图、模型、贴图、关卡、无人机、道具、材质、界面 等分类，禁止资产散落在 `Content/` 根目录；默认关卡 MUST 存放于 `Content/Maps/` 下。

#### Scenario: 资产分类
- **WHEN** 查看 `Content/` 目录
- **THEN** 存在按用途命名的一级子目录（如 Blueprints、Models、Textures、Maps、Drones、Props、UI、Materials），且资产按类别归档

#### Scenario: 默认关卡位置
- **WHEN** 检查项目默认关卡位置
- **THEN** 关卡文件位于 `Content/Maps/` 下，且不存在冗余的小写 `Content/map/` 目录
