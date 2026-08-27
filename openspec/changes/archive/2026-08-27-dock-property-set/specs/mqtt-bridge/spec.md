## ADDED Requirements

### Requirement: 物模型属性设置订阅与回执
UAVMqttBridge MUST 在 MQTT 连接成功后订阅 thing/product/{机场SN}/property/set（对齐 dock TopicConst 物模型属性设置通道），解析报文的 tid/bid 与 data 单属性对象（{属性名: {字段: 值}}），按属性名分发校验，并回发 thing/product/{机场SN}/property/set_reply（对齐 TopicPropertySetResponse）：data 为 { result: 0 }（成功，PropertySetReplyResultEnum.SUCCESS）或 { result: 1 }（参数非法，FAILED）；报文头含 tid/bid/timestamp。未知属性或报文缺 data 时 MUST 回 result=1。

#### Scenario: 属性设置成功回执
- **WHEN** 收到 property/set 报文，data 为合法属性对象（如 { height_limit: { height_limit: 120 } }）
- **THEN** 属性状态更新并回发 property/set_reply，data.result=0

#### Scenario: 属性参数非法回执
- **WHEN** 收到 property/set 报文，data 属性值越界（如 height_limit=10）、缺必填字段或属性名未知
- **THEN** 不更新状态，回发 property/set_reply，data.result=1

### Requirement: 无人机属性设置与校验
UAVMqttBridge MUST 支持 7 个无人机属性（对齐 dock PropertySetFieldEnum / Receiver.valid）：night_lights_state（0/1）、height_limit（20-1500 米）、distance_limit_status（state 0/1 与 distance_limit 15-8000 至少一项，值域校验）、obstacle_avoidance（horizon/upside/downside 0/1 至少一项）、rth_altitude（20-500 米）、rc_lost_action（0=HOVER/1=LAND/2=RETURN_HOME）、exit_wayline_when_rc_lost（0=CONTINUE/1=EXECUTE_RC_LOST_ACTION）。校验通过后 MUST 写入无人机属性状态。

#### Scenario: 高度限制设置
- **WHEN** 收到 { height_limit: { height_limit: 120 } }
- **THEN** 无人机属性状态 HeightLimit 更新为 120 且回执 result=0

#### Scenario: 高度限制越界
- **WHEN** 收到 { height_limit: { height_limit: 10 } }（小于 20）
- **THEN** 属性状态不变，回执 result=1

#### Scenario: 距离限制状态设置
- **WHEN** 收到 { distance_limit_status: { state: 1, distance_limit: 3000 } }
- **THEN** 无人机属性状态 DistanceLimitState/DistanceLimit 更新且回执 result=0

#### Scenario: 避障设置
- **WHEN** 收到 { obstacle_avoidance: { horizon: 0, upside: 1, downside: 1 } }
- **THEN** 无人机属性状态避障三项更新且回执 result=0

#### Scenario: 夜航灯/返航高度/失控动作设置
- **WHEN** 收到 night_lights_state（0/1）、rth_altitude（20-500）、rc_lost_action（0/1/2）合法值
- **THEN** 对应属性状态更新且回执 result=0

### Requirement: 机场属性设置
UAVMqttBridge MUST 支持机场属性 user_experience_improvement（0/1/2，对齐 PropertySetEnum.USER_EXPERIENCE_IMPROVEMENT），校验通过后写入机场属性状态。

#### Scenario: 用户体验改进计划设置
- **WHEN** 收到 { user_experience_improvement: { user_experience_improvement: 1 } }
- **THEN** 机场属性状态 UserExperienceImprovement 更新为 1 且回执 result=0

#### Scenario: 用户体验改进计划越界
- **WHEN** 收到 { user_experience_improvement: { user_experience_improvement: 9 } }
- **THEN** 机场属性状态不变，回执 result=1

### Requirement: OSD 属性联动
UAVMqttBridge MUST 在组装无人机 OSD 时使用属性状态替代硬编码：night_lights_state、height_limit、distance_limit_status（state/distance_limit/is_near_distance_limit）、obstacle_avoidance（horizon/upside/downside）、rc_lost_action、rth_altitude、exit_wayline_when_rc_lost；组装机场 OSD 时 user_experience_improvement 使用属性状态。默认值与属性状态结构默认值一致。

#### Scenario: 属性设置后 OSD 联动
- **WHEN** 设置 height_limit=120 且 rth_altitude=100 后重新组装无人机 OSD
- **THEN** OSD data.height_limit=120、data.rth_altitude=100

#### Scenario: 默认属性值输出
- **WHEN** 未收到任何属性设置时组装 OSD
- **THEN** 无人机 OSD height_limit=500、distance_limit_status.distance_limit=3000、obstacle_avoidance 全 1、rth_altitude=60、rc_lost_action=2、exit_wayline_when_rc_lost=1、night_lights_state=0；机场 OSD user_experience_improvement=2
