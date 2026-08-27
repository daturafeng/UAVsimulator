# flight-control-protocol Specification

## Purpose

电量低于返航阈值时自动返航，与人工 return_home 同路径，保证低电量安全闭环。

## ADDED Requirements

### Requirement: 低电量自动返航
UAVFlightControl MUST 绑定 UAVDroneSim.OnBatteryLow：电量低于返航阈值、持有飞控权且当前处于空中状态（起飞/航线/巡航）时自动执行返航（复用 return_home 路径，停止当前任务并飞回机场返航高度），返回中不重复触发；返航触发广播 OnReturnHomeStatus（status=rth_auto_trigger、reason=battery_low）。

#### Scenario: 低电量自动返航
- **WHEN** 航线飞行中电量降至返航阈值以下
- **THEN** 当前任务被中断，进入返航状态并广播 rth_auto_trigger 事件

#### Scenario: 返航中不重复触发
- **WHEN** 已在返航状态且电量继续下降
- **THEN** 不再次触发自动返航

#### Scenario: 待机低电量不返航
- **WHEN** 待机状态电量降至阈值以下
- **THEN** 不触发自动返航（无飞行任务）
