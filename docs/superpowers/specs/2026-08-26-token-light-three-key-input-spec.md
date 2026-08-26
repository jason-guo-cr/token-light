# Token Light 三键输入规格

**版本：** 1.0
**日期：** 2026-08-26
**状态：** 已确认，待实现
**范围：** ESP32 固件的本地输入、页面导航、专注计时控制与对应验收；不改变 Host、串口 snapshot 或隐私协议。

## 1. 决策与适用边界

本规格取代 `2026-08-25-token-light-companion-spec.md` 的“7. KEY Interaction Contract”及 R1 中所有单键手势验收，作为下一次固件输入改造的唯一行为契约。当前代码和 README 仍描述旧的 GPIO18 单键短按/双击/长按行为；它们在实现本规格前仍是现状，而不是目标行为。

目标交互是三个有空间标签的按键，而不是把三个手势叠加到一个键：

- 左键只做“上一页”，右键只做“下一页”；两个动作均立即可见。
- 中键只做专注计时的主动作（开始、暂停、恢复）；它是唯一允许长按的键。
- 双击不再绑定任何功能，不能为任意单击引入等待窗口。

这适用于日常、高频、需要盲操或注意力被占用的场景。单键多手势仅可作为 `legacy_single_key` 硬件档案的兼容行为，不得作为三键产品档案的回退逻辑。

## 2. 硬件前提与输入档案

### 2.1 已核实的板载能力

