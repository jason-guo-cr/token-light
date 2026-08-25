# Token Light Companion Spec

**版本：** 0.1（TDD 初稿）  
**日期：** 2026-08-25  
**状态：** Host 与 firmware 软件范围已实现；真机验收待完成；PTT 未通过 Phase 5 门槛
**范围：** KEY + 三页面、Codex 状态、Token 宠物、燃速预测、板载温湿度、完成提示音与语音扩展

## 1. Executive Summary

Token Light 将从只显示时间、额度和 token 总量的静态仪表盘，升级为一个可交互、能感知 Codex 工作状态的桌面伙伴。用户通过板载 KEY 在三个页面间切换；设备展示 Codex 当前工作阶段、token 燃速和额度预测，由像素宠物提供环境化反馈，同时读取板载温湿度，并在任务完成时播放克制的提示音。板载语音交互纳入整体架构和后续交付阶段，但不作为第一阶段发布门槛。

## 2. Problem Statement

### 当前问题

- 当前屏幕数据丰富，但多数信息是静态结果，无法回答“Codex 现在在做什么”。
- 400x300 屏幕只使用单一页面，继续增加数据会降低可读性。
- 板载 KEY、SHTC3、音频 codec、双麦克风和扬声器尚未被利用。
- 原始 token 总量不能表达当前消耗是否异常，也不能预测额度是否会提前耗尽。
- Codex 完成任务时，用户仍需回到 Mac 查看，桌面设备没有形成反馈闭环。

### 目标用户

主要用户是长期在 Mac 上使用 Codex、同时把 Token Light 放在视线范围内的开发者。其核心任务是：不切换窗口即可知道 Codex 是否还在工作、token 消耗是否安全，以及何时需要回来处理结果。

🔶 **Assumption：** 单用户、单 Mac、单板子仍是未来两个版本的主要运行方式。

## 3. Goals

1. KEY 操作在不依赖 Mac 响应的情况下完成页面切换和专注计时。
2. Codex 状态变化在下一次 60 秒显示同步内出现在板子上。
3. 展示最近一小时 token 燃速，并在数据充足时给出额度重置时的使用率预测。
4. 像素宠物能用有限动画表达活动、额度和完成状态。
5. 使用板载 SHTC3 展示室内温湿度，并与室外天气明确区分。
6. Codex 任务完成时只播放一次提示音；为后续按住说话的语音交互预留协议和状态。
7. 不向 ESP32 发送提示词、回答、文件路径、命令或工具输出。

## 4. Non-goals

- 从板子取消、批准或修改 Codex 任务。
- 展示 prompt、回答摘要、文件名或 shell 命令。
- 同时选择或管理多个 Codex 任务。
- 在板子上配置 Wi-Fi、位置、专注时长或音量。
- 把语音录音保存到 TF 卡。
- 第一阶段支持唤醒词或常开麦克风。
- 用 token 数量精确换算 Codex 额度；两者是独立指标。
- 替代 Mac 端现有认证和额度同步机制。

## 5. Solution Overview

系统继续采用“Mac 聚合、ESP32 展示”的安全边界：

- Mac 读取本地 Codex session JSONL，计算活动状态、token 燃速和额度趋势。
- Mac 通过现有 USB 串口发送兼容旧协议的 snapshot。
- ESP32 读取 SHTC3、处理 KEY 手势、维护页面与专注计时、渲染宠物并播放完成提示音。
- 语音扩展优先复用 USB 连接；具体音频传输方式必须先完成 feasibility spike。

## 6. Information Architecture

### Page 1: OVERVIEW

保留现有时钟优先布局，并加入最适合扫一眼的信息：

```text
08/25 TUE  BJ RAIN 26C   LIVE  84%
┌────────────────────────────────┐
│             19:08              │
└────────────────────────────────┘
CODEX WEEK  76% LEFT   EST 68%
BURN 1.8M/H  PACE NORMAL
IN 25C 48%   DAY 18.8M  [PET]
```

