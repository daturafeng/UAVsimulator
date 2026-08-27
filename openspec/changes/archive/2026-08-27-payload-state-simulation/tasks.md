## 1. UAVPayloadMath 纯函数

- [x] 1.1 新增 Public/UAVPayloadMath.h 与 Private/UAVPayloadMath.cpp：电量消耗、剩余飞行时间、电池温度/电压（dock 公式）、云台角度（配置 + 时间）、风向 8 方位枚举
- [x] 1.2 新增自动化测试：电量消耗（飞行/待机/到零钳制/禁耗）、剩余时间、电压温度公式、云台跟随与范围、风向枚举

## 2. UAVDroneSimComponent 载荷状态

- [x] 2.1 增加载荷配置：电量（初始值/飞行消耗/待机消耗/landing/return_home）、云台（基础角/振幅/跟随）、相机（模式/变焦范围/默认变焦）
- [x] 2.2 增加载荷查询/控制接口：电量与电池单元、剩余时间、云台角度、相机模式/变焦/录像、累计飞行距离与时间；低电量事件
- [x] 2.3 Tick 推进载荷状态：电量消耗、云台微动、录制时长累计、累计飞行距离/时间

## 3. OSD 组装对齐 dock

- [x] 3.1 BuildDroneOsdPayload 电量段改为双电池 + dock 公式 + 从 DroneSim 读取容量与阈值
- [x] 3.2 payloads / cameras 从模拟状态读取，camera 枚举改 int，补齐 cameras 全部 dock 字段
- [x] 3.3 补齐顶层字段：gear / position_state / wind / total_flight_* / speaker / storage / night_lights_state / distance_limit_status / obstacle_avoidance / track_id 等

## 4. 验证与归档

- [x] 4.1 编译通过（UE 5.7 UBT 构建 UAVsimulatorEditor）
- [x] 4.2 自动化测试通过（UAV.Payload.*，全量 UAV.* 回归通过）
- [x] 4.3 openspec validate 校验变更并归档；git commit + push
