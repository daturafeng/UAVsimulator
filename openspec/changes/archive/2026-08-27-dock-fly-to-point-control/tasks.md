## 1. 协议常量

- [x] 1.1 UAVCore：UAVCloudApiTypes.h/.cpp 新增指令常量 kMethodFlyToPoint / kMethodFlyToPointStop / kMethodFlyToPointUpdate 与事件常量 kEventFlyToPointProgress

## 2. 飞控指点飞行状态机

- [x] 2.1 UAVFlightControl：新增 FUAVFlyToPointProgressDelegate（Status/FlyToId/WayPointIndex/Result）与 OnFlyToPointProgress 动态委托，Header 声明
- [x] 2.2 UAVFlightControl：HandleCommand 增加 fly_to_point / fly_to_point_stop / fly_to_point_update 三分支
- [x] 2.3 UAVFlightControl：HandleFlyToPoint 校验飞控权/参数（points 非空、height 2-10000、max_speed 1-15），中断当前任务并飞向 points[0]，广播 wayline_progress
- [x] 2.4 UAVFlightControl：HandleFlyToPointStop 停止任务并广播 wayline_cancel；HandleFlyToPointUpdate 复用会话更新目标与速度
- [x] 2.5 UAVFlightControl：OnDroneWaypointReached / OnDroneMissionFinished 在指点飞行会话中广播 wayline_progress / wayline_ok

## 3. 桥接层分发与事件

- [x] 3.1 UAVMqttBridge：DispatchServicesMessage 飞控指令前缀补充 fly_
- [x] 3.2 UAVMqttBridge：绑定/解绑 OnFlyToPointProgress，新增 OnFlyToPointProgress 回调发布 kEventFlyToPointProgress
- [x] 3.3 UAVMqttBridge：新增 BuildFlyToPointProgressEventData（result/status/fly_to_id/way_point_index）

## 4. 自动化测试

- [x] 4.1 UAVMqttBridge/Tests：新增 UAVFlyToPointTests.cpp，断言 fly_to_point_progress 报文结构
- [x] 4.2 同上：NewObject 组件调 HandleCommand 断言 fly_to_point 结果码/状态迁移、stop/update 语义

## 5. 构建与验证

- [x] 5.1 UBT 构建 UAVsimulatorEditor（Win64 Development）
- [x] 5.2 Automation RunTests UAV 全部通过（含新增用例与既有基线）
- [x] 5.3 openspec validate --specs 通过，按流程归档变更并 git commit + push