- `BJ ...` 表示室外天气。
- `IN 25C 48%` 表示板载传感器测得的室内环境。
- `EST --` 表示预测样本不足，不得以 0 代替。
- 小宠物不得覆盖任何数值或状态文本。

### Page 2: CODEX NOW

```text
CODEX NOW                    LIVE

             TESTING
            TEST RUN
             03:42

         [LARGE TOKEN PET]

BURN 1.8M/H  WEEK 76%  EST 68%
```

- 大状态只使用固定词汇，不显示任务正文。
- 任务完成后显示 `DONE` 两分钟。
- 无任务显示 `IDLE / NO ACTIVE TASK`。
- 开放任务超过五分钟没有事件显示 `WAITING / NO RECENT EVENTS`。

### Page 3: FOCUS

```text
FOCUS                       25:00

             18:32
            RUNNING

          [FOCUS PET]

DOUBLE: PAUSE       HOLD: RESET
```

- 默认 25 分钟专注、5 分钟休息。
- v1 不持久化，重启后回到 25:00 READY。
- 专注计时完全在 ESP32 上运行，断开 Mac 后仍继续。

## 7. KEY Interaction Contract

KEY 为 GPIO18、低电平有效，配置 `INPUT_PULLUP`。

### 全局手势

- 短按：`OVERVIEW -> CODEX NOW -> FOCUS -> OVERVIEW`。
- 双击：切换专注计时的开始/暂停/恢复，并跳到 `FOCUS`。
- 长按：
  - `FOCUS` 页面：重置到 25:00 READY。
  - `OVERVIEW` 或 `CODEX NOW`：进入语音交互状态；在语音功能未启用时显示 `VOICE OFF`，不得误触发其他操作。

### 手势参数

- 去抖：30 ms。
- 双击窗口：第一次释放后的 350 ms。
- 长按阈值：800 ms。
- 双击不得额外产生短按。
- 长按不得额外产生短按。
- 页面切换应在手势确认后 100 ms 内完成。

🔶 **Assumption：** 350 ms 双击窗口和 800 ms 长按阈值具有可接受的手感，需真机验证。

## 8. Requirement R1 — KEY + Three-page Framework

### Functional Requirements

- 固件保存当前 `Page`、`FocusPhase`、`FocusRunState` 和剩余秒数。
- 页面状态只存 RAM；启动默认为 `OVERVIEW`。
- 页面渲染函数互相独立，不通过坐标条件分支堆叠在单个函数中。
- 未收到 `companion` 新字段时，三个页面仍能安全显示占位状态。
- 使用无符号 `millis()` 差值，正确处理计数器回绕。
- 专注完成切换到 5:00 BREAK READY；休息完成切回 25:00 FOCUS READY。

### Acceptance Criteria

- 三次短按完整循环三个页面。
- 双击可开始、暂停、恢复计时。
- 暂停期间剩余时间不变化。
- 长按重置专注状态。
- 断开 USB 不影响 KEY 和计时。
- `millis()` 在 `UINT32_MAX` 回绕时不跳时或提前完成。

## 9. Requirement R2 — Codex Current Task State

### Data Source

Mac 扫描：

```text
~/.codex/sessions/**/*.jsonl
~/.codex/archived_sessions/*.jsonl
```

文件与事件按事件时间排序，不依赖文件名或 mtime。坏 JSON、未知事件、读文件失败均被忽略。

### Public State Model

| State | Label | Detail | Trigger |
|---|---|---|---|
| `idle` | `IDLE` | `NO ACTIVE TASK` | 没有近期任务 |
| `thinking` | `THINKING` | `PLANNING` | task start、reasoning、agent message |
| `reading` | `READING` | `RESEARCH` | web/read/resource/view 工具 |
| `editing` | `EDITING` | `CODE CHANGE` | apply_patch 或编辑工具 |
| `testing` | `TESTING` | `TEST RUN` | 测试或构建命令 |
| `working` | `WORKING` | `TOOL RUN` | 其他工具调用 |
| `waiting` | `WAITING` | `NO RECENT EVENTS` | 开放任务五分钟无新事件 |
| `done` | `DONE` | `TASK COMPLETE` | task_complete，保留两分钟 |
| `error` | `STOPPED` | `TASK STOPPED` | turn_aborted，保留两分钟 |

