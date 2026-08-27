## 1. 协议常量与飞控事件

- [x] 1.1 UAVCore：UAVCloudApiTypes.h/.cpp 新增事件常量 kEventReturnHomeInfo / kEventFlighttaskReady / kEventHms
- [x] 1.2 UAVFlightControl：新增 FUAVFlighttaskReadyDelegate（FString FlightId）与 OnFlighttaskReady 动态委托，并在 Header 声明
- [x] 1.3 UAVFlightControl：HandleFlighttaskPrepare 成功后广播 OnFlighttaskReady（携带 flight_id）
- [x] 1.4 UAVFlightControl：新增 GetCurrentFlightId() / GetCurrentRthAltitude() 只读接口（供桥接层 return_home_info 使用）

## 2. 桥接层事件报文重构

- [x] 2.1 UAVMqttBridge：重构 OnFlighttaskProgress 为 data={result:0, output:{status, progress:{current_step,percent}, ext:{current_waypoint_index,media_count,flight_id,track_id,wayline_id,wayline_mission_state}}}（snake_case，wayline_mission_state 按状态映射）
- [x] 2.2 UAVMqttBridge：OnReturnHomeStatus 改为发布 return_home_info（planned_path_points / last_point_type / flight_id），移除 return_home_status 发布
- [x] 2.3 UAVMqttBridge：新增 OnFlighttaskReady 回调并在 BeginPlay/EndPlay 绑定/解绑，发布 flighttask_ready（data.flight_ids=[flight_id]）
- [x] 2.4 UAVMqttBridge：新增 hms 组装与发布（BuildHmsPayload + PublishHms），OnMqttConnect 成功后发布空告警，低电量自动返航回调连带发布低电量告警

## 3. 自动化测试

- [x] 3.1 UAVMqttBridge/Tests：新增 UAVProtocolEventsTests.cpp，NewObject 断言 flighttask_progress（output/result 包装、snake_case 字段、wayline_mission_state 映射）
- [x] 3.2 同上：断言 return_home_info（method/planned_path_points/last_point_type/flight_id，不再含 return_home_status）
- [x] 3.3 同上：断言 flighttask_ready（flight_ids）与 hms（空告警与低电量告警结构）

## 4. 构建与验证

- [x] 4.1 UBT 构建 UAVsimulatorEditor（Win64 Development）
- [x] 4.2 Automation RunTests UAV 全部通过（含新增用例与既有 20 项基线）
- [x] 4.3 openspec validate --specs 通过，按流程归档变更
