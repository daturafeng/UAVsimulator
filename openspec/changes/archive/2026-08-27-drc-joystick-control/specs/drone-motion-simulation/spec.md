## ADDED Requirements

### Requirement: 摇杆速度控制
UUAVDroneSim MUST 提供摇杆速度控制：SetJoystickCommand(x,y,h,w,freq,delayTime) 记录机体坐标摇杆（x 前向/后向、y 左向/右向、h 垂直、w 偏航角速度 度/秒），Tick 中按航向旋转得到场景速度平滑移动位置并按 w 更新朝向；freq 控制响应速率，delayTime 内未收到新指令自动悬停（摇杆归零）；悬停（全 0）保持位置；摇杆模式必须停止现有航点任务且不视为任务。

#### Scenario: 摇杆驱动移动
- **WHEN** 调用 SetJoystickCommand(0, 5, 0, 0, 5, 500) 且机头朝北
- **THEN** 无人机沿机头右向移动，朝向不变，位置按速度 × Δt 推进

#### Scenario: 偏航控制
- **WHEN** 调用 SetJoystickCommand(0, 0, 0, 30, 5, 500)
- **THEN** 朝向按 +30 度/秒持续右转，位置不水平移动

#### Scenario: 指令过期悬停
- **WHEN** 摇杆控制中超过 delayTime 未收到新指令
- **THEN** 摇杆目标归零，无人机悬停（水平/垂直速度归零）

#### Scenario: 摇杆模式与任务互斥
- **WHEN** 存在航点任务时进入摇杆控制
- **THEN** 现有任务被停止，HasActiveMission 返回 false，摇杆控制期间按摇杆推进
