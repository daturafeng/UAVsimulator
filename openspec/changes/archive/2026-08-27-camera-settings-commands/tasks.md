## 1. UAVDroneSim 相机设置状态

- [x] 1.1 新增曝光状态：ExposureMode / ShutterSpeed / Iso / ExposureCompensation，setter 与 getter（含钳制）
- [x] 1.2 新增对焦状态：FocusMode / FocusValue / PointFocusAction，setter 与 getter（FocusValue 钳制 0-100）
- [x] 1.3 新增红外测光状态：IrMeteringMode / IrMeteringPointX/Y / IrMeteringArea 参数，setter 与 getter
- [x] 1.4 新增存储/分屏/焦距/看点/POI 状态：PhotoStorageLocation / VideoStorageLocation / ScreenSplitEnabled / FocalLength / LookAtTarget / PoiModeActive / PoiMaxSpeed / PoiGimbalYawRate，setter 与 getter
- [x] 1.5 新增自动化测试：曝光/对焦/测光/存储/分屏/焦距/看点/POI 状态设置与钳制（UAV.Payload.CameraSettings）

## 2. UAVCameraStream 相机设置指令

- [x] 2.1 新增 16 个相机设置指令处理函数（LookAt / ScreenSplit / PhotoStorage / VideoStorage / Exposure / ExposureMode / FocusMode / FocusValue / IrMeteringMode / IrMeteringPoint / IrMeteringArea / PointFocusAction / FocalLength / PoiModeEnter / PoiModeExit / PoiCircleSpeed）
- [x] 2.2 HandleCommand 分发相机设置 method 到对应处理函数，沿用载荷权校验

## 3. UAVMqttBridge 分发与 OSD

- [x] 3.1 UAVCloudApiTypes 增加相机设置指令 method 常量（16 个）
- [x] 3.2 DispatchServicesMessage 分发前缀扩展：photo_storage_* / video_storage_* / ir_metering_* / poi_*
- [x] 3.3 OSD cameras 接入实时设置值（screen_split_enable / photo_storage_settings / video_storage_settings / zoom_focus_mode / zoom_focus_value）

## 4. 验证与归档

- [x] 4.1 编译通过（UE 5.7 UBT 构建 UAVsimulatorEditor）
- [x] 4.2 自动化测试通过（UAV.Payload.* 新增项 + 全量 UAV.* 回归）
- [x] 4.3 openspec validate 校验变更并归档；git commit + push
