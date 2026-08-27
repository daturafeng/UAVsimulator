## Context

桥接层（UUAVMqttBridgeComponent）已承担 services 分发（DispatchServicesMessage）、events 发布（PublishEvent）与设备 state 发布（PublishDeviceState）。固件升级是纯状态模拟（无真实固件包下载），升级目标版本、设备列表与进度状态托管在桥接组件私有成员即可，无需新增模块。协议以 dock cloud-sdk 的 FirmwareMethodEnum / AbstractFirmwareService / EventsMethodEnum / DockStateDataKeyEnum / DockFirmwareVersion 为口径（见 proposal.md - Why）。

## Goals / Non-Goals

**Goals:**
- services 通道精确识别并处理 ota_create，按 devices 数组校验并记录升级目标。
- 升级成功后发布 ota_progress 事件（sent → in_progress → ok），对齐 OtaProgress 结构。
- 连接成功与升级完成后上报固件版本 state（机场 / 无人机 / 载荷三个通道）。
- 新增自动化测试覆盖指令校验、回执、进度事件与固件版本 state。

**Non-Goals:**
- 不实现固件包下载/校验/刷写真实流程，升级进度为同步模拟序列。
- 不新增 OSD 字段、不联动飞控（升级期间不阻止其他指令执行）。
- 不实现 fileupload_* 日志上传（独立能力，后续变更另行覆盖）。
- 不持久化固件版本到磁盘，重启后恢复默认版本。

## Decisions

1. **升级状态托管在桥接组件私有成员**：新增 OtaTargetVersion（目标版本字符串）、bOtaUpgrading（升级中标志），与 BuildOtaProgressEventData 同层。备选：新增 UAVFirmwareModule——纯状态模拟、无 Tick 逻辑，新增模块成本高于收益。
2. **精确方法匹配**：DispatchServicesMessage 用 Method == kMethodOtaCreate 精确分支（ota_create 无前缀可复用），命中后调用新入口 HandleOtaCreate(Method, DataJson)。
3. **回执带 output**：复用 PublishServicesReply 的可选 InOutput 参数，成功时 output = { status: "sent" }（对齐 ServicesReplyData<OtaCreateResponse>）。
4. **进度事件同步三段发布**：处理成功后立即发布 sent（percent=0, current_step=1, rate=0）、in_progress（percent=50, current_step=1）、ok（percent=100, current_step=2, rate=0）三条事件；不引入定时器，避免生命周期与断连竞态（与远程调试进度事件同模式）。
5. **固件版本 state 复用连接建立流程**：PublishDeviceStates（连接成功时调用）扩展为同时发布固件版本报文；新增 BuildDockFirmwareVersionData / BuildDroneFirmwareVersionData / BuildPayloadFirmwareVersionData 三个可测试构建入口；升级完成（ok 事件）后更新 OtaTargetVersion 并重发。
6. **默认固件版本常量**：机场默认 03.01.0000、无人机默认 03.01.0000（占位），升级后使用 ota_create 中对应设备的 product_version。

## Risks / Trade-offs

- 同步三段事件在极短时间窗内连续发布 → dock 端按 bid 关联事件，顺序由发布时序维持，与远程调试进度事件同模式，无定时器竞态。
- 版本号仅为占位模拟 → 仅影响 dock-fe 展示，不涉及真实固件语义；默认值在设计与测试中显式记录。
- 升级中不阻止其他指令 → 模拟范围明确记录在 Non-Goals，dock-fe 展示链路不受影响。
