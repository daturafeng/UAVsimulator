# drone-motion-simulation Specification

## Purpose
定义无人机受控运动模拟的行为：把经纬度/海拔坐标映射到 UE 场景，按速度可控地沿航点移动，并维护位置、朝向与飞行状态，作为飞控指令的物理执行层。
## Requirements
### Requirement: 地理坐标转换
系统 MUST 提供经纬度/海拔与 UE 场景坐标的双向转换，以机场位置为局部坐标系原点（北-东-上），转换结果保持一致性与可逆性。

#### Scenario: 经纬度转场景坐标
- **WHEN** 给定机场经纬度与一个目标经纬度/海拔
- **THEN** 系统返回以机场为原点的 UE 场景坐标（米制），北向对应场景 -Y（或配置轴）且高度对应 Z

#### Scenario: 场景坐标转经纬度
- **WHEN** 给定无人机 UE 场景坐标
- **THEN** 系统返回对应经纬度/海拔，且与正向转换互逆（误差在可接受范围）

### Requirement: 航点移动
系统 MUST 支持按航点序列移动：无人机以最大速度约束下的水平速度与垂直速度向当前航点移动，到达航点后切换到下一航点，直到任务结束；移动过程中更新位置与朝向（朝向指向移动方向）。

#### Scenario: 沿航点移动
- **WHEN** 飞控层下发一条含多个航点的航线
- **THEN** 无人机依次飞向各航点，逐点推进并最终停在最后一个航点

#### Scenario: 速度约束
- **WHEN** 无人机在两个航点间移动
- **THEN** 水平速度不超过配置的最大速度，垂直速度按高度差驱动

#### Scenario: 到达判定
- **WHEN** 无人机与当前航点距离小于到达阈值
- **THEN** 判定到达该航点并切换到下一航点

### Requirement: 位置与状态可查询
系统 MUST 暴露当前经纬度、海拔、朝向、水平/垂直速度与飞行状态，供 OSD 上报与调试使用。

#### Scenario: 查询遥测
- **WHEN** 外部查询无人机状态
- **THEN** 返回当前经纬度、海拔、朝向、速度与飞行状态

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

### Requirement: 拍照状态模拟
UUAVDroneSim MUST 维护拍照状态：bPhotoTaking（拍照中）与剩余照片数（初始 9999），提供 TakePhoto / SetPhotoTaking 接口；TakePhoto 使剩余照片数减 1（最低 0）、累计拍照数加 1，并开始拍照中状态；拍照中状态在启动后 3 秒由组件 Tick 自动结束。

#### Scenario: 拍照推进状态
- **WHEN** 调用 TakePhoto
- **THEN** 拍照中状态为真，剩余照片数减 1，累计拍照数加 1，3 秒后拍照中自动结束

#### Scenario: 剩余照片数下限
- **WHEN** 剩余照片数为 0 时再次 TakePhoto
- **THEN** 剩余照片数保持 0，累计拍照数仍加 1

### Requirement: 录像指令覆盖
UUAVDroneSim MUST 提供录像状态覆盖：StartRecording / StopRecording / ClearRecordingOverride；存在覆盖时 IsRecording 返回覆盖值，否则按飞行模式推导（起飞/航线/返航为录像中）。

#### Scenario: 指令覆盖推导
- **WHEN** 飞行状态为待机但调用 StartRecording
- **THEN** IsRecording 返回 true（指令优先）

#### Scenario: 清除覆盖恢复推导
- **WHEN** 调用 ClearRecordingOverride 且飞行状态为航线
- **THEN** IsRecording 返回 true（恢复按飞行模式推导）

### Requirement: 云台指令目标
UUAVDroneSim MUST 提供云台目标设置：SetGimbalTarget(Pitch, Yaw) 记录目标角并叠加到 UAVPayloadMath 计算结果（目标优先级高于时间微动），ResetGimbalTarget 清除目标恢复时间微动；输出角度保持归一化。

#### Scenario: 云台目标叠加
- **WHEN** 设置目标俯仰 -30 度后查询云台状态
- **THEN** 俯仰输出等于目标角（不再受时间微动影响）

#### Scenario: 云台目标复位
- **WHEN** 调用 ResetGimbalTarget
- **THEN** 云台输出恢复时间微动推导

### Requirement: 载荷权状态
UUAVDroneSim MUST 维护载荷权：SetPayloadAuthority / HasPayloadAuthority；载荷权初始未抢占，payload_authority_grab 成功后置为已抢占。

#### Scenario: 载荷权抢占
- **WHEN** 调用 SetPayloadAuthority(true)
- **THEN** HasPayloadAuthority 返回 true

