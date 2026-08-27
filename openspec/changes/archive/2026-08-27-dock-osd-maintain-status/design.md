# 设计：机场 OSD 补齐 maintain_status

## Context

BuildDockOsdPayload 已组装 38 个顶层字段，唯缺 maintain_status（dock OsdDock 第 30 位字段）。dock OsdDockMaintainStatus 结构：maintain_status_array（List<DockMaintainStatus>），元素含 last_maintain_flight_sorties（Integer）、last_maintain_time（Long）、last_maintain_type（MaintainTypeEnum：0=NO / 1/2/3=无人机保养 / 17/18=机库保养）、state（Boolean）。dock-be 全局 Jackson SNAKE_CASE，JSON 键为 maintain_status / maintain_status_array / last_maintain_flight_sorties / last_maintain_time / last_maintain_type。

## Goals / Non-Goals

**Goals:**

- 机场 OSD data 补齐 maintain_status，字段名与类型对齐 OsdDockMaintainStatus。
- 新增/扩展自动化测试覆盖 maintain_status 顶层字段与 maintain_status_array 结构。

**Non-Goals:**

- 不做维护期/保养到期推算（无真实物理模型，按新机场基线输出）。
- 不引入 UAVDroneSim 维护状态成员（桥接层模拟基线即可）。
- 不改协议与 Topic。

## Decisions

- **结构对齐**：maintain_status 输出对象 { maintain_status_array: [ { last_maintain_flight_sorties: 0, last_maintain_time: 0, last_maintain_type: 0, state: false } ] }，与 OsdDockMaintainStatus / DockMaintainStatus 字段一一对应。
- **基线语义**：新机场未保养 → last_maintain_type=0（NO）、state=false（未到保养期限）、累计架次 0、保养时间为 0（从未保养），避免 dock 端误判为保养到期。
- **组装位置**：在 BuildDockOsdPayload 中 activation_time 之后、electric_supply_voltage 之前输出，与 OsdDock.java 字段顺序一致，便于对照维护。
- **测试**：UAVDockOsdTests.Structure 的 RequiredFields 增加 maintain_status；新增 maintain_status_array 数组结构与首元素四字段断言（FJsonValueArray / AsObject），沿用现有 NewObject + SetDroneSim 测试范式。

## Risks / Trade-offs

- last_maintain_time=0 若 dock 端有"从未保养"特殊逻辑，可能触发保养提醒；模拟器无维护模型，属可接受基线行为。
- 不随累计架次增长更新维护字段，保持确定性，便于测试断言。