测试/构建识别至少包含 `unittest`、`pytest`、PlatformIO、CTest、Cargo test、npm/pnpm/yarn test。命令只用于本地分类，不能进入返回值、日志或串口。

### Privacy Contract

ESP32 只接收：

```json
{
  "state": "testing",
  "label": "TESTING",
  "detail": "TEST RUN",
  "elapsed_seconds": 92,
  "completion_seq": 7
}
```

禁止发送 prompt、response、command、tool input/output、文件路径、thread/turn/call ID。`completion_seq` 是 Host 进程内单调递增的非敏感计数，用于避免重复提示音。

### Acceptance Criteria

- 所有九种状态均有确定映射和单元测试。
- 活动轮询跟随每分钟屏幕刷新，不跟随十分钟额度轮询。
- API quota 获取失败不影响活动状态更新。
- 缺失或损坏 session 日志时返回 IDLE，不使同步进程退出。
- 任意返回 payload 中都找不到输入命令和私有路径。

## 10. Requirement R3 — Token Pet

### Pose Priority

| Priority | Condition | Pose |
|---:|---|---|
| 1 | stopped/error | `alert` |
| 2 | task complete，30 秒内 | `celebrate` |
| 3 | focus timer running | `focus` |
| 4 | quota remaining <= 10% | `tired` |
| 5 | testing | `testing` |
| 6 | editing | `coding` |
| 7 | thinking/reading/working | `working` |
| 8 | waiting | `waiting` |
| 9 | idle | `sleep` |

### Rendering

- 每个 pose 两帧 1-bit sprite。
- 可见页面每 1000 ms 切帧。
- `OVERVIEW` 使用不超过 24x24 的 sprite。
- `CODEX NOW` 与 `FOCUS` 使用不超过 48x48 的 sprite。
- 只有 sprite frame 改变时才需要重绘；不在可见页面的动画不运行。
- 未知 pose 回退到 `sleep`。

### Acceptance Criteria

- 同时满足多个条件时严格遵循优先级。
- 完成庆祝在 30 秒后回到当前活动对应 pose。
- 低额度不能覆盖 error 和 completion 状态。
- 宠物关闭或 sprite 缺失不影响核心信息渲染。

## 11. Requirement R4 — Burn Rate and Consumption Forecast

### Metrics

系统明确区分两个不可互换的数据系列：

1. **Token burn**：本地 JSONL 中 token_count 事件之和。
2. **Quota burn**：Codex API 返回的 `used_percent` 随时间的变化。

不得通过 token 数量换算 quota 百分比。

### Token Burn

- `burn_60m`：`(now - 60m, now]` 内 token_count 总量。
- `burn_label`：紧凑格式，例如 `1.8M/H`。
- 没有事件时显示 `0/H`。
- 时间边界使用事件完整 timestamp，不只使用 date。

### Quota Forecast

Host 将成功额度样本持久化到：

```text
~/.cache/token-light/quota-history.jsonl
```

每条只包含 `observed_at`、`limit_id`、`reset_at`、`used_percent`。不保存认证信息。

- 只使用与当前 `limit_id + reset_at` 相同窗口的样本。
- 使用最近 6 小时内的样本估算百分比点/小时。
- 至少需要两个样本，且时间跨度至少 30 分钟。
- 负斜率按 0 处理。
- `projected_used_percent = current_used + slope * hours_to_reset`，范围限制为 0..100。
- 样本不足时 `projected_used_percent = null`、`forecast_label = "EST --"`。

### Pace Labels

