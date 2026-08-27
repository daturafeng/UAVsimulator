## 1. 直播能力报文组装与发布

- [x] 1.1 UAVMqttBridgeComponent.h 声明 BuildLiveCapacityPayload() const 与 PublishLiveCapacity()（中文注释）
- [x] 1.2 UAVMqttBridgeComponent.cpp 实现组装：data.live_capacity（available_video_number=3 / coexist_video_number_max=3 / device_list 网关项+无人机项，相机/视频字段对齐 report_live_capacity.py），发布到 thing/product/{DockSn}/state
- [x] 1.3 OnMqttConnect 连接成功后（PublishDeviceState 之后）调用 PublishLiveCapacity()

## 2. 自动化测试

- [x] 2.1 新增 UAVLiveCapacityTests.cpp：断言顶层 live_capacity、device_list 两设备项（网关 165-0-7 / 无人机 176-0-0 与 52-0-0）、video_type=zoom 与 switchable_video_types=[normal,wide,zoom,ir]

## 3. 验证与归档

- [x] 3.1 编译通过（UE 5.7 UBT 构建 UAVsimulatorEditor）
- [x] 3.2 自动化测试通过（UAV 全量回归，20 项全绿）
- [ ] 3.3 openspec validate 校验变更并归档；git commit + push
