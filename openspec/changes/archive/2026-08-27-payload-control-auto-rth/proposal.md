## Why

已归档的 payload-state-simulation 让 OSD 中的电量/云台/相机状态随飞行推进，但载荷状态只能被动变化，无法被 dock 指令驱动。dock 的 simulate_control_services.py 明确支持 payload_authority_grab、camera_mode_switch、camera_photo_take/stop、camera_recording_start/stop、camera_aim、gimbal_reset 等载荷指令并回 result=0，而 UAVsimulator 对所有这些 method 统一回 UnknownMethod，联调时载荷链路是断的。另外低电量场景目前只广播 OnBatteryLow 事件，飞控不会自动返航，与真实无人机"低电量自动返航"行为不符，dock 只能通过 OSD mode_code 看到异常掉电而无人机仍在巡航。

## What Changes

- UAVDroneSim 扩展载荷控制接口：拍照状态（photo_state、剩余照片数、已拍照片数）、录像指令覆盖（recording override，指令优先于飞行模式推导）、云台角度指令设置（camera_aim / gimbal_reset）、载荷权状态（payload_authority_grab 抢占后供 OSD 与权限校验）。
- UAVCameraStream 处理载荷指令：payload_authority_grab、camera_mode_switch、camera_photo_take、camera_photo_stop、camera_recording_start、camera_recording_stop、camera_aim、gimbal_reset；指令校验载荷权（抢占成功后才可执行拍照/录像/云台），结果经 OnCommandResult 广播回 services_reply。
- UAVFlightControl 低电量自动返航：绑定 UAVDroneSim.OnBatteryLow，电量低于返航阈值且在空中（起飞/航线/巡航）且已持有飞控权时自动执行返航（与 return_home 指令同一路径），返回过程中不再重复触发。
- UAVMqttBridge 载荷指令分发：camera_*/payload_*/gimbal_* 分发到 UAVCameraStream；payload_authority_grab 成功后补发 thing/product/{sn}/state 载荷控制源报文（对齐 dock report_control_source.py，control_source=A）；自动返航触发时发布 events return_home_status（rth_auto_trigger）。

## Capabilities

### New Capabilities
<!-- 无：能力建立在已有 camera-streaming / flight-control-protocol / drone-motion-simulation / mqtt-bridge 之上 -->

### Modified Capabilities
- camera-streaming: 载荷指令（相机/云台/载荷权）从未知回非 0 变为可执行并驱动载荷状态。
- flight-control-protocol: 低电量自动返航，电量阈值触发与人工 return_home 同路径。
- drone-motion-simulation: 载荷状态可被指令控制（拍照/录像覆盖/云台指令/载荷权）。
- mqtt-bridge: 载荷指令分发与载荷控制源 state 报文、自动返航事件。

## Impact

- Source/UAVDroneSim：UAVDroneSimComponent 增加拍照/录像覆盖/云台指令/载荷权接口与状态。
- Source/UAVCameraStream：UAVCameraStreamComponent 增加载荷指令处理（依赖已有 UAVDroneSim），注入 DroneSim 引用。
- Source/UAVFlightControl：UAVFlightControlComponent 绑定低电量事件并复用返航逻辑。
- Source/UAVMqttBridge：DispatchServicesMessage 分发载荷指令；新增载荷控制源 state 发布与自动返航事件。
- Source/UAVCore：UAVCloudApiTypes 增加载荷指令 method 常量。
- Source/UAVDroneSim/Private/Tests：新增拍照/录像覆盖/载荷权自动化测试。
- 行为变化：dock 下发 payload_authority_grab 后模拟器回 0 并上报控制源 A；camera_* / gimbal_* 指令驱动载荷状态且 OSD 可见；电量低于返航阈值自动返航。