🔶 **Assumption：** v1 使用以下阈值，实际体验后再校准。

| Quota slope | Pace |
|---:|---|
| `< 0.5 pp/h` | `COOL` |
| `0.5..<2 pp/h` | `NORMAL` |
| `2..<5 pp/h` | `HOT` |
| `>= 5 pp/h` | `MELTDOWN` |
| 样本不足 | `UNKNOWN` |

### Acceptance Criteria

- 60 分钟窗口边界、跨时区和坏事件均有测试。
- 不同 reset window 的历史样本不得混用。
- 预测值不会小于当前 used，也不会超过 100。
- 历史文件损坏时丢弃坏行并继续。
- `EST --` 与真实 `0%` 可区分。

## 12. Requirement R5 — Onboard Temperature and Humidity

### Hardware and Sampling

- 使用板载 SHTC3，I2C 地址 `0x70`。
- I2C：SDA GPIO13，SCL GPIO14。
- 每 60 秒采样一次；首次渲染前允许显示 `IN --C --%`。
- 温度显示为四舍五入整数摄氏度，湿度显示为四舍五入整数百分比。
- 室内字段始终使用 `IN` 前缀，避免与北京室外天气混淆。

### Validity and Failure Handling

- 有效范围：温度 -40..125C，湿度 0..100%。
- 单次读取失败保留最后一次有效值并标记 cached。
- 最后有效值超过 5 分钟时标记 stale，并显示 `IN --C --%`。
- 传感器错误不得阻塞 display、KEY、serial 或 audio loop。

### Acceptance Criteria

- 正常值、边界值、NaN、越界值、短时失败和 stale 均有测试。
- 连续 24 小时传感器读取失败不会导致固件 watchdog reset。
- Host 室外天气失败不影响室内温湿度，反之亦然。

## 13. Requirement R6 — Completion Audio and Voice Extension

### R6a: Completion Audio（正式交付范围）

- 当 `completion_seq` 首次增加时播放一次完成音。
- 相同 snapshot 重复发送不得重复播放。
- 启动时收到非零 completion_seq 只建立基线，不播放历史完成音。
- 提示音总时长不超过 1.5 秒，不循环。
- 22:00..08:00 默认静音；可通过 Host 参数 `--audio-always` 覆盖。
- `error` 默认不播放完成音，未来可增加独立错误音。
- 播放失败不得阻塞渲染或串口接收。

🔶 **Assumption：** 默认静音时段为上海本地时间 22:00..08:00。

### R6b: Push-to-talk Voice（规划范围，非首发门槛）

- 仅在 `OVERVIEW` 或 `CODEX NOW` 长按 KEY 时进入。
- 超过 800 ms 后显示 `LISTENING`，松开后显示 `SENDING`。
- 最长录音 15 秒，超时自动结束。
- 不使用常开麦克风和唤醒词。
- 原始音频不得写入 TF 卡；请求完成或失败后从 RAM 清除。
- Host 返回最多两行、每行不超过 24 个 ASCII 字符的板端摘要；完整回答仍留在 Mac/Codex。
- 语音功能不可用时显示 `VOICE OFF`，随后返回原页面。

🔵 **Open Question：** 音频通过高波特率 USB CDC、独立 USB Audio Class，还是局域网传输。实施 R6b 前必须先完成吞吐、延迟和驱动冲突 spike。

### Acceptance Criteria

- 完成音对同一 completion_seq 至多播放一次。
- quiet hours、Host override 和初始化基线均有测试。
- 音频播放期间每轮主循环仍能处理串口和 KEY。
- Voice spike 达不到“松开后 3 秒内进入 SENDING、端到端 10 秒内显示结果”的目标时，R6b 不进入正式实现。

## 14. Serial Protocol v2

现有 snapshot 增加可选字段，旧 firmware 可直接忽略：

