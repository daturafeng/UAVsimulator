## 1. 机场 OSD 组装

- [x] 1.1 头文件新增 BuildDockOsdPayload（Public 测试入口）与机场状态推导辅助函数声明（IsDroneInDock / GetFlightTaskStepCode / IsDockInMission）
- [x] 1.2 实现 BuildDockOsdPayload：对齐 OsdDock 全字段（network_state / drone_in_dock / drone_charge_state / alternate_land_point / position_state / storage / air_conditioner / sub_device / backup_battery / drone_battery_maintenance_info / media_file_detail / wireless_link 等），中文注释
- [x] 1.3 PublishDockOsd 改用 BuildDockOsdPayload 组装 data，Topic/gateway 结构不变

## 2. 机场状态推导

- [x] 2.1 实现 IsDroneInDock：机场原点 ±0.00002 度内 + 高度 ≤12 + Idle
- [x] 2.2 实现 GetFlightTaskStepCode：任务中=0 / 返航降落=2 / 其余=5；充电状态：归巢待命且电量<100 为充电中
- [x] 2.3 acc_time / flighttask_prepare_capacity / cover_state / mode_code 由 UAVDroneSim 状态推导

## 3. 自动化测试

- [x] 3.1 新增 UAV.MqttBridge.DockOsd 测试（Source/UAVMqttBridge/Private/Tests/UAVDockOsdTests.cpp）：结构完整性（38 个顶层字段与子对象）
- [x] 3.2 状态推导测试：归巢/任务中/返航场景的 drone_in_dock / cover_state / drone_charge_state / flighttask_step_code 数值

## 4. 验证与归档

- [x] 4.1 编译通过（UE 5.7 UBT 构建 UAVsimulatorEditor）
- [x] 4.2 自动化测试通过（UAV.MqttBridge.DockOsd 新增项 + UAV.* 全量回归，19 项全绿）
- [x] 4.3 openspec validate 校验变更并归档；git commit + push
