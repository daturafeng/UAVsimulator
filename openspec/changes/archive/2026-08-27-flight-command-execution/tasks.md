## 1. UAVCore 协议常量与工具

- [x] 1.1 在 UAVCore 新增上云 API 常量（Topic 模板、method 字符串、默认机场/无人机 SN、相机索引、视频类型）
- [x] 1.2 新增报文头工具（tid/bid/timestamp 生成，JSON 报文头构造）

## 2. 地理坐标转换（UAVDroneSim）

- [x] 2.1 新增 UAVGeoUtils：经纬度/海拔 ↔ 场景坐标双向转换（机场为原点 ENU）
- [x] 2.2 为 UAVGeoUtils 增加可验证的转换用例（互逆与已知点校验）

## 3. 无人机运动模拟增强（UAVDroneSim）

- [x] 3.1 航点结构（经纬度/海拔 + 到达阈值）与航点队列
- [x] 3.2 速度可控移动：水平速度钳制 + 垂直速度 + 朝向跟随
- [x] 3.3 到达判定与航点推进、任务结束回调
- [x] 3.4 遥测查询接口（经纬度/海拔/朝向/速度/状态）

## 4. 飞控指令协议与状态机（UAVFlightControl）

- [x] 4.1 指令参数结构体与 method → 参数解析映射
- [x] 4.2 飞控权抢占与状态机（待机/起飞到点/航线/返航/降落）
- [x] 4.3 起飞到点（takeoff_to_point）执行与 takeoff_to_point_progress 事件
- [x] 4.4 航线任务（flighttask_create/prepare/execute/undo/pause/recovery）与 flighttask_progress 事件
- [x] 4.5 返航（return_home / return_home_cancel）与任务中断
- [x] 4.6 指令结果返回（result 0/非0）与失败原因

## 5. 直播会话模型（UAVCameraStream）

- [x] 5.1 直播会话结构（video_id 生成、RTMP 地址拼接、清晰度/镜头类型）
- [x] 5.2 live_start_push / live_stop_push / live_set_quality / live_lens_change 处理

## 6. 验证与归档

- [x] 6.1 编译通过（使用 UE 5.7 的 UBT 构建 UAVsimulatorEditor）
- [x] 6.2 通过 openspec validate 校验变更
- [x] 6.3 同步主 spec 并归档变更