```json
{
  "type": "snapshot",
  "time": "19:08",
  "status": "live",
  "primary": {
    "used_percent": 24,
    "remaining_percent": 76,
    "reset_at": 1788170400
  },
  "token_usage": {
    "today_label": "18.8M",
    "week_label": "95.9M",
    "burn_60m": 1800000,
    "burn_label": "1.8M/H"
  },
  "forecast": {
    "pace": "normal",
    "pace_label": "NORMAL",
    "quota_points_per_hour": 0.8,
    "projected_used_percent": 32,
    "forecast_label": "EST 32%"
  },
  "companion": {
    "activity": {
      "state": "testing",
      "label": "TESTING",
      "detail": "TEST RUN",
      "elapsed_seconds": 92,
      "completion_seq": 7
    },
    "pet": {
      "pose": "testing",
      "frame_count": 2,
      "frame_period_ms": 1000
    }
  },
  "audio": {
    "enabled": true,
    "quiet": false
  }
}
```

Protocol constraints:

- 整行 JSON 继续小于现有 1200-byte 接收限制。
- label/detail 均为固定白名单 ASCII 文本。
- 所有新增顶层对象可选。
- 数字字段类型错误时忽略该字段，保留最后有效值。
- error/cached snapshot 仍携带 token、forecast 和 companion 的最后有效数据。

## 15. Technical Architecture

### Host Additions

- `activity.py`：session 扫描、事件排序、公开状态分类。
- `burn_rate.py`：60 分钟 token burn、quota history、线性预测。
- `companion.py`：宠物 pose 和协议 payload。
- `quota_history.py`：原子追加与裁剪历史，最多保留 14 天。
- CLI：活动每分钟轮询；quota history 只在成功额度轮询后写入。

### Firmware Additions

- `companion_controller.h/.cpp`：页面和专注计时纯状态机。
- `key_gesture.h/.cpp`：按键去抖与手势识别纯状态机。
- `ambient_model.h/.cpp`：有效值、cache、stale 状态机。
- `shtc3_reader.h/.cpp`：硬件 I2C adapter。
- `completion_notifier.h/.cpp`：completion_seq、quiet hours 与一次性播放决策。
- `audio_driver.h/.cpp`：ES8311/I2S 的非阻塞播放 adapter。
- `pet_sprites.h`：1-bit sprite assets。
- 按页面拆分 renderer，避免单文件坐标逻辑继续膨胀。

所有 controller/model 必须不依赖 Arduino `String`、U8g2、Serial 和具体 GPIO，使其可在 PlatformIO native 环境测试。

## 16. Delivery Plan

### Phase 0 — Test Harness and Protocol

- 增加 Host RED tests 与 PlatformIO native tests。
- 冻结 Serial Protocol v2 和 privacy contract。
- 完成旧 snapshot 兼容测试。

### Phase 1 — KEY + Three Pages

- 实现 page/focus controller、gesture detector 和三页面空框架。
- 真机验证按键手感、刷新速度和无 Host 运行。

### Phase 2 — Activity + Pet + Completion Sound

- 实现 session classifier、completion_seq、宠物 pose 和非阻塞提示音。
- 完成 privacy audit。

### Phase 3 — Burn Rate and Forecast

- 扩展 token timestamp 数据、quota history 和预测。
- 至少积累 24 小时真实历史后评估 pace 阈值。

### Phase 4 — Onboard Ambient Sensor

- 接入 SHTC3，完成 cached/stale UI。
- 进行 24 小时稳定性运行。

### Phase 5 — Voice Feasibility and Optional Delivery

- 测试 USB/网络音频传输候选方案。
- 满足延迟、隐私和稳定性门槛后，才实现 PTT 完整链路。

## 17. TDD Plan

### Host RED Tests

| File | Coverage |
|---|---|
| `tests/test_activity.py` | 九种状态、timeout、坏 JSON、跨目录排序、隐私 |
| `tests/test_burn_rate.py` | 60m window、格式、quota slope、reset filtering、clamp |
| `tests/test_companion.py` | pose 映射、优先级、payload contract |
| `tests/test_companion_snapshot.py` | optional protocol、cached/error 保留、旧协议兼容 |

