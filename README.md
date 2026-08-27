# UAVsimulator

基于 UE 5.7 的无人机飞控模拟器，用于接收 `D:\WebCode\dock`（DJI 上云 API，机场 dock3、机型 M4TD）下发的飞控/航线指令，模拟无人机受控移动或按航线飞行，并将相机视角通过 RTMP 推送给 dock 系统。

## 模块划分

- `UAVsimulator`：游戏入口模块。
- `UAVCore`：公共类型、工具、模块依赖基座。
- `UAVDroneSim`：无人机本体模拟（飞行状态、逻辑/物理移动）。
- `UAVFlightControl`：飞控指令（飞行权、起飞到点、航线执行）状态机与执行。
- `UAVCameraStream`：相机载荷模拟与 RTMP 推流。
- `UAVMqttBridge`：与 dock 后端 MQTT 的上云 API 协议桥接。

模块依赖方向：`UAVMqttBridge → UAVFlightControl → UAVDroneSim → UAVCore`；`UAVCameraStream` 独立依赖无人机状态。

## 构建

- 引擎版本：UE 5.7
- 在源码目录执行 UnrealBuildTool 编译，或打开 `UAVsimulator.sln` 用 IDE 构建。

## 文档

技术文档统一归档在 `Docs/` 目录。
