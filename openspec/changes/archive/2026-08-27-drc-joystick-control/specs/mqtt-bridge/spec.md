## ADDED Requirements

### Requirement: DRC 指令通道
UAVMqttBridge MUST 在连接后订阅 thing/product/{机场SN}/drc/down，将报文（tid/bid/method/data，无 gateway）分发到 UAVFlightControl：drone_control / heart_beat / drone_emergency_stop；并把结果回发到 thing/product/{来源SN}/drc/up，data 为 { result, output?: { seq } }（drone_control / heart_beat 带 output.seq，drone_emergency_stop 仅 result）。services 通道 MUST 精确分发 drc_mode_enter / drc_mode_exit（其余 drc_* 前缀方法仍按未知指令回复 services_reply）。

#### Scenario: DRC 指令分发与回执
- **WHEN** 收到 thing/product/{DockSn}/drc/down 的 drone_control 报文
- **THEN** 调用 UAVFlightControl.HandleCommand，并向 thing/product/{来源SN}/drc/up 回发 { tid, bid, timestamp, method, data: { result, output: { seq } } }

#### Scenario: 急停回执
- **WHEN** 收到 thing/product/{DockSn}/drc/down 的 drone_emergency_stop 报文
- **THEN** 回发 drc/up 报文，data 仅含 result（无 output）

#### Scenario: DRC 模式指令走 services
- **WHEN** 收到 method 为 drc_mode_enter / drc_mode_exit 的 services 报文
- **THEN** 调用 UAVFlightControl.HandleCommand 并在 services_reply 中返回其 result

### Requirement: DRC 状态事件上报
UAVMqttBridge MUST 绑定 UAVFlightControl.OnDrcStatusNotify，将 DRC 会话状态变化拼装为 thing/product/{机场SN}/events 报文（method=drc_status_notify）发布，data = { result: 0, drc_state }（对齐 dock DrcStateEnum：0=DISCONNECTED、1=CONNECTING、2=CONNECTED）。

#### Scenario: 进入 DRC 上报已连接
- **WHEN** 飞控广播 OnDrcStatusNotify(2)
- **THEN** 发布 drc_status_notify 事件，data.drc_state=2

#### Scenario: 退出 DRC 上报断开
- **WHEN** 飞控广播 OnDrcStatusNotify(0)
- **THEN** 发布 drc_status_notify 事件，data.drc_state=0