### Firmware Native RED Tests

| Suite | Coverage |
|---|---|
| `test_companion_controller` | 页面循环、专注 start/pause/resume/reset、完成、millis wrap |
| `test_key_gesture` | debounce、short/double/long 互斥、context action |
| `test_ambient_model` | valid/cache/stale/NaN/range |
| `test_completion_notifier` | baseline、exactly-once、quiet hours、override |

### Hardware Tests

- SHTC3、ES8311、speaker 和 microphone driver 只在真机验收，不在 native test 中模拟驱动时序。
- 每个硬件 adapter 必须注入到已测试的 model/controller 中，业务规则不得留在 adapter。

## 18. Success Metrics

### Primary

- 连续 7 天运行中，95% 的 Codex 状态变化在 65 秒内正确显示，且 Host/firmware 无崩溃。

### Secondary

- KEY 手势真机测试 100 次，误识别不超过 1 次。
- completion_seq 重复 snapshot 测试中，重复提示音为 0。
- 有足够历史样本时 forecast 可用率 >= 90%。
- 室内温湿度有效数据可用率 >= 99%。
- 三页面任何页面无文字重叠或越界。

### Guardrails

- Serial payload < 1200 bytes。
- 不传输 privacy contract 禁止的数据。
- 主循环不得因传感器或音频单次操作阻塞超过 20 ms。
- 新功能关闭时，现有 overview 功能和测试不回归。

## 19. Dependencies and Risks

- **Codex JSONL schema 变化**：分类可能失效。
  - Mitigation：容错 parser、未知事件回退、fixture 覆盖真实最小 schema。
- **预测误导**：线性斜率不能代表未来工作负载。
  - Mitigation：样本不足显示 `EST --`，明确为趋势而非保证。
- **RLCD 频繁刷新观感**：宠物动画可能造成闪烁。
  - Mitigation：两帧、1 Hz 上限，只在可见时刷新；真机决定是否降到 0.5 Hz。
- **按键手势过载**：短/双/长按不易记忆。
  - Mitigation：每页底部显示当前可用操作，语音不可用时明确反馈。
- **音频阻塞主循环**：codec 初始化或播放影响 serial。
  - Mitigation：非阻塞 driver、固定短音、controller 与 adapter 分离。
- **语音链路工程风险高**：USB throughput 或 driver 冲突。
  - Mitigation：Phase 5 spike；失败不影响 completion sound 交付。
- **温度受板载发热影响**：SHTC3 可能高于实际室温。
  - Mitigation：先记录与独立温度计偏差，再决定是否提供校准 offset。

## 20. Open Questions

1. 🔵 语音音频最终采用 USB CDC、USB Audio Class 还是局域网传输？
2. 🔵 板载扬声器是否已经安装，还是需要外接到 MX1.25 接口？
3. 🔵 quiet hours 是否固定为 22:00..08:00，还是只提供 CLI 开关？
4. 🔵 SHTC3 是否需要持久化校准 offset？
5. 🔵 Token 宠物最终造型由手绘 sprite 还是 imagegen 生成后再像素化？
6. 🔵 Phase 3 积累真实数据后，pace 阈值是否需要按 plan 类型调整？

## 21. PRD Self-assessment

- **最强部分：** 技术边界、隐私协议、六项需求的阶段依赖和可测试性已经明确。
- **最弱部分：** Voice transport 尚未完成 feasibility 验证；它是显式开放问题，不应提前承诺实现方案。
- **最高风险假设：** pace 阈值、KEY 手势手感、SHTC3 板载热偏差。
- **下一步：** 经授权后烧录真机，完成 KEY 100 次、SHTC3 24 小时、完成音与页面边界验收；积累 24 小时 quota 历史后复核 pace 阈值。Phase 5 结论见 `../spikes/2026-08-25-token-light-voice-feasibility.md`。
