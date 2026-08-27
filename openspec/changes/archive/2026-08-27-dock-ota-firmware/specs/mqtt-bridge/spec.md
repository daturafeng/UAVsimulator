## ADDED Requirements

### Requirement: 固件升级指令分发
UAVMqttBridge MUST 将 services 通道收到的固件升级指令 ota_create 精确分发并处理（对齐 dock FirmwareMethodEnum.OTA_CREATE）。指令 data MUST 解析 devices 数组（1-2 个设备），每个设备项 MUST 含 sn / product_version / file_url / md5 / file_size / firmware_upgrade_type / file_name；设备项缺失或字段类型非法时 MUST 返回 InvalidParams。处理成功后 MUST 回发 services_reply，data 为 { result: 0, output: { status: "sent" } }（对齐 ServicesReplyData<OtaCreateResponse>），并记录待升级设备（机场与/或无人机）。

#### Scenario: 固件升级指令分发
- **WHEN** 收到 method 为 ota_create 的 services 报文，data.devices 含机场与无人机各一项（firmware_upgrade_type 为 2/3）
- **THEN** 指令执行成功并回发 services_reply，data 含 result=0 与 output.status="sent"

#### Scenario: 升级设备参数非法
- **WHEN** 收到 ota_create 且 data.devices 为空、设备项缺 sn/product_version，或 firmware_upgrade_type 不在 2/3
- **THEN** 回发 result=InvalidParams 的 services_reply，不发布进度事件

### Requirement: 固件升级进度事件
UAVMqttBridge MUST 在 ota_create 成功后向 thing/product/{机场SN}/events 发布 ota_progress 进度事件（对齐 dock EventsMethodEnum.OTA_PROGRESS / EventsDataRequest<OtaProgress>），data 为 { result: 0, output: { status, progress: { percent, current_step }, ext: { rate } } }。事件序列 MUST 依次为 status="sent"（percent=0、current_step=1、rate=0）、status="in_progress"（percent=50、current_step=1）、status="ok"（percent=100、current_step=2、rate=0）；current_step 对齐 OtaProgressStepEnum（1=DOWNLOADING、2=UPGRADING）。

#### Scenario: 升级进度事件序列
- **WHEN** ota_create 指令处理成功
- **THEN** 依次发布 ota_progress 事件 sent（percent=0, current_step=1, rate=0）、in_progress（percent=50, current_step=1）、ok（percent=100, current_step=2），三条事件 data.result 均为 0

#### Scenario: 升级失败不发布进度
- **WHEN** ota_create 参数校验失败（result 非 0）
- **THEN** 仅回发 services_reply，不发布任何 ota_progress 事件

### Requirement: 固件版本 state 上报
UAVMqttBridge MUST 在 MQTT 连接成功后向 thing/product/{机场SN}/state 发布固件版本报文（对齐 dock DockStateDataKeyEnum.FIRMWARE_VERSION / DockFirmwareVersion）：data 含 firmware_version（格式 xx.xx.xxxx）、compatible_status（是否需要一致性升级，默认 false）、firmware_upgrade_status（升级中 true，否则 false）；向 thing/product/{无人机SN}/state 发布 rc_and_drone_firmware_version（对齐 RcStateDataKeyEnum.FIRMWARE_VERSION / FirmwareVersion，data 为 { firmware_version }）与载荷固件版本（对齐 PayloadFirmwareVersion，data 键为载荷索引，值为 { firmware_version }）。OTA 升级成功（ok 事件）后 MUST 将机场与无人机 firmware_version 更新为目标版本并重发固件版本 state，firmware_upgrade_status 恢复 false。

#### Scenario: 连接成功后固件版本上报
- **WHEN** MQTT 连接成功
- **THEN** 向机场 state 发布 firmware_version 报文（含 firmware_version / compatible_status / firmware_upgrade_status），向无人机 state 发布 rc_and_drone_firmware_version 与载荷固件版本报文

#### Scenario: 升级完成后版本更新
- **WHEN** ota_create 指定目标版本且 ok 进度事件已发布
- **THEN** 机场与无人机 state 重发 firmware_version 报文，firmware_version 更新为目标版本、firmware_upgrade_status=false
