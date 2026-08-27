## Context

桥接层（UUAVMqttBridgeComponent）已承担 services 分发（DispatchServicesMessage）、机场 OSD 组装（BuildDockOsdPayload）与 events 发布（PublishEvent），并持有无人机模拟组件引用。远程调试指令是纯状态模拟（无物理），机场设备状态与 OSD 同属桥接层，按"既有能力优先"不新增模块。指令/进度协议以 dock cloud-sdk 的 DebugMethodEnum / AbstractDebugService / EventsMethodEnum / RemoteDebugProgress 为口径（见 proposal.md - Why）。

## Goals / Non-Goals

**Goals:**
- services 通道精确识别并处理 20 个远程调试指令，带参指令按 dock 枚举校验。
- 12 个带进度方法发布 RemoteDebugProgress 事件（sent → ok）。
- 机场 OSD 相关字段由指令驱动，指令覆盖优先于推导。
- 新增自动化测试覆盖指令处理、回执、进度事件与 OSD 联动。

**Non-Goals:**
- 不实现 eSIM 指令（esim_activate / sim_slot_switch / esim_operator_switch 为 DOCK2 专属）。
- 不联动飞控/无人机运动（drone_open/close 仅反映电源状态，不阻止指令执行）。
- 不引入定时器异步进度（sent → ok 同步连续发布）。
- 不修改无人机 OSD / 相机行为。

## Decisions

1. **状态托管在桥接组件私有成员**：机场设备状态（调试模式、补光灯、报警、推杆、舱门覆盖、充电覆盖、电池存储模式、空调模式、电池保养、链路工作模式、无人机电源）作为 UUAVMqttBridgeComponent 私有 UPROPERTY 成员，与 BuildDockOsdPayload 同层。备选：新增 UAVDockSim 模块——状态无物理模拟、无 Tick 逻辑，新增模块成本高于收益，且桥接层已是协议与状态汇聚点。
2. **精确方法集合匹配**：DispatchServicesMessage 用静态 TSet<FString> 精确匹配 20 个调试 method（drone_open 等无统一前缀，不能用前缀分支）；命中后调用新入口 HandleDebugCommand(Method, DataJson)。
3. **回执带 output**：PublishServicesReply 增加可选 InOutput 参数（默认 nullptr，现有调用不变）；调试指令成功时 output = { status: "sent" }（对齐 ServicesReplyData<RemoteDebugResponse>），失败（参数校验等）时仅 result。
4. **进度事件同步两段发布**：处理成功后立即发布 sent（percent=0，currentStep/totalSteps=1，stepKey 按方法语义映射）与 ok（percent=100，stepKey 相同）两条事件；不引入定时器，避免生命周期与断连竞态。switch 类指令不发布进度事件。
5. **OSD 覆盖优先级**：cover_state 与 drone_charge_state.state 采用"指令覆盖优先、未覆盖回退推导"（bCoverOverride / bChargeOverride 标志）；其余字段（supplement_light_state、alarm_state、putter_state、battery_store_mode、air_conditioner_state、maintenance_state、link_workmode、device_online_status）直接由状态成员输出；mode_code 在调试模式激活时输出 2，否则沿用 4/3 推导。
6. **stepKey 语义映射**：cover_open=open_cover、cover_close=close_cover、putter_open=free_putter、putter_close=close_putter、charge_close=stop_charge、drone_open=open_drone；无精确枚举语义的方法（device_reboot / drone_close / drone_format / device_format / charge_open）缺省 stepKey，progress 仍带 percent/currentStep/totalSteps/stepResult。

## Risks / Trade-offs

- drone_open/close 仅驱动 OSD 电源字段，不阻止无人机指令 → 模拟范围明确记录在 Non-Goals，dock-fe 展示链路不受影响。
- 同步双事件在极短时间窗内连续发布 → dock 端按 bid 关联事件，顺序保证由发布时序维持，无需定时器。
- 覆盖标志不随归巢状态自动清除 → 模拟语义简单可预期，测试确定性优先；如需自动恢复可在后续变更补充。
