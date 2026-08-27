# drone-motion-simulation Specification

## Purpose

UAVDroneSim 维护相机设置状态（曝光/对焦/测光/存储/分屏/焦距/看点/POI），供相机设置指令驱动与 OSD 输出。

## ADDED Requirements

### Requirement: 相机设置状态模拟
UUAVDroneSim MUST 维护相机设置状态并提供 setter/getter：曝光模式/快门/ISO/曝光补偿、对焦模式/对焦值（钳制 0-100）/点对焦动作、红外测光模式/测光点/测光区域、照片与录像存储位置、分屏使能、焦距、看点目标（经纬度/海拔）、POI 环绕模式与环绕速度。

#### Scenario: 曝光模式设置
- **WHEN** 调用 SetExposureMode(0) 后查询
- **THEN** 曝光模式返回 0，快门/ISO/曝光补偿为已设置值

#### Scenario: 对焦值钳制
- **WHEN** 调用 SetFocusValue(150)（超出 0-100）
- **THEN** 对焦值被钳制到 100

#### Scenario: 存储位置设置
- **WHEN** 调用 SetPhotoStorageLocation("current") 后查询
- **THEN** 照片存储位置返回 ["current"]（数组形式，对齐 OSD）

#### Scenario: POI 环绕状态
- **WHEN** 调用 SetPoiMode(true) 与 SetPoiCircleSpeed(10.0, 30.0)
- **THEN** POI 环绕模式为 true，最大环绕速度 10.0、云台偏航角速度 30.0

