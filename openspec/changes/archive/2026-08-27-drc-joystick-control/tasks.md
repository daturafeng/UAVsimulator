## 1. 协议常量

- [x] 1.1 UAVCore：UAVCloudApiTypes.h/.cpp 新增 topic 模板 kTopicDrcDownTemplate / kTopicDrcUpTemplate
- [x] 1.2 UAVCore：新增指令 method 常量 kMethodDrcModeEnter / kMethodDrcModeExit / kMethodDroneControl / kMethodHeartBeat / kMethodDroneEmergencyStop 与事件常量 kEventDrcStatusNotify

## 2. 无人机摇杆运动模型

- [x] 2.1 UAVDroneSim：UAVDroneSimComponent 新增 SetJoystickCommand / SetJoystickActive / IsJoystickActive 接口与摇杆状态成员（x/y/h/w/freq/delayTime/最近指令时间）
- [x] 2.2 UAVDroneSim：TickComponent 新增摇杆物理：机体坐标按航向旋转、速度平滑、w 偏航、delayTime 过期悬停、累计遥测；摇杆模式优先于任务推进并停止现有任务
- [x] 2.3 UAVDroneSim：自动化测试 UAVJoystickControlTests.cpp 覆盖移动/偏航/过期悬停/任务互斥

## 3. 飞控 DRC 会话状态机

- [x] 3.1 UAVFlightControl：新增 DRC 会话成员（bDrcActive / bJoystickControlActive / LastDrcSeq）、OnDrcStatusNotify 委托与 GetLastDrcSeq 查询
- [x] 3.2 UAVFlightControl：HandleCommand 增加 drc_mode_enter / drc_mode_exit / drone_control / heart_beat / drone_emergency_stop 五分支与处理器（校验飞控权/空中状态/参数范围/会话状态）
- [x] 3.3 UAVFlightControl：drone_control 成功时停止现有任务并调用 SetJoystickCommand；drone_emergency_stop 停止一切运动；进入/退出广播 OnDrcStatusNotify

## 4. 桥接层 DRC 通道与事件

- [x] 4.1 UAVMqttBridge：OnMqttConnect 订阅 kTopicDrcDownTemplate，新增 DrcSubscription 与 OnDrcMessage 回调
- [x] 4.2 UAVMqttBridge：新增 DispatchDrcMessage（解析 tid/bid/method/data，分发到飞控）与 PublishDrcUpReply（drc/up 回执，drone_control/heart_beat 带 output.seq，急停仅 result）
- [x] 4.3 UAVMqttBridge：DispatchServicesMessage 精确分发 drc_mode_enter / drc_mode_exit
- [x] 4.4 UAVMqttBridge：绑定/解绑 OnDrcStatusNotify，新增回调发布 kEventDrcStatusNotify；新增 BuildDrcUpReply / BuildDrcStatusNotifyData 测试入口
- [x] 4.5 UAVMqttBridge/Tests：新增 UAVDrcControlTests.cpp 断言回执结构、事件 data、services/DRC 通道分发

## 5. 构建与验证

- [x] 5.1 UBT 构建 UAVsimulatorEditor（Win64 Development）
- [x] 5.2 Automation RunTests UAV 全部通过（含新增用例与既有基线）
- [x] 5.3 openspec validate --specs 通过，按流程归档变更并 git commit + push
