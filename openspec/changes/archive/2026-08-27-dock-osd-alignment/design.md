## Context

上一变更 camera-settings-commands 完成后，无人机 OSD（thing/product/{DroneSn}/osd）已对齐 dock report_drone_osd.py 的完整字段，但机场 OSD（thing/product/{DockSn}/osd）仍只有 sn 与 drone_status 两个字段。dock 后端 OsdRouter 用 Jackson 将机场 OSD 反序列化为 OsdDock（40+ 字段），缺失字段全部为空，联调时设备管理页与任务编排拿不到机场环境、机库、子设备、充电与图传链路信息。

dock 侧口径（report_dock_osd.py + OsdDock.java）：
- 字段名与类型严格对齐 OsdDock，JSON 采用 snake_case（Jackson 驼峰转蛇形）。
- 多数派生物理量可从 UAVDroneSim 推导：机场原点（AirportOrigin）、无人机当前坐标/高度、飞行状态、电量、累计飞行时长。
- 环境/链路类字段按模拟基线输出：network_state、wireless_link、storage、position_state、air_conditioner、backup_battery、maintain_status 等。

当前实现：PublishDockOsd 直接内联组装两个字段；BuildDroneOsdPayload 已提供完整的无人机 OSD 组装范式（中文注释、FJsonObject 字段设置、安全跳过未注入模拟组件）。

## Goals / Non-Goals

**Goals:**

- 机场 OSD data 对齐 dock OsdDock 字段集与类型，字段名 snake_case。
- 由 UAVDroneSim 推导归巢/充电/任务步骤/累计时长等状态，随模拟状态实时变化。
- 环境/链路/机库基线字段按模拟值稳定输出，结构完整可被 dock Jackson 正常反序列化。
- 新增自动化测试覆盖字段结构完整性与状态推导。

**Non-Goals:**

- 不做机库物理模拟（舱门/机械臂真实动作，cover_state/putter_state 仅按归巢状态推导）。
- 不做真实充电过程（充电仅由归巢待命 + 电量未满推导，不随时间充放电）。
- 不改协议与 Topic：仍发布到 thing/product/{DockSn}/osd，gateway 结构不变。
- 不做机场 OSD 与无人机 OSD 的字段互通重构（两套报文各自对齐 dock 口径）。

## Decisions

- **字段归属桥接层**：机场 OSD 组装（BuildDockOsdPayload）与状态推导（IsDroneInDock / GetFlightTaskStepCode / GetDroneChargeState）放在 UAVMqttBridgeComponent，与现有 BuildDroneOsdPayload 一致；UAVDroneSim 仅作为只读数据源，不新增机场状态成员，避免模拟层与协议层耦合。
- **归巢判定口径**：无人机在机场原点 ±0.00002 度内、高度 ≤12、且飞行状态为 Idle（待机）视为归巢待命（drone_in_dock=true）。dock 脚本用"模式编码==3"近似无人机不在任务中，模拟器 Idle 即对应此语义；飞行中（Flying）即便经过机场上空也不判定归巢。
- **充电状态**：归巢待命且电量 <100 → drone_charge_state.state=1（充电中），capacity_percent 输出电量取整；电量满则 state=0。充电过程本身不做（Non-Goal）。
- **任务步骤编码**：对齐 dock 口径——无人机任务中（TakingOff/Wayline）输出 0、返航/降落（ReturnHome/Landing）输出 2、其余状态输出 5；dock 的 5/17 等模式不在模拟器状态集合内，按"其余"处理。
- **机场 mode_code**：归巢待命输出 3（待命），无人机任务中输出 4（机场执行任务），与 dock 场景语义一致。
- **模拟基线**：network_state（type=2/quality=5/rate=100.0）、wireless_link（4g/sdr 全链路质量 5）、storage（total=512*1024 MB、used 随录制时长递增）、position_state（gps=21/is_fixed=2/quality=5/rtk=17）、环境温度 22.5/24.0、湿度 58、height=12.0、first_power_on=当前-180 天、activation_time=当前-120 天等，均取自 dock 脚本基线值。
- **可测试性**：BuildDockOsdPayload 提供 Public 测试入口（BlueprintCallable 仅内部测试用），自动化测试通过 NewObject + SetDroneSim 注入模拟组件后直接断言 JSON 结构。
- **无人机 SN 复用**：sub_device.device_sn 输出 DroneSn、device_model_key 输出 "0-100-0"（M4TD），device_online_status/device_paired=1。

## Risks / Trade-offs

- [环境/链路字段为静态基线] → dock 为演示级消费，字段存在且结构正确即可满足反序列化；动态场景属后续能力。
- [归巢判定依赖 Idle 状态] → 若无人机降落未回 Idle（如保持 Landing），drone_in_dock 可能为 false；飞控状态机在降落完成后置 Idle，风险可控。
- [maintain_status 输出 null] → OsdDock 该字段可为空，与 report_dock_osd.py 一致不输出；dock 反序列化不报错。
- [字段集大、易漏] → 以 OsdDock.java 字段清单为唯一依据逐一核对，并用结构完整性测试兜底。
