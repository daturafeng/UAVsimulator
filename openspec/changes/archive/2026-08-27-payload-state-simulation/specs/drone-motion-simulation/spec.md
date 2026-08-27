# drone-motion-simulation Specification

## Purpose

无人机本体模拟除位置/朝向/速度外，新增载荷状态（电量、云台、相机）与累计飞行遥测，为 OSD 上报提供随飞行推进的模拟数据。

## ADDED Requirements

### Requirement: 电量状态模拟
UUAVDroneSim MUST 维护电池电量百分比（初始可配置，默认 100），在飞行状态（起飞/航线/巡航/降落/返航）按飞行消耗速率、待机状态按待机消耗速率推进；电量不低于 0，并提供电量、剩余飞行时间（电量/飞行消耗速率）、返航电量阈值与降落电量阈值的查询接口。

#### Scenario: 飞行中电量下降
- **WHEN** 组件处于飞行状态且流逝时间 Δt
- **THEN** 电量按 飞行消耗速率 × Δt 下降且不小于 0

#### Scenario: 待机电量缓慢下降
- **WHEN** 组件处于待机状态且流逝时间 Δt
- **THEN** 电量按 待机消耗速率 × Δt 下降（速率默认显著低于飞行速率）

#### Scenario: 低电量事件
- **WHEN** 电量首次低于返航电量阈值
- **THEN** 组件广播低电量事件（此后不再重复广播，直至电量回升到阈值以上再次触发）

### Requirement: 电池单元信息
UUAVDroneSim MUST 提供电池单元（index/temperature/voltage）推导：温度 = 27.5 + (100-电量)*0.06 + 水平速度*0.18，电压 = max(22000, 25800 + 电量*18 - 水平速度*35) 毫伏；OSD 上报两个电池单元（第二个单元温度 +0.5、电压 -80）。

#### Scenario: 电压/温度随电量与速度变化
- **WHEN** 电量或水平速度变化
- **THEN** 单元电压/温度按上述公式单调方向变化（电压不低于 22000 毫伏）

### Requirement: 云台状态模拟
UUAVDroneSim MUST 维护云台俯仰/横滚/偏航角度：俯仰 = 基础角 + sin(模拟时间*0.24)*俯仰振幅，横滚 = cos(模拟时间*0.22)*横滚振幅，偏航在开启跟随（默认）时 = 机头朝向 + sin(模拟时间*0.28)*4° 并归一化到 0-360，否则保持独立角度。

#### Scenario: 云台角度随时间微动
- **WHEN** 模拟时间推进
- **THEN** 俯仰/横滚在振幅范围内正弦变化，偏航跟随机头朝向

### Requirement: 相机状态模拟
UUAVDroneSim MUST 维护相机模式（0 拍照 / 1 录像，默认 1）与变焦倍率（默认 3.0，钳制到可配置范围 1.0-7.0），提供 SetCameraMode / SetZoomFactor 控制接口；录像状态按飞行模式推导：起飞/航线/返航状态为录像中，其余状态非录像中；累计录制秒数供 OSD record_time 使用。

#### Scenario: 飞行模式驱动录像状态
- **WHEN** 飞行状态为起飞/航线/返航
- **THEN** IsRecording 返回 true，否则返回 false

#### Scenario: 变焦倍率钳制
- **WHEN** SetZoomFactor 传入超出范围的值
- **THEN** 变焦倍率被钳制到 [ZoomFactorMin, ZoomFactorMax]

### Requirement: 累计飞行遥测
UUAVDroneSim MUST 累计飞行距离（米）与飞行时长（秒）：移动期间按水平位移累加距离、按 Δt 累加时长，并提供查询接口。

#### Scenario: 移动产生累计遥测
- **WHEN** 组件处于移动状态
- **THEN** 累计飞行距离与时长持续增加