当前项目的目标板是 Waveshare ESP32-S3-RLCD-4.2。官方资料只将 `KEY`（GPIO18，低有效）列为可定制按键；`BOOT`（GPIO0）是启动下载按键，`PWR` 是电源按键，不是运行时的第三个通用输入。[官方 GPIO/按键说明](https://docs.waveshare.com/ESP32-ESPHome-Tutorials/Example-RLCD-Voice)；[产品板载说明](https://www.waveshare.com/product/arduino/boards-kits/esp32-s3/esp32-s3-rlcd-4.2.htm)。

因此，不能把 BOOT 或 PWR 临时当作左/中/右键来宣称完成本规格：这会破坏下载或电源语义。三键档案要求通过扩展排针或已批准的外接按键板提供三个独立、可读的 GPIO 输入。实施前必须把实物接线、GPIO 编号和是否有外部上拉写入板级配置并经真机确认；本规格故意不臆造尚未提供的引脚号。

### 2.2 必须支持的输入档案

| 档案 | 适用硬件 | 行为 |
|---|---|---|
| `three_key` | 三个独立的外接/板级输入已接线并验证 | 本规格第 3–7 节的目标行为；发布默认档案。 |
| `legacy_single_key` | 已部署、只有 GPIO18 KEY 可用的设备 | 保持现有短按换页、双击控制专注、长按重置/VOICE OFF 行为，直至硬件升级。 |

档案由构建时板级配置显式选择，不能根据“某引脚当前读到高电平”自动猜测。未接线的 GPIO 可能始终为高或浮动，自动回退会产生不可重复的行为。

对 `three_key`，固件应以 `INPUT_PULLUP` 读取三个低有效输入；若接线已提供外部上拉，仍须在板级配置中明确，不得依赖悬空电平。输入层向业务层只暴露逻辑 `Left`、`Center`、`Right` 事件，controller、renderer 和 native 测试不得依赖实际 GPIO。

## 3. 三键功能映射

| 逻辑键 / 标识 | 短按（唯一主动作） | 中键长按 | 双击 |
|---|---|---|---|
| 左键 `Left` / `PREV` | 切到上一页：`OVERVIEW ← CODEX NOW ← FOCUS ← OVERVIEW` | 不定义；松开后仍只执行一次“上一页” | 不定义 |
| 中键 `Center` / `FOCUS` | 控制专注计时：`READY → RUNNING`，`RUNNING → PAUSED`，`PAUSED → RUNNING`；同时切到 `FOCUS` 页面 | 仅在 `FOCUS` 页重置为 `FOCUS / READY / 25:00`；在其他页只显示 `VOICE OFF`，不启动录音或网络功能 | 不定义 |
| 右键 `Right` / `NEXT` | 切到下一页：`OVERVIEW → CODEX NOW → FOCUS → OVERVIEW` | 不定义；松开后仍只执行一次“下一页” | 不定义 |

`Center` 的短按从任意页面都作用于同一个板载专注计时器，并在动作后显示 `FOCUS` 页。这保留“主动作一按即达”的价值，也避免用户先导航、再启动的两步操作。导航不会改变计时器状态；定时器仍在断开 USB 时运行。

中键长按的 reset 是低频、明显的二级动作：仅在用户已位于 `FOCUS` 页时可用，按住达到阈值即执行，且不需要第二次按键确认。它不会触发中心键的开始/暂停/恢复。语音能力尚未通过 feasibility gate，故非 FOCUS 页的中键长按只能给出 `VOICE OFF` 反馈，不能保存或发送音频。

## 4. 事件、时序与并发规则

### 4.1 单键时序

| 参数 | 值 | 规则 |
|---|---:|---|
| 电平 | 低电平为按下 | 读取使用板级配置指定的上拉模式。 |
| 去抖 | 30 ms | 原始电平连续 30 ms 一致后才形成稳定 press/release 边沿。 |
| 中键长按阈值 | 800 ms | 从去抖后的稳定按下开始计时；达到阈值立刻发出一次 `CenterLong`。 |
| 短按确认 | 稳定释放时 | 对左/右键，任何未发生其他输入冲突的稳定释放产生一次导航；对中键，只有尚未发出长按时才产生 `CenterShort`。 |
| 双击窗口 | 无 | 不保留、不开启、也不等待第二击。 |
| 反馈延迟 | 不超过 100 ms | 从被确认的事件（稳定 release 或长按阈值）到 controller 状态更新和下一帧可见反馈的上限。 |

所有时间比较必须使用无符号 `uint32_t` 差值，因此 `millis()` 从 `UINT32_MAX` 回绕后，去抖和长按阈值仍正确。按住中键超过 800 ms 后，在释放时不得额外产生短按；长按在同一次按下周期内至多发出一次。

左/右键不识别长按：用户即使按住再释放，也只得到一次导航。这避免了把未定义的节奏变成隐藏命令，同时保证按键不会“无响应”。

### 4.2 多键与异常输入

- 不支持组合键。第一个完成去抖的按下拥有本次输入周期；在它释放且所有键恢复稳定未按下前，其他键的变化被忽略。
- 若两个按下在同一采样周期同时稳定，按 `Left → Center → Right` 的固定优先级选择一个；该规则只用于可复现测试，不应被当作用户功能。
- 去抖期间的抖动、重复 press、重复 release、超过阈值后的持续按住都不得产生重复业务动作。
- 启动时所有输入先完成一次 30 ms 稳定采样，不能把上电时已按住的按键解释为短按；中键若持续按住可在稳定后按正常 800 ms 长按规则处理。
- 输入处理不得阻塞串口、传感器、音频或 1 秒计时 tick；单轮输入处理预算不超过 5 ms。

## 5. 状态与反馈契约

### 5.1 专注计时状态机

| 事件 | 前置状态 | 后置状态 | 页面 | 反馈文案 |
|---|---|---|---|---|
| `CenterShort` | `READY` | `RUNNING` | `FOCUS` | `FOCUS STARTED` |
| `CenterShort` | `RUNNING` | `PAUSED` | `FOCUS` | `FOCUS PAUSED` |
| `CenterShort` | `PAUSED` | `RUNNING` | `FOCUS` | `FOCUS RESUMED` |
| `CenterLong` | `FOCUS` 页面，任意 run state | `FOCUS / READY / 25:00` | `FOCUS` | `FOCUS RESET` |
| `CenterLong` | `OVERVIEW` 或 `CODEX NOW` | 不变 | 不变 | `VOICE OFF` |
| `LeftShort` / `RightShort` | 任意 | 计时状态不变 | 相邻页面 | `PAGE: OVERVIEW`、`PAGE: CODEX NOW` 或 `PAGE: FOCUS` |

`FOCUS` 完成和 `BREAK` 完成的既有自动相位切换保持不变：完成后切换相位、装载 5:00 或 25:00、状态为 `READY`。在 `PAUSED` 时，剩余时间不变；页面切换也不暂停或恢复计时。

### 5.2 可见与声学反馈

- 每次已接受的业务事件在目标页面显示一条动作反馈，至少持续 800 ms，最多 1,200 ms；常规页面内容随后恢复。反馈必须与状态变更同帧或下一帧出现。
- 页面底部帮助文字必须更新为三键可发现标签，例如 `LEFT: PREV  CENTER: FOCUS  RIGHT: NEXT`；`FOCUS` 页可补充 `HOLD CENTER: RESET`。不得再显示 `DOUBLE:`、`SHORT:` 或把 VOICE 写成已可用功能。
- 本改造不新增按键提示音。现有 Codex completion 音、静音时段和音频占用策略保持不变，按键反馈以屏幕为准。
- 中键长按进度可选显示为 0–800 ms 的填充提示；若未实现，800 ms 阈值后的 `FOCUS RESET`/`VOICE OFF` 反馈仍是必需项。

## 6. 兼容与迁移要求

1. Host CLI、snapshot 协议、privacy contract、页面枚举及专注计时长度不因输入改造而变化。旧 snapshot 必须仍能驱动三个页面。
2. `legacy_single_key` 的行为和旧测试保持；`three_key` 不得调用旧 `KeyGestureDetector` 的双击路径，也不得因单击等待 350 ms。
3. 输入业务状态应从 `CompanionController` 的旧 `onShortPress/onDoublePress/onLongPress` 演进为明确的上一页、下一页、中心短按、中心长按动作，或由等价的逻辑事件适配层调用。命名和测试必须能看出空间语义，不能以“第 1/2/3 个 gesture”隐藏映射。
4. 引脚定义与 profile 选择集中在板级输入配置；禁止在 renderer/controller 中读取 GPIO，也禁止散落 `#define` 引脚号。
5. 因实际三键接线尚未给出，合并 `three_key` 为默认烧录档案前必须完成硬件 gate：记录接线图、验证三个输入的低有效空闲电平、确认不占用显示/I2C/I2S/USB/启动引脚，并完成第 7.2 节真机用例。该 gate 未通过时只可发布 `legacy_single_key`。

## 7. 自动化验收标准

### 7.1 PlatformIO native 单元测试（必须）

新增或替换纯 C++ 输入状态机测试；每项均应能在 `pio test -d firmware -e native` 中自动执行：

| ID | 可自动化场景 | 通过条件 |
|---|---|---|
| A1 | 左键和右键完整环形导航 | 从每个页面触发 Left/Right 后分别到正确相邻页；六种转移都覆盖。 |
| A2 | 中键主动作 | `READY → RUNNING → PAUSED → RUNNING`，每次均切至 `FOCUS`，且剩余时间按既有 tick 规则变化。 |
| A3 | 中键长按互斥 | 800 ms 时恰好一次 reset；释放和后续采样不产生 start/pause/resume。 |
| A4 | 非 FOCUS 长按 | 在 `OVERVIEW`、`CODEX NOW` 分别产生 `VOICE OFF` 反馈意图，页面、计时状态和串口状态不变。 |
| A5 | 无双击等待 | 左/右/中键的单次稳定释放在 100 ms 以内产生业务事件；测试中不存在 350 ms pending-short 状态或 double 事件。 |
| A6 | 去抖与重复抑制 | 30 ms 内 bounce 不产生事件；一次干净按下/释放只产生一个动作；长按与持续按住不重复触发。 |
| A7 | 同时按键仲裁 | 双键/三键重叠只接受一个所有者事件，按 `Left → Center → Right` 优先级，释放前不接受第二动作。 |
| A8 | 时间回绕 | 去抖和中键长按跨 `UINT32_MAX` 仍在 30 ms/800 ms 边界正确触发一次。 |
| A9 | 计时隔离 | 导航、无效长按、断开 Host（以不输入 snapshot 模拟）都不改变运行中的剩余时间；暂停时不减少。 |
| A10 | 档案隔离 | `three_key` 测试不依赖双击；`legacy_single_key` 保留现有 gesture 契约并可独立构建。 |

测试可以通过注入逻辑按键电平、时钟和反馈 sink 完成，不得依赖真实 GPIO、U8g2、Serial 或睡眠等待。测试应同时断言 controller 状态与反馈意图，避免只检查最终页面而漏掉误触。

### 7.2 真机验收（硬件 gate，必须）

在已记录的三键接线上，用 100 次/键的脚本化或人工计数测试。每键 100 次中：正确动作至少 99 次、错误动作 0 次、漏识别不超过 1 次；中键长按 reset 100 次中不得出现短按动作。记录固件版本、GPIO 配置、按键位置和原始结果。

同时验证：

- 三键空闲时都为稳定高电平，按下均为稳定低电平；未触碰 BOOT/PWR 不改变下载和供电行为。
- 从稳定 release/800 ms 阈值到显示反馈不超过 100 ms；用串口调试时间戳或录屏逐帧测量。
- 连续 10 分钟运行、串口持续接收 snapshot、音频 completion 触发和三键操作并发时，不重启、不丢失专注计时、不出现卡死。
- 物理标签与屏幕帮助文案一致：左为 PREV，中为 FOCUS，右为 NEXT。

## 8. 实施完成定义

实现完成必须同时满足：本规格第 7.1 节 native 测试全绿、`pio run -d firmware` 成功、README 和旧 companion spec 的用户可见按键说明已更新为三键目标并保留 legacy 注释、以及第 7.2 节硬件 gate 有可追溯记录。若外接三键硬件尚未提供，软件可完成可注入的 `three_key` 逻辑和测试，但发布结论必须明确标为“硬件 gate 待完成”，不得宣称已在原厂板载三键上验收。
