---
title: "ProjectRift 从 v0.4.7 到 Steam 1.0 完整开发路线图"
subtitle: "UE5.8 + Codex 辅助开发 | 已完成基线 v0.1.0-v0.4.7 | 下一版本 v0.4.8"
author: "OpenAI"
date: "2026-07-10"
CJKmainfont: "Noto Sans CJK SC"
mainfont: "Noto Sans CJK SC"
monofont: "Noto Sans Mono CJK SC"
geometry: "margin=1.8cm"
fontsize: 9.5pt
toc: true
toc-depth: 2
numbersections: true
colorlinks: true
header-includes:
  - |
    \usepackage{fancyhdr}
    \usepackage{longtable}
    \usepackage{booktabs}
    \usepackage{xcolor}
    \usepackage{enumitem}
    \setlength{\parindent}{0pt}
    \setlength{\parskip}{0.35em}
    \setlength{\headheight}{14pt}
    \emergencystretch=3em
    \pagestyle{fancy}
    \fancyhf{}
    \fancyhead[L]{ProjectRift 完整 Steam 发行路线图}
    \fancyhead[R]{基线 v0.4.7}
    \fancyfoot[C]{\thepage}
---

# 修订说明

这是对上一版路线图的基线修正。项目实际已经完成到 **v0.4.7**，而不是 v0.4.0。因此本版执行以下调整：

- `v0.1.0-v0.4.7` 全部视为已经完成并冻结的历史版本，不再列为后续 Codex 开发任务。
- 已有的任务目标、刷怪器、基础敌人、掉落拾取、撤离、结算、资源带出以及大厅-裂隙-大厅旅行闭环都应保留。
- 第一项新开发任务改为 **v0.4.8 Vertical Slice 稳定化与打包基线**。
- v0.5.0 之后的路线继续面向完整商业游戏，但所有改动必须以现有 v0.4.7 实现为基础，通过兼容接口、适配器和增量扩展完成。
- 不回写或重排既有 Git 历史；本路线中的版本号只表示接下来的产品里程碑。

# 文档定位

本路线图替代原来的“两个月 MVP”时间表，将 ProjectRift 从当前已经完成的 **v0.4.7 可玩闭环**，继续推进到可公开销售、可完整通关、可更新和可长期维护的 **Steam v1.0 正式版**。

旧文档中的世界观、核心循环、GAS、网络背包、舰船修复和多人合作方向继续保留。新的重点不再是“短期做出作品集 Demo”，而是建立完整游戏所需的生产架构、内容管线、存档兼容、真实 Steam 联机、质量保障、商店审核和发布维护流程。

本路线图不规定必须用多少个月。只有当前版本的验收门槛全部通过，才进入下一版本。一个版本可以拆成多个 Codex Task Card 和多个 Git commit，但只有全部验收通过后才创建版本标签。

> **重要：不要重命名现有 UE 工程、C++ 模块或已经被蓝图引用的类。** 设计代号仍写作 ProjectRift；如果实际仓库目前叫 `ProjectA`，就继续保留 `ProjectA`。本文中的 `APR/UPR/FPR` 只是示例前缀，Codex 必须先检查当前仓库中的真实命名并沿用，禁止批量改名。

# 已完成基线与冻结规则

## 已完成版本摘要

| 已完成版本族 | 已具备能力 | 后续处理原则 |
|---|---|---|
| v0.1.x | C++ 项目、Git、基础地图、Game Framework、输入、多人移动、大厅准备和地图旅行入口 | 不重建工程骨架，不重写 CharacterMovement |
| v0.2.x | GAS、ASC/AttributeSet、伤害、死亡复活、突击能力和输入 Tag | 只做兼容扩展，不另建第二套 GAS 框架 |
| v0.3.x | 物品数据、网络背包、FastArray、拾取、消耗品、丢弃和掉落 | 保留现有 ItemId 和复制协议，后续做版本化迁移 |
| v0.4.0-v0.4.7 | Rift GameMode/GameState、守点目标、刷怪、基础敌人、撤离、结算、资源带出和完整往返闭环 | 作为当前冻结产品基线，下一步只做稳定化 |

旧路线中 v0.4.7 的完整流程是：

```text
进入舰船大厅
-> 玩家准备、房主开始任务
-> ServerTravel 到裂隙
-> 完成守点目标
-> 战斗、掉落与拾取资源
-> 开启撤离点
-> 撤离结算
-> 返回舰船大厅
```

用户已经确认这一版本完成，因此新路线不要求重新实现 v0.4.1-v0.4.7。

## 开始新路线前的非破坏性基线操作

```text
1. 确认工作区干净，并为当前提交建立 Git 标签 baseline-v0.4.7。
2. 保存一份当前 Development Editor 完整编译日志。
3. 单人 PIE 跑通一次完整闭环。
4. 2 Players Listen Server 跑通一次完整闭环。
5. 记录已知问题、地图名、关键类名和现有存档格式。
6. 不因为命名、格式或“代码看起来不够漂亮”而重构已完成系统。
```

后续允许修改旧代码的情况只有四种：

- 新功能确实需要一个向后兼容的接口扩展。
- 发现可稳定复现的 Bug，并先有复现步骤或回归测试保护修复结果。
- 打包、Steam、存档兼容、性能或安全要求证明旧实现无法满足发布标准。
- 旧代码存在阻断新版本的生命周期问题，例如重复结算、委托泄漏、旅行后状态残留或客户端可修改权威数据。

修改旧系统时优先增加适配层、迁移函数或新接口；不能直接删除旧接口并让已有蓝图、DataAsset 或存档失效。

# v1.0 产品定义

## 核心定位

ProjectRift 是一款支持单人和 1-4 人合作的第三人称科幻 PVE 撤离游戏。执勤小队在异星迫降后，通过反复进入裂隙执行任务、搜集资源、带出战利品、修复舰船和解锁装备，最终修复跃迁核心并返航。

正式版继续使用以下循环：

```text
舰船大厅准备
-> 选择任务、职能模块与装备
-> 进入裂隙
-> 探索、战斗、完成主目标与可选目标
-> 决定继续搜刮还是撤离
-> 结算个人奖励和团队推进
-> 修复舰船、制造装备、强化角色
-> 解锁下一章裂隙
-> 最终 Boss 与返航结局
```

## 建议的 1.0 内容上限

| 内容维度 | 1.0 目标 |
|---|---|
| 玩家人数 | 1-4 人；Steam Listen Server；单人可完整通关 |
| 主线长度 | 首次通关约 8-12 小时；可重复任务提供更长游玩时间 |
| 职能模块 | 突击、工程、医疗/净化、侦察，共 4 套 |
| 主动技能 | 每个职能 3 个主动技能，并有 1 个被动特性 |
| 裂隙章节 | 能源、金属废墟、生物巢穴、科技设施、深层裂隙，共 5 章 |
| 任务类型 | 守点、采集、搬运、摧毁、猎杀，外加可选目标与任务修正 |
| 敌人 | 至少 8 种普通敌人、4 种精英敌人、5 个章节 Boss |
| 装备 | 4 个武器家族、约 12 个武器变体、护甲、芯片和工具模块 |
| 物品 | 约 60 个稳定 ItemId，含装备、消耗品、材料和任务物品 |
| 舰船成长 | 动力炉、医疗舱、通讯阵列、武器库、跃迁核心 5 个模块 |
| 难度 | 至少 3 档任务难度；按 1/2/3/4 人动态缩放 |
| 语言 | 简体中文与英语首发 |
| 平台 | Windows 首发；完整键鼠与手柄支持；Steam Deck 以可玩性测试为目标 |
| Steam 功能 | 大厅/邀请、成就、统计、Rich Presence、云存档、Playtest、分支与回滚 |

这些数字是控制范围的上限，而不是鼓励继续增加内容。未经正式变更评审，不在 1.0 中加入开放世界、PVP、交易市场、微交易、复杂飞船驾驶、完整随机地牢、Dedicated Server、Host Migration 或长期服务型内容。

## “完整游戏”完成标准

只有同时满足以下条件，项目才可以进入 v1.0：

1. 从新游戏、教学、五章主线到最终返航结局可以完整跑通。
2. 单人、2 人和 4 人均不存在因为缺少某职业而无法完成的目标。
3. 掉落、背包、装备、制造、舰船修复和存档组成稳定成长闭环。
4. Steam 创建/加入/邀请、地图切换、掉线处理和返回大厅流程可用于真实玩家。
5. 主菜单、设置、手柄、字幕、语言、音量、画质和按键重绑定达到商业软件基本标准。
6. Development 与 Shipping 构建都能安装、启动、游玩、更新和回滚。
7. 完成内容、性能、网络、存档兼容、可访问性和本地化测试。
8. 商店页、法务资料、版权清单、预告片、截图、成就、支持渠道和发布流程准备完成。

# 总体版本规划

| 版本族 | 核心目标 | 里程碑出口 |
|---|---|---|
| v0.4.8 | 冻结并稳定现有 v0.4.7 闭环 | 可打包、可重复试玩的 Vertical Slice |
| v0.5.x | 数据驱动、存档、舰船成长、开发工具与自动测试 | 具备可持续扩展的生产基础 |
| v0.6.x | 完整战斗、武器、四职能、倒地救援与单人补偿 | 核心战斗 Feature Complete |
| v0.7.x | 商业级背包、装备、词条、掉落与制造 | 完整经济与 Build 闭环 |
| v0.8.x | 任务框架、风险机制、遭遇导演、敌人生态与 Boss 框架 | 可批量生产任务和敌人内容 |
| v0.9.x | 舰船大厅、五章地图、五个 Boss、剧情和结局 | 主线从头到尾可玩 |
| v0.10.x | UI、设置、手柄、可访问性、本地化与教学 | 面向普通玩家可理解、可操作 |
| v0.11.x | Steam 联机、平台功能、SteamPipe 与分支 | Steam 平台链路完整 |
| v0.12.x | Alpha、诊断、自动化、Steam Playtest | Feature Complete Alpha 退出 |
| v0.13.x | 内容锁定、平衡、本地化 QA、Beta | Content Complete Beta 退出 |
| v0.14.x | 性能、网络、加载、包体、兼容和最终打磨 | Release Candidate 门槛通过 |
| v0.15.x | 商店审核、RC 修复、发布演练与 Gold Master | 获得可发布构建 |
| v1.0.x | 正式发布与热修复 | 商业版本稳定运行 |

# Codex 总体执行协议

## 每个版本的固定工作流

```text
1. 从干净 Git 状态创建 feature 分支。
2. 让 Codex 先只读检查现有实现，输出计划，不立刻改代码。
3. 确认它识别了真实模块名、真实类名、现有接口、蓝图依赖和 v0.4.7 已完成范围。
4. 只实现当前版本，不顺带重构下一版本，也不重新实现已完成版本。
5. Codex 输出：修改文件、设计说明、UE 手工步骤、测试步骤、已知风险。
6. 涉及 .h、UCLASS/USTRUCT、Build.cs、Target.cs 或插件时，关闭 UE 后完整编译。
7. 单人 PIE；涉及网络时做 2 人；关键里程碑做 4 人或打包实机测试。
8. 通过验收后提交；不通过就留在当前版本修复。
9. 更新 CHANGELOG、测试记录和存档版本号。
```

## 可复制给 Codex 的主提示词

```text
我正在开发 UE5.8 C++ 项目，产品代号为 ProjectRift。当前仓库已经完成到 v0.4.7。

已完成且应尽量保持不变的范围：
- 大厅准备与房主开始任务。
- 大厅到裂隙以及裂隙返回大厅的联机旅行。
- 守点目标、刷怪器、基础敌人、战斗掉落和拾取。
- 撤离点、结算、资源带出和单人/2 人完整闭环。

当前任务：实现 <版本号> <版本名称>。

执行前：
1. 先检查仓库中真实的项目模块名、类名前缀、GameMode/GameState、GAS、背包、任务、撤离、结算和旅行实现。
2. 以仓库实际代码为准；如果与旧文档命名不同，不要为了匹配文档而改名。
3. 先给出实现计划和拟修改文件，不要立即改代码。
4. 不得重命名项目模块，不得批量重命名旧类，不得删除 v0.4.7 已完成接口。

实现约束：
- 只修改本版本需要的文件，不重构无关系统。
- 需要修改旧系统时优先新增兼容接口、适配器或迁移函数。
- 联机状态必须服务端权威；客户端只发请求和播放表现。
- 数据驱动优先使用 DataAsset、GameplayTag、DeveloperSettings 或明确的数据结构。
- 不假设任何蓝图、Widget、地图或资源已经存在；需要的编辑器资产只列出手工创建步骤。
- 不使用 Experimental 功能作为核心依赖。
- 所有新日志使用项目自己的 LogCategory；Shipping 中关闭开发作弊入口。

完成后请输出：
1. 修改文件清单。
2. 核心实现说明。
3. UE 编辑器内需要手工完成的步骤。
4. 单人、2 人和打包测试步骤。
5. 可能影响存档、复制、旅行或旧蓝图的风险。
6. 建议 Git commit 信息。
```

## Git 规则

- 当前完成点标签：`baseline-v0.4.7`
- 功能分支：`feature/v0-x-y-short-name`
- Bug 分支：`fix/issue-short-name`
- Alpha/Beta/RC 分支：`release/0.12-alpha`、`release/0.13-beta`、`release/1.0-rc`
- 里程碑标签：`milestone-v0.4.8-vertical-slice`、`milestone-v0.13.0-content-complete`、`v1.0.0`
- C++、配置、蓝图和地图尽量分开提交；不要把 DerivedDataCache、Binaries、Intermediate、Saved 提交进仓库。
- 不要重写或 squash 已经代表 v0.1.0-v0.4.7 的稳定历史，除非仓库尚未发布且你明确理解影响。

\newpage

# 阶段 A：冻结并稳定现有 Vertical Slice（v0.4.8）

阶段出口：不新增玩法系统，只把已经完成的 v0.4.7 闭环变成可重复、可打包、可交给外部测试者运行的第一个稳定构建。

## v0.4.8 Vertical Slice 稳定化、回归保护与打包基线

**本次目标：** 保持 v0.4.7 的玩法和接口不变，集中修复崩溃、重复结算、复制、切图、生命周期和打包问题，形成第一个外部可试玩构建。

**Codex 编码范围：**

- 先只读审计 v0.4.7 的任务、刷怪、敌人、掉落、撤离、结算和返回大厅调用链，输出调用顺序和权威边界。
- 给结算和资源带出增加幂等保护，确保同一局不会因为重复事件、重叠触发或旅行回调发放两次奖励。
- 检查 Timer、动态委托、SpawnManager、Objective、ExtractionZone 和 UI 回调在地图退出时是否正确清理。
- 为关键流程建立最小 Smoke Test 或自动化测试入口；无法自动化的部分建立可重复的命令式测试步骤。
- 增加项目 LogCategory、MissionId/RunId、PlayerId 和旅行阶段日志，便于定位联机问题。
- 检查 Development 打包配置、默认地图、GameInstance、Cook 地图清单以及返回大厅资源交接。
- 更新 CHANGELOG、已知问题、v0.4.7 基线说明和 v0.4.8 测试记录。

**Codex 任务边界：**

- 禁止重新设计任务框架、刷怪器、背包、结算或旅行架构。
- 禁止把当前 ServerTravel 全面改成 Seamless Travel，除非现有旅行无法通过打包验收并且先有最小复现。
- 只修有复现步骤、日志证据或明确生命周期风险的问题。
- 不新增职业、敌人、装备、制造、章节或正式 UI。

**UE 编辑器手工项：**

- 保存所有地图和蓝图，执行 Fix Up Redirectors，并检查引用无丢失。
- 建立一套固定测试数据：一个任务、一个敌人配置、一个掉落表和一个撤离点。
- 打包 Windows Development 构建，并在非编辑器环境测试。
- 至少使用两个独立游戏窗口或两台电脑完成一次 Listen Server 测试。
- 通过后创建标签 `milestone-v0.4.8-vertical-slice`。

**验收标准：**

- [ ] 单人完整闭环连续运行 5 次，无崩溃、无重复奖励、无返回大厅状态残留。
- [ ] 2 人完整闭环连续运行 3 次，双方看到一致的任务、掉落、撤离和结算结果。
- [ ] 客户端无法直接完成任务、生成掉落、发放奖励或修改持久资源。
- [ ] 打包版本可以启动、进入大厅、进入裂隙、完成任务、撤离、结算并返回大厅。
- [ ] 退出副本后没有持续刷怪、残留 Timer、重复委托或旧 UI 继续响应。
- [ ] 无 Blocker/Critical 已知 Bug；其余问题有编号、复现步骤和临时规避方式。

**建议分支与提交：**

```text
branch: feature/v0-4-8-vertical-slice-stabilization
commit: v0.4.8 Stabilize the existing v0.4.7 gameplay loop
```

**禁止事项 / 风险提示：**

- v0.4.8 是质量门，不是功能版本。任何“顺便新增”的需求都移到 v0.5.0 以后。
- 若审计发现旧实现与文档不一致，以实际可运行代码为准，并把差异记录到技术文档，不要强行改成文档示例。

\newpage

# 阶段 B：生产基础、存档与舰船成长（v0.5.0-v0.5.6）

阶段出口：所有核心内容可以通过数据配置扩展；存档有版本号、备份和迁移；舰船修复真正推进主线；开发者能够快速生成测试场景并运行基础自动化。

## v0.5.0 Primary DataAsset 与 AssetManager 基础

**本次目标：** 为物品、任务、职能、敌人、掉落和舰船模块建立稳定的数据注册方式，避免继续依赖散落的硬编码类引用。

**Codex 编码范围：**

- 配置 AssetManager 或项目现有 Registry，支持按稳定 ID 查询 Item、Mission、Role、Enemy、ShipModule 数据。
- 给数据类定义校验函数：ID 唯一、软引用有效、数值范围合法。
- 为旧 DataAsset 提供兼容加载路径，不强制一次迁移所有资产。
- 使用 SoftObjectPtr/PrimaryAssetId 降低大厅加载全部战斗资源的风险。

**Codex 任务边界：** 先扫描当前 DataAsset 与 DataTable 使用方式；创建适配器，不批量替换全部引用。

**UE 编辑器手工项：**

- 在 Project Settings 注册 Primary Asset 类型和扫描目录。
- 把 3-5 个已有测试资产迁移为示例，保留旧资产直到验证完成。

**验收标准：**

- [ ] 运行时能通过 ID 加载示例物品和任务。
- [ ] 重复 ID 会在编辑器校验或自动测试中报错。
- [ ] 打包后软引用资产仍能被 Cook。

**建议分支与提交：**

```text
branch: feature/v0-5-0
commit: v0.5.0 Add primary asset registry and data validation
```

**禁止事项 / 风险提示：**

- 不要把蓝图资源硬路径写进 C++ 字符串。

## v0.5.1 GameplayTag 合同与项目调参设置

**本次目标：** 集中管理技能、状态、任务、物品、伤害和交互标签，并把常用数值从代码移入可审查配置。

**Codex 编码范围：**

- 整理 Tag 命名树，例如 Ability、State、Damage、Item、Mission、Role、UI、Event。
- 新增项目 DeveloperSettings / BalanceSettings，存放交互距离、撤离倒计时、复活时间和基础缩放参数。
- 为跨系统事件定义明确结构体或 GameplayMessage 风格接口，避免字符串广播。
- 添加启动时 Tag/配置完整性检查。

**Codex 任务边界：** 标签重命名必须保留 Redirect 或迁移清单；优先新增，不直接删除。

**UE 编辑器手工项：**

- 在 GameplayTags 设置中补齐标签和注释。
- 确认旧技能引用的标签没有被删除。

**验收标准：**

- [ ] 旧技能与背包行为不回归。
- [ ] 关键数值可在编辑器配置而无需重新编译。
- [ ] 缺失必需 Tag 时启动日志给出明确错误。

**建议分支与提交：**

```text
branch: feature/v0-5-1
commit: v0.5.1 Centralize gameplay tags and project tuning settings
```

**禁止事项 / 风险提示：**

- 不要把所有数值都放进一个巨大 Settings 类；按领域拆分。

## v0.5.2 版本化玩家档案与安全保存

**本次目标：** 建立可跨版本升级、能检测损坏并可恢复备份的本地玩家档案。

**Codex 编码范围：**

- 新增 ProfileSave、SaveSubsystem、SaveVersion、ProfileId 和时间戳。
- 保存资源钱包、长期背包、装备、职能解锁、舰船模块、设置和剧情进度。
- 使用临时文件/备份槽实现近似原子写入；读取失败时尝试上一份备份。
- 建立 `MigrateFromVersion` 链，禁止未来直接更改结构后让旧档崩溃。

**Codex 任务边界：** 保存结构先独立于 UI；Codex 必须写出字段迁移和失败路径。

**UE 编辑器手工项：**

- 创建“新建档案、继续游戏、删除档案”临时入口。
- 准备一个旧版本测试存档样本。

**验收标准：**

- [ ] 保存后重启游戏数据一致。
- [ ] 模拟主文件损坏时能提示并读取备份。
- [ ] 旧版本样本能迁移到当前版本。
- [ ] 存档不保存 Actor/UObject 裸指针。

**建议分支与提交：**

```text
branch: feature/v0-5-2
commit: v0.5.2 Add versioned profile save backups and migration
```

**禁止事项 / 风险提示：**

- 每次改存档结构都必须提升 SaveVersion 并补测试。

## v0.5.3 多人个人进度与团队主线政策

**本次目标：** 明确 Listen Server 下每名玩家的个人战利品、个人档案和房主世界推进，避免正式版只保存房主。

**Codex 编码范围：**

- 区分 PersonalProfile、MissionSettlement 和 HostCampaignState。
- 每个客户端本地加载自己的档案；服务端只接收本局所需的已验证 Loadout 摘要。
- 任务结算由 Host 生成每人 Settlement，客户端收到后幂等写入自己的档案。
- 定义客机加入朋友世界时：个人装备归自己，舰船主线使用房主进度；任务奖励归个人。

**Codex 任务边界：** 不要宣称 P2P 本地档案能完全防作弊；目标是规则一致、无复制漏洞和无竞技经济。

**UE 编辑器手工项：**

- 在大厅显示“当前使用房主世界进度”的提示。
- 用两套不同测试档案验证加入和退出。

**验收标准：**

- [ ] 客户端撤离后自己的资源会保存到自己的档案。
- [ ] 房主修船不会永久改写客机的舰船进度。
- [ ] 同一结算重发不会复制奖励。
- [ ] 客机断线后重进，未结算奖励按明确政策处理。

**建议分支与提交：**

```text
branch: feature/v0-5-3
commit: v0.5.3 Define per player profile and host campaign settlement
```

**禁止事项 / 风险提示：**

- 不引入数据库或账号服务器；1.0 保持无后端服务依赖。

## v0.5.4 舰船修复与章节解锁系统

**本次目标：** 让撤离资源转化为可见主线推进，并为五章内容提供明确解锁门槛。

**Codex 编码范围：**

- 建立 ShipModuleDefinition、ShipModuleState、ResourceWallet 和修复事务。
- 实现动力炉、医疗舱、通讯阵列、武器库、跃迁核心的多阶段修复。
- 每次修复消耗服务端/档案中的资源，并触发可查询的 UnlockTag。
- 提供章节、制造、医疗、自救、任务情报等功能门槛接口。

**Codex 任务边界：** 修复操作必须使用事务式接口，不允许 Widget 直接减资源。

**UE 编辑器手工项：**

- 创建修船面板和五个模块占位模型/交互点。
- 配置每个阶段的成本与临时描述。

**验收标准：**

- [ ] 资源不足无法修复且不扣资源。
- [ ] 成功修复只扣一次并立即保存。
- [ ] 重启后模块和解锁状态存在。
- [ ] 加入房主大厅时显示房主世界的模块状态。

**建议分支与提交：**

```text
branch: feature/v0-5-4
commit: v0.5.4 Add ship repair progression and chapter unlocks
```

**禁止事项 / 风险提示：**

- 不要把最终成本定死；全部通过数据配置，后续平衡会调整。

## v0.5.5 开发者工具、日志与诊断 HUD

**本次目标：** 降低后续几十个版本的调试成本，让 Codex 修 Bug 时有稳定复现入口。

**Codex 编码范围：**

- 建立按系统划分的 LogCategory：Net、GAS、Inventory、Mission、Save、AI、Steam。
- 新增 Development-only CheatManager/Console 命令：生成物品、加资源、解锁模块、启动/完成任务、刷敌人、强制撤离。
- 添加 Debug HUD 显示 NetRole、ASC 状态、当前任务、RunId、SettlementId、背包版本和敌人预算。
- Shipping 构建通过宏和配置关闭作弊命令。

**Codex 任务边界：** 工具代码放独立模块/目录，不污染正常玩法逻辑。

**UE 编辑器手工项：**

- 创建简洁 Debug Widget 或使用 Canvas Debug Draw。
- 记录一份常用调试命令表。

**验收标准：**

- [ ] 调试命令只在 Development/Editor 有效。
- [ ] 可在 2 人 PIE 中分别识别 Host/Client 状态。
- [ ] 强制完成任务不会重复结算。
- [ ] Shipping 构建不能调用开发作弊入口。

**建议分支与提交：**

```text
branch: feature/v0-5-5
commit: v0.5.5 Add development cheats structured logs and debug HUD
```

**禁止事项 / 风险提示：**

- 日志不得打印 Steam 身份令牌、文件系统隐私数据或完整个人标识。

## v0.5.6 自动化 Smoke Test 与本地构建脚本

**本次目标：** 把最容易回归的数据、存档、背包、结算和打包问题变成可重复检查。

**Codex 编码范围：**

- 添加 Automation Test：ItemId 唯一、背包堆叠、结算幂等、存档往返、存档迁移、ShipModule 成本。
- 添加最小 Functional Test 地图或测试 Actor，验证目标完成和撤离状态。
- 建立脚本：生成工程文件、编译 Editor、运行测试、打 Development 包；路径参数化。
- 输出机器可读测试日志，Codex 修复后可复跑。

**Codex 任务边界：** Codex 先写最小稳定测试，不追求覆盖率数字；每次发现严重 Bug 后补对应回归测试。

**UE 编辑器手工项：**

- 在 Session Frontend / Automation 中运行测试。
- 在干净环境执行一次构建脚本。
- 创建标签 `milestone-v0.5.6-production-foundation`。

**验收标准：**

- [ ] 全部单元/自动化测试通过。
- [ ] 删除 Binaries/Intermediate 后脚本仍能重建。
- [ ] 打包后能加载测试所需的数据资产。
- [ ] 存档迁移测试覆盖至少一个旧版本。

**建议分支与提交：**

```text
branch: feature/v0-5-6
commit: v0.5.6 Add automation smoke tests and repeatable build script
```

**禁止事项 / 风险提示：**

- 测试不得依赖真实 Steam 网络；平台测试在后续版本完成。

\newpage

# 阶段 C：完整战斗、职能与单人兼容（v0.6.0-v0.6.6）

阶段出口：普通武器和四套职能均可用于完整任务；伤害、状态、倒地、救援和单人无人机在 1-4 人下稳定；战斗表现与权威逻辑分离。

## v0.6.0 统一伤害 Execution 与状态效果

**本次目标：** 把测试期简单扣血升级为可扩展的伤害上下文，同时保持现有技能接口兼容。

**Codex 编码范围：**

- 新增 DamageExecution / DamageContext，支持基础伤害、伤害类型、护盾、抗性、来源、命中部位和友军规则。
- 定义 Kinetic、Energy、Corruption 等少量 DamageTag；不做过度复杂元素克制。
- 统一 ShieldBreak、Stagger、Invulnerable、Polluted 等 GameplayTag 与 GameplayEffect。
- 为旧 SetByCaller Damage 提供兼容入口，逐个迁移而非一次删除。

**Codex 任务边界：** 先扫描现有 GE_Damage 调用点，逐步迁移并保留自动测试。

**UE 编辑器手工项：**

- 创建测试 GE 和 Cue，验证护盾破碎、污染、治疗与净化。
- 建立伤害测试靶场。

**验收标准：**

- [ ] 旧普通攻击仍能正确扣盾再扣血。
- [ ] 伤害来源和击杀归属正确。
- [ ] 友军伤害默认关闭。
- [ ] 状态效果到期和净化不会残留 Tag。

**建议分支与提交：**

```text
branch: feature/v0-6-0
commit: v0.6.0 Add unified damage execution and status effects
```

**禁止事项 / 风险提示：**

- 不要在 Execution 中播放特效；表现交给 GameplayCue。

## v0.6.1 武器、弹药、瞄准与装填框架

**本次目标：** 建立第三人称射击的可配置武器基础，让角色不再只依赖测试近战/射线。

**Codex 编码范围：**

- 新增 WeaponDefinition、WeaponInstance/Actor、Hitscan 与 Projectile 两种执行策略。
- 实现瞄准、散布、射速、弹匣、备用弹药、装填、切换和服务端射击校验。
- 客户端可做枪口、后坐力和准星预表现；伤害、弹药扣除和命中最终由服务端决定。
- 为高延迟记录时间戳和基本容差，但 1.0 不实现竞技级完整回溯。

**Codex 任务边界：** 先做一个稳定步枪打通全链路，再扩展其他策略。

**UE 编辑器手工项：**

- 创建步枪、霰弹枪、能量枪、发射器四类占位武器 DataAsset。
- 配置骨骼 Socket、动画蒙太奇、准星和临时声音。

**验收标准：**

- [ ] Host 与 Client 都能射击、装填和切换武器。
- [ ] 客户端无法超射速或在无弹药时造成伤害。
- [ ] 投射物只由服务端生成权威实例。
- [ ] 高延迟模拟下不会明显双重扣弹。

**建议分支与提交：**

```text
branch: feature/v0-6-1
commit: v0.6.1 Add replicated weapon ammo aim and reload framework
```

**禁止事项 / 风险提示：**

- 不要在此版本加入十几种枪；DataAsset 和接口稳定才是目标。

## v0.6.2 命中反馈、受击、硬直与 GameplayCue

**本次目标：** 在不改变权威伤害结果的前提下补齐战斗手感和可读性。

**Codex 编码范围：**

- 建立 Cue：枪口、命中、护盾命中、护盾破碎、治疗、净化、状态区域和死亡。
- 实现轻量 HitReact/Stagger，使用 Tag 防止无限硬直。
- 向本地玩家提供准星反馈、Hit Marker、伤害方向和受击提示。
- 所有表现都能在晚加入/网络延迟下容错，不作为伤害真值。

**Codex 任务边界：** Cue 订阅现有 GAS 事件；不要用 Multicast 再重复广播同一效果。

**UE 编辑器手工项：**

- 绑定临时 Niagara、声音、镜头震动和动画。
- 提供关闭/降低镜头震动的设置入口占位。

**验收标准：**

- [ ] 两端都能看到正确命中类型。
- [ ] Cue 不会每次属性复制都重复播放。
- [ ] 玩家被连续攻击时仍有可操作窗口。
- [ ] 关闭镜头震动后核心信息仍清楚。

**建议分支与提交：**

```text
branch: feature/v0-6-2
commit: v0.6.2 Add combat feedback hit reactions and gameplay cues
```

**禁止事项 / 风险提示：**

- 不要将 VFX 是否播放作为命中判定条件。

## v0.6.3 数据驱动职能系统与突击模块定稿

**本次目标：** 把现有突击技能迁移到可切换 AbilitySet/RoleDefinition，并完成正式版技能闭环。

**Codex 编码范围：**

- 建立 RoleDefinition、AbilitySet、授予/移除句柄和被动效果。
- 突击模块：推进冲锋、过载爆发、战术护盾、被动击杀回盾或压制抗性。
- 职业切换仅在大厅/安全区允许；服务端验证解锁和冷却状态。
- 切换后清理旧 Ability、Effect、输入状态和召唤物。

**Codex 任务边界：** 沿用现有输入 Tag，不重写 Enhanced Input。

**UE 编辑器手工项：**

- 创建突击职能卡片、图标、技能描述和测试数值。
- 在大厅添加选择交互。

**验收标准：**

- [ ] 突击技能在单人和 2 人均正确。
- [ ] 切换职能后旧技能和旧被动不残留。
- [ ] 死亡重生后技能仍正确初始化。
- [ ] 旅行两局后不会重复授予。

**建议分支与提交：**

```text
branch: feature/v0-6-3
commit: v0.6.3 Finalize data driven roles and assault module
```

**禁止事项 / 风险提示：**

- 不要让 Character 硬编码具体职能类。

## v0.6.4 工程模块定稿

**本次目标：** 实现偏部署、防守和任务效率的工程玩法，同时限制多人召唤物成本。

**Codex 编码范围：**

- 自动炮台：服务端生成、选敌、攻击、寿命、所有者和数量上限。
- 护盾发生器：范围内应用可识别的护盾/减伤效果，并有耐久或时限。
- 快速修复：通过能力/接口影响目标交互速率，不直接改任务计时器内部变量。
- 工程被动：工具效率或部署物冷却加成。

**Codex 任务边界：** 召唤物基类和生命周期必须可复用，避免炮台与护盾各写一套网络代码。

**UE 编辑器手工项：**

- 创建炮台、护盾发生器蓝图和部署预览材质。
- 配置碰撞、导航阻挡和简单特效。

**验收标准：**

- [ ] 客户端不能在非法位置或超上限部署。
- [ ] 召唤物主人断线/切图后能清理。
- [ ] 炮台伤害归属工程玩家。
- [ ] 快速修复对所有任务仍保持“任何职业都能完成”。

**建议分支与提交：**

```text
branch: feature/v0-6-4
commit: v0.6.4 Finalize engineer deployables and interaction bonuses
```

**禁止事项 / 风险提示：**

- 不要允许部署物永久存在或无限堆叠。

## v0.6.5 医疗/净化与侦察模块定稿

**本次目标：** 补齐支持和侦察职能，保证它们在单人时也有伤害与生存价值。

**Codex 编码范围：**

- 医疗/净化：治疗脉冲、净化波、复苏信标、被动治疗效率。
- 侦察：资源扫描、弱点标记、短距离机动/抓钩或相位冲刺、被动移动/探测。
- 治疗、净化和标记使用 GAS Effect/Tag；目标筛选由服务端验证。
- 弱点标记只提供有限团队增伤，不与所有乘区无限相乘。

**Codex 任务边界：** 移除 Effect 时按 Tag/Handle 精确处理，禁止 ClearAllEffects。

**UE 编辑器手工项：**

- 创建两个职能的 UI、图标、Cue 和测试动画。
- 配置资源扫描的轮廓/图标显示。

**验收标准：**

- [ ] 医疗玩家可治疗自己和队友，不能治疗已彻底死亡目标。
- [ ] 净化能移除指定污染而不误删永久被动。
- [ ] 侦察扫描只显示允许公开的资源。
- [ ] 4 个职能任意一个都能单人完成基础任务。

**建议分支与提交：**

```text
branch: feature/v0-6-5
commit: v0.6.5 Finalize medic purifier and scout role modules
```

**禁止事项 / 风险提示：**

- 抓钩若网络稳定性不足，可使用短距离相位冲刺替代，功能目标优先。

## v0.6.6 倒地救援、单人无人机与人数缩放

**本次目标：** 完成单人和合作模式的生存闭环，并统一按人数计算任务、敌人和 Boss 压力。

**Codex 编码范围：**

- 倒地状态、流血计时、爬行/禁用能力、队友交互救援和彻底死亡/观战。
- 单人生成 SupportDrone：跟随、资源扫描、一次自救、低频护盾；多人弱化或关闭。
- 建立 DifficultyScalingService，统一输出玩家人数、难度和章节乘数。
- 目标时间、刷怪预算、精英上限、Boss 生命和奖励都读取同一 ScalingSnapshot。

**Codex 任务边界：** 缩放由一个服务端服务产生快照，其他系统只消费结果。

**UE 编辑器手工项：**

- 创建救援进度 UI、倒地特效、无人机蓝图与导航/跟随配置。
- 测试 1、2、4 人预设。

**验收标准：**

- [ ] 单人倒地可由无人机自救一次，之后按规则失败。
- [ ] 多人可互救，医疗技能有优势但非必需。
- [ ] 断线、重生、旅行后无人机和倒地状态不残留。
- [ ] 1/2/4 人的任务与敌人压力显著不同且可通关。

**建议分支与提交：**

```text
branch: feature/v0-6-6
commit: v0.6.6 Add downed revive solo drone and unified scaling
```

**禁止事项 / 风险提示：**

- 不要只按人数线性增加 Boss 血量；同时控制敌人数、目标时间和精英压力。

\newpage

# 阶段 D：背包、装备、掉落与制造（v0.7.0-v0.7.6）

阶段出口：玩家可以在局内拾取、使用、丢弃、装备和比较物品；撤离后能制造、拆解和升级；所有交易服务端权威且对重连、重复 RPC 和存档迁移有保护。

## v0.7.0 物品实例 GUID 与权威事务层

**本次目标：** 把测试期按 ItemId 操作升级为可追踪的 ItemInstance，并建立防重复、可审计的物品事务。

**Codex 编码范围：**

- 每个非纯堆叠装备拥有 InstanceGuid、DefinitionId、数量、等级、耐久、随机种子和绑定状态。
- 建立 InventoryTransaction：Add、Remove、Move、Split、Merge、Use、Drop、Equip，全部带 TransactionId。
- FastArray Entry 只通过组件事务修改，并正确 MarkItemDirty/MarkArrayDirty。
- 增加服务端日志和重复事务拒绝。

**Codex 任务边界：** 先写迁移函数，把旧存档 ItemId/Count 转成新实例。

**UE 编辑器手工项：**

- 创建测试装备和堆叠材料。
- 使用调试 HUD 检查 GUID 和 FastArray ReplicationKey。

**验收标准：**

- [ ] 两个相同 Definition 的装备拥有不同 GUID。
- [ ] 堆叠、拆分、合并数量守恒。
- [ ] 重复 TransactionId 不重复生成物品。
- [ ] 重连后实例数据一致。

**建议分支与提交：**

```text
branch: feature/v0-7-0
commit: v0.7.0 Add item instance GUIDs and authoritative transactions
```

**禁止事项 / 风险提示：**

- 禁止 Widget 直接修改 FastArray。

## v0.7.1 装备槽位、外观与 GAS 属性句柄

**本次目标：** 实现 Weapon、Armor、Chip、Tool 四类装备，并同步必要外观而不公开完整背包。

**Codex 编码范围：**

- 扩展 EquipmentComponent 和槽位规则，所有 Equip/Unequip 经服务端事务。
- 装备时应用定义中的 GameplayEffect/Ability，记录 ActiveEffectHandle 和 GrantedAbilityHandle。
- 卸下时精确移除；重复装备同一实例不得叠加。
- 公开复制当前武器/外观摘要，完整词条只 OwnerOnly。

**Codex 任务边界：** 沿用 v0.7.0 事务层，不另写 UI 专用装备路径。

**UE 编辑器手工项：**

- 创建四个装备槽 UI 占位。
- 为武器/护甲配置角色 Socket 或外观组件。

**验收标准：**

- [ ] 装备和卸下后属性、技能、外观同步正确。
- [ ] 死亡重生与切图后不会重复叠加 GE。
- [ ] 其他玩家能看到武器外观，但看不到完整背包。
- [ ] 非法类型不能放入错误槽位。

**建议分支与提交：**

```text
branch: feature/v0-7-1
commit: v0.7.1 Add replicated equipment slots visuals and GAS handles
```

**禁止事项 / 风险提示：**

- MaxHealth 改变时明确当前 Health 的比例/补偿规则，并写测试。

## v0.7.2 稀有度、词条与确定性生成

**本次目标：** 为装备提供有限但可读的 Build 差异，同时保证服务端生成和存档可重现。

**Codex 编码范围：**

- 定义 Common、Uncommon、Rare、Prototype 等 4 档稀有度。
- 建立 AffixDefinition、可用槽位、数值范围、互斥 Tag 和权重。
- 使用服务端 LootSeed 确定性生成并把最终 Roll 保存到实例。
- 词条通过聚合 GameplayEffect 或明确 Modifier 应用，避免运行时生成不可追踪类。

**Codex 任务边界：** 控制乘区数量，先做加法/百分比两类清晰修正。

**UE 编辑器手工项：**

- 配置第一批约 12 个词条，并写清楚本地化名称和说明。
- 建立测试面板比较基础值与词条后值。

**验收标准：**

- [ ] 相同 Seed 和定义生成相同结果。
- [ ] 非法槽位/互斥词条不会出现。
- [ ] 保存读取后词条数值不改变。
- [ ] 装备/卸下词条效果无残留。

**建议分支与提交：**

```text
branch: feature/v0-7-2
commit: v0.7.2 Add deterministic rarity and affix generation
```

**禁止事项 / 风险提示：**

- 不要加入几十个不可测试词条；先保证 12 个高质量词条。

## v0.7.3 个人战利品、世界掉落与奖励预算

**本次目标：** 建立适合合作 PVE 的分配规则，减少抢装备和复制争议。

**Codex 编码范围：**

- 定义：普通消耗品/任务搬运物可世界共享；任务结算和稀有装备使用个人奖励。
- 建立 LootTableDefinition、RewardBudget、章节/难度/敌人/目标来源。
- 服务端使用 RunSeed 生成，记录来源和接收者；个人掉落只对拥有者可见或直接进入结算。
- 添加低保/重复保护的简单计数器，但不做复杂商业化抽卡。

**Codex 任务边界：** 随机结果全部由服务端确定，并写入 MissionResult 以便追踪。

**UE 编辑器手工项：**

- 配置各章节占位掉落池和任务奖励预览。
- 为个人掉落设置明显 Owner 提示。

**验收标准：**

- [ ] 两名玩家结算可以得到不同个人装备。
- [ ] 共享任务物不会因两个玩家同时拾取而复制。
- [ ] 奖励总量符合预算，难度提升有可见收益。
- [ ] 失败与中途退出按既定政策结算。

**建议分支与提交：**

```text
branch: feature/v0-7-3
commit: v0.7.3 Add personal loot shared pickups and reward budgets
```

**禁止事项 / 风险提示：**

- 不实现玩家交易，避免本地存档与 P2P 环境下的经济安全问题。

## v0.7.4 消耗品、工具、弹药与快捷栏

**本次目标：** 让局内背包形成有意义的资源管理，并支持手柄操作。

**Codex 编码范围：**

- 实现 3-4 格快捷栏、服务端使用请求、使用时间、冷却和打断。
- 支持治疗针、护盾包、能量电池、净化剂、弹药包和任务工具。
- 任务关键物品设置不可丢弃/不可制造/任务结束自动处理。
- 快捷栏只保存 InstanceGuid/槽位引用，不复制一份物品。

**Codex 任务边界：** 所有使用效果通过 GAS/标准接口应用，不在 Inventory 直接改属性。

**UE 编辑器手工项：**

- 创建快捷栏 HUD、数量、冷却和手柄选择提示。
- 配置消耗品使用动画和 Cue。

**验收标准：**

- [ ] 满血/无 Debuff 等无效条件不会消耗物品。
- [ ] 使用中受击/倒地时按规则打断。
- [ ] 网络延迟下物品只扣一次。
- [ ] 任务物品不能被非法丢弃。

**建议分支与提交：**

```text
branch: feature/v0-7-4
commit: v0.7.4 Add consumables tools ammo and replicated quick slots
```

**禁止事项 / 风险提示：**

- 不要为每个消耗品新写一个独立组件；优先数据驱动。

## v0.7.5 制造、拆解与装备升级

**本次目标：** 在舰船武器库形成稳定的局外资源消耗和装备成长。

**Codex 编码范围：**

- 建立 RecipeDefinition、CraftingStation、DismantleResult 和 UpgradeCost。
- 制造/拆解/升级使用事务：验证资源、扣除、生成/修改实例、保存，失败时不产生半状态。
- 舰船模块解锁配方层级；章节推进提供新材料与配方。
- 升级只提升有限等级并控制数值，避免无限成长破坏难度。

**Codex 任务边界：** 先建立一条完整配方，再批量填数据；不要让 Codex自动生成大量未经审查的数值。

**UE 编辑器手工项：**

- 创建制造台交互和临时制作界面。
- 配置 10-15 个初始配方与拆解返还。

**验收标准：**

- [ ] 资源不足不能制造且不扣除。
- [ ] 重复点击/延迟不会制造两份。
- [ ] 拆解已装备物品被禁止或先明确卸下。
- [ ] 重启后制造与升级结果保留。

**建议分支与提交：**

```text
branch: feature/v0-7-5
commit: v0.7.5 Add crafting dismantling and equipment upgrades
```

**禁止事项 / 风险提示：**

- 制造不是主游戏本身，保持简单透明。

## v0.7.6 完整背包与装备 UI、重连和防复制回归

**本次目标：** 将系统包装为玩家可用的鼠标和手柄界面，并集中验证网络边界情况。

**Codex 编码范围：**

- 提供 UI ViewModel/Controller 接口：背包分页、排序、筛选、比较、装备、丢弃、快捷栏和舰船仓库。
- 事件驱动刷新，不在 Tick 全量读取 FastArray。
- 处理重连、旅行、存档加载后 UI 重新绑定。
- 添加抢拾、重复 RPC、断线时事务、满背包、装备交换等回归测试。

**Codex 任务边界：** Codex 负责数据接口和焦点辅助，Widget 布局由编辑器手工完成。

**UE 编辑器手工项：**

- 完成 WBP Inventory/Equipment/Tooltip/Confirmation 的商业级基础布局。
- 测试键鼠和手柄导航焦点。

**验收标准：**

- [ ] 所有操作都能用键鼠和手柄完成。
- [ ] 排序/筛选只影响显示，不改变权威顺序和数据。
- [ ] 2 人抢拾、丢弃、重连不产生复制。
- [ ] 打包版本 UI 能正确加载所有图标。

**建议分支与提交：**

```text
branch: feature/v0-7-6
commit: v0.7.6 Complete inventory equipment UI and anti duplication tests
```

**禁止事项 / 风险提示：**

- 不要让 UI 保存自己的“影子背包”作为真值。

\newpage

# 阶段 E：任务、遭遇导演、敌人生态与 Boss 框架（v0.8.0-v0.8.6）

阶段出口：可以通过数据组合出不同任务合同；五类目标、可选目标、风险增长、遭遇导演和敌人配置可复用；Boss 框架能够支撑后续五章 Boss，而不是每个 Boss 从零写一套。

## v0.8.0 任务合同与裂隙选择框架

**本次目标：** 让大厅任务台根据章节、难度、修正和奖励创建可复现的 MissionDefinition。

**Codex 编码范围：**

- 建立 MissionDefinition、MissionContract、BiomeId、DifficultyId、ModifierIds、RewardPreview 和 Seed。
- 大厅由房主选择合同，选择结果复制给队伍；开始前服务端验证章节解锁。
- TravelContext 携带合同 ID/Seed，不携带大量 UObject 引用。
- 任务结果记录合同版本，便于后续平衡和问题复现。

**Codex 任务边界：** 复用 v0.4 TravelContext 与现有大厅流程，不重新创建第二套 Session。

**UE 编辑器手工项：**

- 创建任务台 UI 原型和 3 个测试合同。
- 配置章节锁定/解锁提示。

**验收标准：**

- [ ] 所有玩家看到相同合同与奖励预览。
- [ ] 未解锁章节不能通过 RPC 强行开始。
- [ ] 相同合同 Seed 可以复现目标和遭遇配置。
- [ ] 返回大厅后可选择下一任务。

**建议分支与提交：**

```text
branch: feature/v0-8-0
commit: v0.8.0 Add data driven mission contracts and selection
```

**禁止事项 / 风险提示：**

- 奖励预览是范围/类型提示，不提前生成最终装备实例。

## v0.8.1 目标图与五类主目标

**本次目标：** 在已有守点目标上扩展采集、搬运、摧毁和猎杀，并支持串联/并行目标。

**Codex 编码范围：**

- 建立轻量 ObjectiveGraph/ObjectiveSequence，支持前置条件、完成条件和可选节点。
- 实现 Collect、Carry、Destroy、Hunt，统一继承现有 ObjectiveBase。
- GameState 同步当前玩家需要看的目标摘要，不复制整个内部图。
- 任务保存/断线重连时可重建当前目标状态。

**Codex 任务边界：** 避免做编辑器可视化节点工具；先用 DataAsset 数组/结构体表达。

**UE 编辑器手工项：**

- 创建 5 类目标蓝图和测试交互物。
- 在测试地图构建一个“采集 -> 搬运 -> 猎杀”的小流程。

**验收标准：**

- [ ] 所有目标均由服务端推进。
- [ ] 串联和并行完成条件正确。
- [ ] 任务物品掉落/断线后不会永久卡死。
- [ ] 2 人能共同推进同一目标。

**建议分支与提交：**

```text
branch: feature/v0-8-1
commit: v0.8.1 Add objective graph and five objective types
```

**禁止事项 / 风险提示：**

- 每种目标必须有失败恢复或重生机制，不能让任务永久软锁。

## v0.8.2 裂隙稳定度、可选目标与任务修正

**本次目标：** 加入清晰的风险收益决策，使玩家需要决定继续搜刮还是撤离。

**Codex 编码范围：**

- RiftStability 随时间、警报、目标阶段下降，驱动敌人预算、环境危险和奖励倍率。
- 实现可选目标与失败不阻断主线的奖励逻辑。
- 建立 ModifierDefinition：低重力、护盾干扰、污染增强、精英增援、资源富集等。
- 所有修正通过 Tag/接口影响系统，禁止在任务代码里大量 if MissionName。

**Codex 任务边界：** 把稳定度当成服务器规则输入，不要让 UI 自己倒计时。

**UE 编辑器手工项：**

- HUD 添加稳定度、风险等级和可选目标显示。
- 为每个修正添加清晰图标/描述。

**验收标准：**

- [ ] 稳定度在所有客户端一致。
- [ ] 高风险确实提升危险和奖励。
- [ ] 可选目标失败不阻塞撤离。
- [ ] 修正可以组合且不会留下永久 Effect。

**建议分支与提交：**

```text
branch: feature/v0-8-2
commit: v0.8.2 Add rift stability optional objectives and modifiers
```

**禁止事项 / 风险提示：**

- 修正数量先控制在 6 个以内，逐个测试组合。

## v0.8.3 遭遇导演与战斗预算

**本次目标：** 将简单刷怪器升级为按区域、队伍状态、难度和稳定度调度的遭遇导演。

**Codex 编码范围：**

- 建立 EncounterDirector，输入 ScalingSnapshot、RiftStability、玩家生命/距离、当前目标。
- 使用威胁点数而非纯数量，限制精英、远程、自爆和总 AI 上限。
- 实现冷却期、喘息期、增援事件和安全出生检查。
- 记录导演决策到调试日志，支持指定 Seed 重现。

**Codex 任务边界：** 迁移现有 SpawnDirector，保留 v0.4.2 接口或提供适配器。

**UE 编辑器手工项：**

- 在各测试区域标记 SpawnGroup、Nav 区和不可出生区。
- 用 Debug HUD 显示预算、当前敌人和下一事件。

**验收标准：**

- [ ] 导演不会在玩家视野/近距离无提示出生。
- [ ] 低血量全队会获得合理喘息，不会永久停刷。
- [ ] 4 人时压力增加但不超过 AI 上限。
- [ ] 相同 Seed 与输入能近似复现事件顺序。

**建议分支与提交：**

```text
branch: feature/v0-8-3
commit: v0.8.3 Upgrade spawn system to encounter director budgets
```

**禁止事项 / 风险提示：**

- 不要让导演每帧遍历所有世界 Actor；使用注册和定时采样。

## v0.8.4 普通与精英敌人生态

**本次目标：** 扩展为能形成组合压力的 8 种普通敌人和 4 种精英配置。

**Codex 编码范围：**

- 以 EnemyDefinition + 行为组件/Ability 配置差异，减少每种敌人复制代码。
- 普通建议：爬行者、喷吐者、爆裂虫、掘地者、蜂群体、寄生体、守卫无人机、远程哨兵。
- 精英建议：护巢者、裂隙猎手、召唤者、技能干扰者。
- 统一感知、仇恨、攻击请求、死亡、掉落和状态免疫接口。

**Codex 任务边界：** Codex 优先写共享能力/组件；具体模型、动画和 BT 配置由编辑器完成。

**UE 编辑器手工项：**

- 为敌人创建可辨识轮廓、声音、弱点和动画。
- 配置每个敌人的威胁预算与章节出现表。

**验收标准：**

- [ ] 每个敌人有独特、可读、可反制的战斗作用。
- [ ] 敌人组合不会导致不可避免的无限控制。
- [ ] AI 只在服务端决策，客户端表现稳定。
- [ ] 大量普通怪时帧率和网络仍在预算内。

**建议分支与提交：**

```text
branch: feature/v0-8-4
commit: v0.8.4 Add data driven regular and elite enemy ecosystem
```

**禁止事项 / 风险提示：**

- 如果美术产能不足，可用同一骨架和材质变体，但行为必须有区分。

## v0.8.5 通用 Boss 阶段与技能调度框架

**本次目标：** 为五个章节 Boss 建立复用的阶段、技能、前摇、弱点、召唤和奖励结构。

**Codex 编码范围：**

- 建立 BossDefinition、PhaseDefinition、AbilityPattern、Telegraph、ArenaEvent 和 RewardId。
- 技能调度由服务端状态机/GAS 执行，支持冷却、权重、阶段条件和防重复。
- 同步当前阶段、目标、可打断/硬直状态；不复制内部决策细节。
- Boss 重置、全灭、玩家断线和卡出场地有恢复路径。

**Codex 任务边界：** 不要让每个 Boss 直接在 Tick 中写巨大 switch；使用数据和小型能力单元。

**UE 编辑器手工项：**

- 创建 Boss 测试竞技场、血条、阶段提示和调试按钮。
- 建立通用前摇颜色/声音规范。

**验收标准：**

- [ ] 阶段切换只触发一次。
- [ ] 所有高伤技能有可识别前摇。
- [ ] 全灭重试后 Boss 状态完全重置。
- [ ] 2 人和 4 人技能目标选择正确。

**建议分支与提交：**

```text
branch: feature/v0-8-5
commit: v0.8.5 Add reusable boss phases patterns and telegraphs
```

**禁止事项 / 风险提示：**

- 表现可延迟，但伤害窗口必须由服务端统一。

## v0.8.6 裂隙守卫 Boss 与任务生产里程碑

**本次目标：** 使用通用框架完成第一个正式 Boss，验证任务、导演、掉落、结算和缩放全链路。

**Codex 编码范围：**

- 实现范围震荡、锁定冲锋、地面污染、召唤增援和低血量狂暴。
- Boss 奖励进入个人结算，并推进章节/舰船修复。
- 将 Boss 任务作为 MissionContract，可在大厅选择和重试。
- 补充 Boss、目标图和遭遇导演自动/功能测试。

**Codex 任务边界：** Codex 只实现代码和数据接口；动画、Arena 和 VFX 需要手工验收。

**UE 编辑器手工项：**

- 完成 Boss 竞技场灰盒、碰撞、导航、临时动画和音画前摇。
- 单人、2 人、4 人分别实机平衡。
- 创建标签 `milestone-v0.8.6-core-feature-complete`。

**验收标准：**

- [ ] Boss 至少有 4 个可读技能和 2 个阶段。
- [ ] 全灭、重试、断线、撤离均不软锁。
- [ ] Boss 死亡只奖励一次。
- [ ] 完整任务打包版连续 3 次通过。

**建议分支与提交：**

```text
branch: feature/v0-8-6
commit: v0.8.6 Complete Rift Guardian and core gameplay production milestone
```

**禁止事项 / 风险提示：**

- 此里程碑后核心系统进入兼容优先模式，新内容应复用已有框架。

\newpage

# 阶段 F：五章内容、舰船大厅与完整主线（v0.9.0-v0.9.6）

阶段出口：从新档案到最终返航结局全流程可玩。该阶段的每个章节版本是“内容里程碑”，允许在其内部使用多个小分支和提交；Codex 主要负责复用系统、章节规则、任务数据与验证工具，地图、美术、动画、灯光和叙事摆放必须在 UE 编辑器中完成。

## v0.9.0 美术/关卡生产规范与资源许可清单

**本次目标：** 在大量导入资产前冻结视觉、目录、命名、性能和版权规则，避免后期返工或 Steam 法务风险。

**Codex 编码范围：**

- 建立 AssetNaming、目录规则、碰撞通道、LOD/Nanite、材质实例、音频类别和性能预算文档。
- 新增资产许可登记表字段：来源、作者、商用权限、修改权限、归属要求、发票/许可证位置。
- 添加 Editor Utility/验证脚本，检查贴图尺寸、缺失碰撞、命名、硬引用和未使用资产。
- 定义章节关卡接口：SpawnGroups、ObjectiveSockets、ExtractionSocket、BossArena、Streaming 分区。

**Codex 任务边界：** Codex 可生成验证工具和清单模板，但不能判断你未提供的许可证是否有效。

**UE 编辑器手工项：**

- 制作 1 页视觉风格板和颜色/灯光规范。
- 整理 Marketplace、自制、音频和字体授权文件。

**验收标准：**

- [ ] 新增资产能追溯合法来源。
- [ ] 关卡验证工具能发现缺失关键点位。
- [ ] 测试地图符合命名和性能预算。
- [ ] 不再向根 Content 目录随意堆资源。

**建议分支与提交：**

```text
branch: feature/v0-9-0
commit: v0.9.0 Add production art level standards and license registry
```

**禁止事项 / 风险提示：**

- 不使用无法确认商业授权的模型、音乐、字体或 AI 生成素材。

## v0.9.1 正式舰船大厅、教学与可视化修复

**本次目标：** 把灰盒大厅升级为完整 Hub，使组队、装备、制造、修船、任务和剧情均有空间与交互。

**Codex 编码范围：**

- 为任务台、仓库、制造台、医疗舱、职能终端和修船点提供统一 Interactable 接口。
- 舰船模块修复后广播状态，控制灯光、门、UI 和站点功能。
- 建立新玩家引导状态：移动、装备、任务选择和第一次出发。
- 支持多人大厅中各玩家独立操作 UI，房主负责世界任务选择。

**Codex 任务边界：** 保持已完成 Session 和 Travel 逻辑，只替换地图内容与交互适配。

**UE 编辑器手工项：**

- 完成大厅生产级布局、导航、灯光、音频、玩家出生与镜头。
- 制作 Intro 迫降简短演出或可跳过叙事序列。
- 为已修复模块制作前后视觉差异。

**验收标准：**

- [ ] 新档可以不看外部说明完成第一次出发。
- [ ] 2-4 人大厅 UI/交互互不抢焦点。
- [ ] 模块修复的视觉状态能正确保存和加载。
- [ ] 返回大厅不会出现玩家重叠或错误出生。

**建议分支与提交：**

```text
branch: feature/v0-9-1
commit: v0.9.1 Build production ship hub tutorial and repair visuals
```

**禁止事项 / 风险提示：**

- 不要把大厅做成开放世界；所有区域服务于明确功能。

## v0.9.2 第一章：能源裂隙

**本次目标：** 完成首个生产章节，承担教学后第一轮完整任务和裂隙守卫 Boss。

**Codex 编码范围：**

- 配置能源章节 MissionContracts：采集、守点、资源富集修正和基础可选目标。
- 章节规则引入电力节点、短时断电和能量危险区。
- 掉落池产出能源晶体、基础武器与初级芯片。
- 完成章节解锁、结算、舰船动力炉推进和相关存档。

**Codex 任务边界：** 章节专属逻辑通过 MissionModifier/Actor 组件实现，不在核心 GameMode 写章节名判断。

**UE 编辑器手工项：**

- 制作能源裂隙生产地图、探索支路、任务点、出生组和撤离点。
- 完成裂隙守卫的正式模型/动画/音画表现。
- 进行导航、碰撞、遮挡和性能检查。

**验收标准：**

- [ ] 单人首次任务 15-25 分钟内可完成。
- [ ] 任务目标位置和风险路径清晰。
- [ ] Boss 奖励与动力炉推进正确。
- [ ] 1/2/4 人均可完成且无卡图点。

**建议分支与提交：**

```text
branch: feature/v0-9-2
commit: v0.9.2 Complete Energy Rift chapter and first boss
```

**禁止事项 / 风险提示：**

- 地图先完成玩法和性能，再做最终装饰。

## v0.9.3 第二章：金属废墟

**本次目标：** 完成强调搬运、路线防守和重型敌人的殖民废墟章节。

**Codex 编码范围：**

- 实现可丢下/接力的重型核心搬运接口，服务端同步持有者和速度惩罚。
- 配置金属章节任务、矿点、残骸和护巢者/哨兵组合。
- 制作章节 Boss：装甲破城体，使用可破坏护甲阶段和冲击波。
- 掉落异星合金、护甲和工程工具。

**Codex 任务边界：** 重物是现有 CarryObjective 的扩展，不另建孤立任务系统。

**UE 编辑器手工项：**

- 制作废墟/工厂地图、搬运路线、捷径和防守空间。
- 完成 Boss Arena 与护甲破损视觉。

**验收标准：**

- [ ] 重物在玩家死亡、断线和切换持有者时不丢失/复制。
- [ ] 单人无人机提供轻微交互补偿但不替玩家搬运。
- [ ] Boss 护甲阶段与弱点清晰。
- [ ] 章节通关推进舰体/武器库相关目标。

**建议分支与提交：**

```text
branch: feature/v0-9-3
commit: v0.9.3 Complete Metal Ruins chapter and armored boss
```

**禁止事项 / 风险提示：**

- 搬运时间不能成为纯等待；沿途必须有路线和战斗决策。

## v0.9.4 第三章：生物巢穴

**本次目标：** 完成强调污染、净化、资源风险和群体敌人的有机章节。

**Codex 编码范围：**

- 建立污染累积、阈值 Debuff、净化点和 PollutionResistance 接口。
- 配置样本采集、巢穴摧毁、寄生体/蜂群/喷吐者组合。
- 制作章节 Boss：巢群母体，包含孵化、毒区、可破坏器官和阶段迁移。
- 掉落生物样本、医疗/净化装备和恢复类配方。

**Codex 任务边界：** 污染通过 GAS Tag/Effect 统一实现，避免关卡蓝图直接修改玩家变量。

**UE 编辑器手工项：**

- 制作生物巢穴地图、污染区域可读性和安全区。
- 完成 Boss 器官弱点、音效与污染前摇。

**验收标准：**

- [ ] 任何职能都可通过道具/环境处理污染。
- [ ] 医疗模块更高效但不是必需。
- [ ] 污染状态保存/旅行后不会错误残留。
- [ ] Boss 小怪和环境总预算不超性能上限。

**建议分支与提交：**

```text
branch: feature/v0-9-4
commit: v0.9.4 Complete Bio Nest chapter pollution and hive boss
```

**禁止事项 / 风险提示：**

- 高对比污染视觉需要兼顾色觉差异，后续可访问性版本继续完善。

## v0.9.5 第四章：科技设施

**本次目标：** 完成强调终端破解、远程火力、安防系统和信息优势的设施章节。

**Codex 编码范围：**

- 扩展终端交互为可中断的破解/防守步骤，工程职能有速度优势。
- 配置无人机、哨兵、干扰者和安全门/激光危险。
- 制作章节 Boss：失控主控核心，使用炮台网络、区域锁定和系统过载阶段。
- 掉落数据芯片、高级芯片和通讯阵列升级资源。

**Codex 任务边界：** 终端使用通用 Interactable/Objective，不在 Level Blueprint 保存权威进度。

**UE 编辑器手工项：**

- 制作设施地图、门禁回路、终端空间和 Boss 控制室。
- 配置远程攻击遮挡、掩体和导航。

**验收标准：**

- [ ] 非工程玩家也能破解，工程更快。
- [ ] 多人同时交互不会重复推进。
- [ ] Boss 区域锁定有安全路径和明确前摇。
- [ ] 章节解锁最终深层裂隙情报。

**建议分支与提交：**

```text
branch: feature/v0-9-5
commit: v0.9.5 Complete Technology Facility chapter and core boss
```

**禁止事项 / 风险提示：**

- 远程敌人密度要受导演预算限制，避免无掩体秒杀。

## v0.9.6 第五章：深层裂隙、最终 Boss 与返航结局

**本次目标：** 完成最终章节、跃迁核心修复、最终战、结局和 New Game+ 入口或自由任务模式。

**Codex 编码范围：**

- 配置多阶段最终合同：稳定锚点、回收碎片、最终守卫战和撤离。
- 制作最终 Boss：裂隙意志/入侵核心，复用通用框架并组合前四章机制。
- 结算推进跃迁核心，触发可跳过结局演出、Credits 和通关标记。
- 通关后解锁高难任务/自由合同，不清空玩家档案。

**Codex 任务边界：** 最终 Boss 新能力仍使用现有 Phase/AbilityPattern，不在一个类里写全部逻辑。

**UE 编辑器手工项：**

- 制作深层裂隙地图、最终 Arena、结局序列、Credits 与返回菜单流程。
- 完成全主线从新档到结局的人工通关测试。
- 创建标签 `milestone-v0.9.6-campaign-playable`。

**验收标准：**

- [ ] 新档能够从序章完整通关至 Credits。
- [ ] 最终战单人、2 人、4 人均无软锁。
- [ ] 结局可跳过且不会重复发奖。
- [ ] 通关后档案、装备与自由任务可继续使用。

**建议分支与提交：**

```text
branch: feature/v0-9-6
commit: v0.9.6 Complete Deep Rift finale final boss and ending
```

**禁止事项 / 风险提示：**

- 这一版本标志“主线可玩”，不等于内容完成或可发布。

\newpage

# 阶段 G：UI、设置、手柄、可访问性与本地化（v0.10.0-v0.10.5）

阶段出口：从第一次启动到通关，玩家不需要控制台或开发者说明；键鼠和手柄均可操作；中文与英文可切换；设置和字幕达到可发布标准。

## v0.10.0 UI 架构冻结与生命周期治理

**本次目标：** 整理已有 UMG，不为了追求新框架而重写；建立统一的 Screen、Modal、HUD 和输入模式管理。

**Codex 编码范围：**

- 审计当前 Widget 创建/销毁、PlayerController 绑定和旅行后重建。
- 建立 UIManager/ViewModel 或轻量 Controller 层，管理层级、输入模式、焦点和异步加载。
- 只有在确有手柄/多层菜单需求且迁移成本可控时引入 CommonUI；否则保留 UMG。
- 事件驱动更新属性、任务、背包和 Session 状态。

**Codex 任务边界：** 优先包裹旧 Widget，而不是删除重做。

**UE 编辑器手工项：**

- 整理 Widget 目录、风格、字号、间距和可复用控件。
- 建立 UI 流程图。

**验收标准：**

- [ ] 旅行后 UI 不重复创建。
- [ ] 菜单打开/关闭时输入模式与鼠标正确。
- [ ] 手柄焦点不会丢失。
- [ ] UI 不通过 Tick 拉取核心数据。

**建议分支与提交：**

```text
branch: feature/v0-10-0
commit: v0.10.0 Stabilize UI architecture lifecycle and focus
```

**禁止事项 / 风险提示：**

- CommonUI 不是必需技术点；只有收益大于迁移风险时使用。

## v0.10.1 主菜单、Steam 房间、大厅与配装流程

**本次目标：** 完成商业版入口、档案、创建/加入、邀请、准备、任务、职能和配装流程的 UI。

**Codex 编码范围：**

- 提供 UI 可调用的 Session 异步状态与错误码接口。
- 大厅 PlayerState 变化驱动玩家卡片、准备、职能和房主标识。
- 配装验证服务端执行，UI 显示超载、未解锁或非法装备原因。
- 加载/失败/取消/超时有明确状态。

**Codex 任务边界：** UI 显示真实 Session/PlayerState 数据，不建立本地假状态。

**UE 编辑器手工项：**

- 完成主菜单、档案选择、房间列表、创建房间、大厅、任务台和配装 Widget。
- 制作空列表、断网和错误提示。

**验收标准：**

- [ ] 不使用控制台即可完成创建/加入/开始。
- [ ] Host/Client 权限按钮正确。
- [ ] Session 失败不会把玩家卡在黑屏。
- [ ] 键鼠和手柄均可完成全流程。

**建议分支与提交：**

```text
branch: feature/v0-10-1
commit: v0.10.1 Complete main menu lobby mission and loadout UX
```

**禁止事项 / 风险提示：**

- Steam 功能未接入前可使用 Null 子系统测试，但接口必须异步。

## v0.10.2 HUD、队友信息、任务导航与 Ping

**本次目标：** 让玩家在战斗中清楚理解生命、技能、队友、任务、风险、掉落和撤离。

**Codex 编码范围：**

- HUD 绑定 GAS 属性、Ability 冷却、弹药、快捷栏、任务状态和 RiftStability。
- 队友面板显示生命/护盾、倒地、距离、职能和关键任务物状态。
- 实现服务端验证的 Ping/标记：位置、敌人、资源、目标、撤离。
- 建立提示队列，避免任务、掉落和系统消息互相覆盖。

**Codex 任务边界：** 世界标记使用可复用 MarkerSubsystem，不让每个目标创建独立 HUD 逻辑。

**UE 编辑器手工项：**

- 完成 HUD、指南针/世界标记、Boss 血条、伤害方向和提示动画。
- 测试 16:9、16:10、超宽屏与不同 UI Scale。

**验收标准：**

- [ ] 所有重要信息无需依赖颜色单一表达。
- [ ] 2-4 人 Ping 不会无限刷屏。
- [ ] HUD 在重生/旅行后重新绑定。
- [ ] 不同分辨率无裁切。

**建议分支与提交：**

```text
branch: feature/v0-10-2
commit: v0.10.2 Complete HUD teammate mission navigation and ping system
```

**禁止事项 / 风险提示：**

- Ping 必须有频率限制和玩家屏蔽入口。

## v0.10.3 背包、制造、修船与结算正式界面

**本次目标：** 把局外成长和撤离收益包装成易理解、可比较、可确认的完整体验。

**Codex 编码范围：**

- 提供装备比较数据、词条格式化、资源不足原因、解锁条件和结算动画数据。
- 所有有损操作（丢弃、拆解、覆盖存档）使用确认流程。
- 修船 UI 展示当前阶段、成本、功能收益和下一主线目标。
- 结算 UI 分团队、个人、奖励、主线推进和再次出发。

**Codex 任务边界：** Codex 提供格式化和交易接口，视觉与动画手工完成。

**UE 编辑器手工项：**

- 完成 Inventory、Equipment、Crafting、ShipRepair、MissionResult 的最终布局。
- 制作图标、稀有度样式和文本排版。

**验收标准：**

- [ ] 玩家能理解装备变化和修船收益。
- [ ] 所有有损操作都可取消。
- [ ] 结算不会因为动画跳过而漏写奖励。
- [ ] 手柄焦点和滚动列表稳定。

**建议分支与提交：**

```text
branch: feature/v0-10-3
commit: v0.10.3 Finalize inventory crafting ship repair and result screens
```

**禁止事项 / 风险提示：**

- 不要通过 UI 动画结束事件触发权威存档；结算先完成，动画只展示。

## v0.10.4 设置、按键重绑定、手柄与可访问性

**本次目标：** 提供商业 PC 游戏应有的视频、音频、输入、镜头和可访问性选项。

**Codex 编码范围：**

- 实现 SettingsSubsystem：分辨率、显示模式、帧率、画质、动态分辨率、音量分类和保存/恢复。
- Enhanced Input 重绑定、冲突检测、恢复默认、键鼠/手柄提示自动切换。
- 可访问性：字幕、文本大小、色觉友好标记、镜头震动强度、动态模糊、冲刺/瞄准切换、按住/切换交互。
- 提供首次启动安全默认值和无效设置回滚倒计时。

**Codex 任务边界：** 设置与玩家进度分开保存，避免云存档冲突覆盖本机硬件设置。

**UE 编辑器手工项：**

- 完成设置菜单和输入提示图标。
- 用 Xbox/通用 XInput 手柄实测全流程。

**验收标准：**

- [ ] 设置重启后保留。
- [ ] 无效分辨率会自动回滚。
- [ ] 只用手柄能从启动到通关。
- [ ] 关闭震动/闪烁后仍能读懂战斗前摇。

**建议分支与提交：**

```text
branch: feature/v0-10-4
commit: v0.10.4 Add settings rebinding controller and accessibility options
```

**禁止事项 / 风险提示：**

- 不要承诺未经测试的特定无障碍认证；以实际功能为准。

## v0.10.5 简中/英文地化、教学、叙事日志与 Credits

**本次目标：** 完成所有玩家可见文本的本地化管线，并让主线叙事无需外部文档即可理解。

**Codex 编码范围：**

- 使用 String Table/Localization Dashboard 管理 UI、物品、任务、字幕和成就文本。
- 消除 C++/蓝图硬编码显示字符串，使用 FText 和稳定 Key。
- 建立教学任务、上下文提示、Codex/日志收集和可跳过对白。
- Credits 包含开发、第三方资产、字体、音频和开源许可证归属。

**Codex 任务边界：** Codex 可扫描硬编码文本并生成 String Table 迁移清单，不要自动翻译后直接视为最终稿。

**UE 编辑器手工项：**

- 完成中文和英文首轮翻译、字幕时轴、字体回退与文本溢出检查。
- 进行伪本地化或长文本测试。

**验收标准：**

- [ ] 运行时可切换语言或重启后生效。
- [ ] 没有关键 UI 硬编码中文/英文。
- [ ] 长英文、中文和特殊符号不裁切。
- [ ] Credits 与许可登记表一致。

**建议分支与提交：**

```text
branch: feature/v0-10-5
commit: v0.10.5 Add Chinese English localization tutorial narrative and credits
```

**禁止事项 / 风险提示：**

- 所有营销和游戏文本需人工校对，尤其是技能数值与法务表述。

\newpage

# 阶段 H：Steam 平台与真实联机（v0.11.0-v0.11.5）

阶段出口：真实 Steam 账号可以创建、搜索、邀请和加入；Shipping 构建使用正确 AppID；成就、统计、Rich Presence、云存档、Steam Input 和 SteamPipe 分支可工作。

> Steamworks 入驻、付费和商店筹备不能等到 v0.11 才开始。文档后面的“并行 Steam 发行轨道”要求从现在立即启动；v0.11 是代码与平台功能完成阶段。

## v0.11.0 Steam 配置层、真实 AppID 与构建隔离

**本次目标：** 把开发期 Null/测试配置升级为可在 Editor、Development 和 Shipping 安全切换的 Steam 平台配置。

**Codex 编码范围：**

- 配置 OnlineSubsystemSteam/项目当前采用的 Online Services 层；保持一个项目内明确抽象。
- 区分 LocalNull、SteamDev、SteamShipping 配置；AppID、BuildId 和分支通过安全配置提供。
- Shipping 启动检查 Steam 初始化与所有权失败路径，并给出用户可读错误。
- 处理 `steam_appid.txt` 仅用于本地开发，不随 Steam 正式安装内容发布。

**Codex 任务边界：** 先检查现有 GameInstance Session 接口，适配而不是创建第二套入口。

**UE 编辑器手工项：**

- 在 Steamworks 后台创建应用、Depot 和测试分支。
- 使用真实开发 AppID 和两个测试账号。

**验收标准：**

- [ ] Editor/Null 测试仍可运行。
- [ ] Steam Development 构建能显示 Overlay。
- [ ] Shipping 通过 Steam 启动并识别正确 AppID。
- [ ] 错误 AppID/未启动 Steam 时有清晰处理。

**建议分支与提交：**

```text
branch: feature/v0-11-0
commit: v0.11.0 Add environment separated Steam configuration and AppID
```

**禁止事项 / 风险提示：**

- 不要把私密 Steamworks 凭证、Depot 密码或个人身份资料提交到 Git。

## v0.11.1 Steam Lobby、房间搜索、邀请与 Rich Presence

**本次目标：** 完成玩家实际使用的创建、搜索、加入、好友邀请和当前状态展示。

**Codex 编码范围：**

- 实现异步 Create/Find/Join/Destroy，带请求状态、取消、超时和错误映射。
- Session/Lobby 设置包含版本、地区、难度、人数、是否进行中和可加入策略。
- 支持 Steam Overlay 好友邀请、Join via Presence、命令行/邀请加入回调。
- 设置 Rich Presence：菜单、舰船大厅、裂隙章节、队伍人数。

**Codex 任务边界：** 所有异步 Delegate 在完成/失败/销毁时解除绑定。

**UE 编辑器手工项：**

- 完成真实房间列表、邀请提示和加入确认 UI。
- 两台机器/两个账号跨网络测试。

**验收标准：**

- [ ] 创建、搜索、加入、离开可重复执行。
- [ ] 好友邀请能启动/唤醒游戏并加入。
- [ ] 版本不兼容房间不会加入。
- [ ] 房间已开始/已满有明确提示。

**建议分支与提交：**

```text
branch: feature/v0-11-1
commit: v0.11.1 Add Steam lobbies invites presence and session browser
```

**禁止事项 / 风险提示：**

- 不要只在同一台机器 PIE 测试 Steam；必须使用独立账号与打包构建。

## v0.11.2 Session 生命周期、重连、主机丢失与认证

**本次目标：** 处理商业联机最常见的异常，而不承诺复杂 Host Migration。

**Codex 编码范围：**

- 建立 SessionState：Idle、Creating、Lobby、Traveling、InMission、Returning、Destroying。
- 客户端短暂掉线提供有限重连窗口；服务端以 UniqueNetId 恢复 PlayerState/本局摘要。
- Host 退出时安全终止任务，按明确规则发放/不发放结算，并把客户端送回菜单。
- 启用 SteamAuth/身份校验和 RPC 频率/参数验证；记录拒绝原因。

**Codex 任务边界：** 1.0 明确不做 Host Migration；目标是可预测失败和保护玩家档案。

**UE 编辑器手工项：**

- 制作断线、重连、主机退出和踢出提示。
- 用网络模拟与强制结束进程测试。

**验收标准：**

- [ ] Client 断线后在窗口内可恢复任务。
- [ ] Host 丢失不会让客户端永久黑屏。
- [ ] 未经认证/版本不符的连接被拒绝。
- [ ] 主机丢失不会复制个人奖励。

**建议分支与提交：**

```text
branch: feature/v0-11-2
commit: v0.11.2 Harden session lifecycle reconnect host loss and auth
```

**禁止事项 / 风险提示：**

- 不要把本地存档值当作服务器权威伤害/拾取结果。

## v0.11.3 Steam 成就、统计与游戏内触发

**本次目标：** 把完整游戏进度接入平台功能，同时保证离线/失败不会阻塞玩法。

**Codex 编码范围：**

- 建立 Achievement/Stat Definition 与事件桥接，避免各系统直接写 Steam Key。
- 建议 25-35 个成就，覆盖主线、职能、合作、挑战和探索，避免纯重复刷量。
- 统计包括任务完成、Boss、救援、伤害/治疗等适量聚合；失败可重试队列。
- 开发构建支持清除/模拟，Shipping 关闭调试重置。

**Codex 任务边界：** 代码只引用稳定 API Name；显示文本来自本地化/Steam 后台。

**UE 编辑器手工项：**

- 在 Steamworks 后台建立与代码一致的 API Name、图标和本地化文本。
- 制作游戏内成就通知或使用 Steam Overlay。

**验收标准：**

- [ ] 成就只解锁一次。
- [ ] 离线/Steam API 暂时失败不会阻塞结算。
- [ ] 重新上线后可提交待同步事件。
- [ ] API Name 与后台完全一致。

**建议分支与提交：**

```text
branch: feature/v0-11-3
commit: v0.11.3 Add Steam achievements stats and resilient event bridge
```

**禁止事项 / 风险提示：**

- 不要收集不必要的个人数据或隐藏遥测。

## v0.11.4 Steam Cloud、保存冲突与 Steam Input

**本次目标：** 保护多设备档案，并完成手柄平台体验。

**Codex 编码范围：**

- 将玩家进度档案纳入 Steam Cloud；本机硬件设置保持本地。
- 保存包含版本、时间戳、ProfileGuid；检测本地/云冲突并提供选择或明确最新策略。
- 上传前完成本地原子写入和备份；下载后执行存档迁移。
- 接入 Steam Input 或确保标准 XInput 全功能，提供动态按钮图标映射。

**Codex 任务边界：** 云同步失败不得阻止玩家离线启动；后续再同步。

**UE 编辑器手工项：**

- 配置 Steam Cloud 路径/配额和 Input Action Set。
- 用两台机器制造冲突并验证 UI。

**验收标准：**

- [ ] 不同机器能同步同一档案。
- [ ] 冲突不会静默覆盖更重要进度。
- [ ] 损坏云档可回退本地备份。
- [ ] 手柄按钮提示随设备切换。

**建议分支与提交：**

```text
branch: feature/v0-11-4
commit: v0.11.4 Add Steam Cloud conflict handling and Steam Input
```

**禁止事项 / 风险提示：**

- 不要把日志、缓存和机器专属画质设置上传云端。

## v0.11.5 SteamPipe、Depot、分支与自动上传

**本次目标：** 建立可重复的 BuildCookRun -> SteamPipe 上传 -> 分支测试 -> 回滚流程。

**Codex 编码范围：**

- 添加 BuildGraph/脚本封装 Editor 编译、Cook、Stage、Package、符号归档和 SteamPipe ContentBuilder。
- 定义 Depot、默认分支、internal、qa、playtest、demo、release-candidate 分支。
- 输出 Build Manifest：版本、Git Commit、UE 版本、SaveVersion、内容哈希和已知问题。
- 优化 Pak/IoStore 分块和文件布局，避免小改动造成超大补丁。

**Codex 任务边界：** Codex 可写脚本和配置模板；实际 Steam 登录、上传和分支设置需人工执行。

**UE 编辑器手工项：**

- 在 Steamworks 上传测试 Build，先设为 internal/qa。
- 验证切换分支、回滚上一 Build 和干净安装。
- 创建标签 `milestone-v0.11.5-steam-platform-complete`。

**验收标准：**

- [ ] 一条命令/脚本可生成可上传包。
- [ ] internal 分支可安装并完成任务。
- [ ] 上一 Build 可在后台快速回滚。
- [ ] Build 不包含开发 AppID 文件和非发行调试资产。

**建议分支与提交：**

```text
branch: feature/v0-11-5
commit: v0.11.5 Add SteamPipe build upload branches and rollback pipeline
```

**禁止事项 / 风险提示：**

- 上传脚本中的账号信息使用环境变量/本机安全配置，不提交仓库。

\newpage

# 阶段 I：Alpha、诊断、自动化与 Steam Playtest（v0.12.0-v0.12.5）

阶段出口：核心功能不再新增；真实玩家通过 Steam Playtest 完成多轮测试；崩溃、存档、网络和体验问题有可复现记录；所有 Blocker 和大部分 Critical 问题清零。

## v0.12.0 Feature Complete Alpha 冻结

**本次目标：** 声明核心功能齐全，建立 Alpha 分支和变更控制；之后只允许修复、平衡和已批准的小型 UX 改善。

**Codex 编码范围：**

- 创建 Feature Matrix，逐项标记 Done、Known Issue、Cut、Post-Launch。
- 建立 Bug 严重度和发布门槛：Blocker、Critical、Major、Minor。
- 锁定 SaveVersion 迁移规则、网络协议/BuildVersion 和内容 ID。
- 建立 `release/0.12-alpha` 分支与每日可安装构建。

**Codex 任务边界：** Codex 仅处理清单中的缺口和 Bug，不主动扩展系统。

**UE 编辑器手工项：**

- 从新档通关一次并记录所有流程。
- 更新商店描述中的实际功能，删除未完成承诺。

**验收标准：**

- [ ] 所有核心 Feature 有可运行入口。
- [ ] 没有“计划中”系统被 UI 假装成已完成。
- [ ] 新档到结局无阻断。
- [ ] Alpha 分支可由测试者安装。

**建议分支与提交：**

```text
branch: feature/v0-12-0
commit: v0.12.0 Freeze feature complete alpha scope
```

**禁止事项 / 风险提示：**

- 任何新功能必须同时说明要延后或删除的同等工作量。

## v0.12.1 崩溃诊断、日志打包与隐私安全

**本次目标：** 让外部测试反馈能够定位到构建、会话和具体错误，同时避免采集敏感数据。

**Codex 编码范围：**

- 给每个启动/任务生成 BuildId、RunId、SessionId 和支持码。
- 崩溃/失败界面提供复制支持码、日志路径和反馈说明。
- 日志轮转、大小上限、关键系统上下文和 Shipping 安全级别。
- 可选错误上报必须明确告知并遵守所用服务要求；不需要时保持本地日志流程。

**Codex 任务边界：** Codex 可扫描日志调用和敏感字段，但最终隐私政策需人工审核。

**UE 编辑器手工项：**

- 制作 Bug Report 模板：步骤、预期、实际、BuildId、人数、日志。
- 检查日志中无令牌、密码、个人路径或完整 Steam 标识。

**验收标准：**

- [ ] 故意触发测试错误能得到支持码。
- [ ] 日志足以识别地图、任务、RunId 和网络角色。
- [ ] 日志不会无限增长。
- [ ] 隐私敏感数据被脱敏。

**建议分支与提交：**

```text
branch: feature/v0-12-1
commit: v0.12.1 Add privacy safe crash diagnostics and support logs
```

**禁止事项 / 风险提示：**

- 不要静默上传用户文件或未披露遥测。

## v0.12.2 完整 QA 矩阵与网络自动化

**本次目标：** 将单人、2 人、4 人、旅行、重连、存档和五章流程形成固定回归套件。

**Codex 编码范围：**

- 扩展 Automation/Functional Tests：任务图、Boss 重置、存档迁移、云冲突模拟、事务幂等。
- 建立 Gauntlet/命令行多实例 Smoke Test（若投入合理），至少自动启动 Server/Client 并完成连接。
- 定义网络模拟档：延迟、丢包、抖动、断线重连。
- 生成测试报告并关联 Git Commit/BuildId。

**Codex 任务边界：** 自动化从最稳定路径开始，不要为了追求数量构建脆弱脚本。

**UE 编辑器手工项：**

- 准备测试账号、测试存档、章节快速入口和 1/2/4 人人工清单。
- 每周固定执行完整回归。

**验收标准：**

- [ ] 所有自动化测试稳定重复通过。
- [ ] 关键联机场景有人工与自动记录。
- [ ] 存档迁移覆盖 Alpha 前至少两个版本。
- [ ] 失败测试能明确定位而非随机超时。

**建议分支与提交：**

```text
branch: feature/v0-12-2
commit: v0.12.2 Add full QA matrix and network automation tests
```

**禁止事项 / 风险提示：**

- 测试环境和正式环境 AppID/分支必须隔离。

## v0.12.3 内部 Alpha 与长时间 Soak Test

**本次目标：** 在邀请外部玩家前，用固定测试队完成全流程、重复任务和长时间运行。

**Codex 编码范围：**

- 修复内部测试发现的崩溃、死锁、内存增长、重复结算、存档损坏和旅行失败。
- 增加关键性能计数和网络统计采样。
- 执行 2-4 小时连续任务、反复旅行、创建/销毁 Session 和存档循环。
- 维护 Bug Burn-down 和每日可玩状态。

**Codex 任务边界：** 每个 Bug 单独复现和修复，不让 Codex基于模糊描述大改。

**UE 编辑器手工项：**

- 组织至少 3 轮：单人全通、2 人全通、4 人高压。
- 记录疲劳点、迷路点、卡关点和不清楚的 UI。

**验收标准：**

- [ ] 连续 2 小时无崩溃和明显内存持续增长。
- [ ] 2 人完整主线可通。
- [ ] 4 人至少完成各章节代表任务。
- [ ] 所有 Blocker 清零。

**建议分支与提交：**

```text
branch: feature/v0-12-3
commit: v0.12.3 Run internal alpha and multiplayer soak tests
```

**禁止事项 / 风险提示：**

- 内部测试期间仍禁止添加新章节或新职业。

## v0.12.4 Steam Playtest 第一轮

**本次目标：** 使用独立 Playtest AppID 小范围验证真实安装、网络、教学、难度和硬件兼容。

**Codex 编码范围：**

- 接入 Playtest 分支的 AppID/构建配置和可选反馈入口。
- 提供版本水印、已知问题、隐私说明和反馈表链接。
- 统计只收集必要聚合事件且明确披露；也可完全依赖问卷和日志。
- 根据反馈修复崩溃、无法加入、教学阻断和存档问题。

**Codex 任务边界：** Codex 处理带 BuildId 和复现步骤的问题；反馈中主观建议先分类，不直接全做。

**UE 编辑器手工项：**

- 在 Steamworks 配置 Playtest、测试者容量和访问批次。
- 发布测试公告、FAQ、Bug 模板和社区规则。

**验收标准：**

- [ ] 不同地区玩家能安装并加入好友。
- [ ] 至少收集一批完整首小时和一次通关反馈。
- [ ] 高频问题有明确优先级。
- [ ] Playtest 不影响正式商店评价与主应用购买。

**建议分支与提交：**

```text
branch: feature/v0-12-4
commit: v0.12.4 Launch first Steam Playtest wave and fix blockers
```

**禁止事项 / 风险提示：**

- 测试者数量宁少而可支持，不要一次开放到无法处理反馈。

## v0.12.5 Alpha 第二轮与退出门槛

**本次目标：** 验证第一轮修复，并把项目推进到可以锁定内容的状态。

**Codex 编码范围：**

- 修复剩余 Critical，关闭无法复现问题或增加诊断。
- 复测连接、主机丢失、云存档、结算、五章和最终 Boss。
- 整理 Alpha 指标：完成率、任务时长、失败原因、常用职能、崩溃率。
- 创建 Alpha 退出报告与 `milestone-v0.12.5-alpha-exit` 标签。

**Codex 任务边界：** 指标用于发现问题，不为了数字而改变核心定位。

**UE 编辑器手工项：**

- 进行第二批或回访测试。
- 更新 Known Issues 和后续 Beta 重点。

**验收标准：**

- [ ] Blocker=0，Critical=0 或有明确获批例外。
- [ ] 主线首次通关率达到可接受水平。
- [ ] Steam 加入/邀请成功率稳定。
- [ ] 存档损坏无已知高概率路径。

**建议分支与提交：**

```text
branch: feature/v0-12-5
commit: v0.12.5 Complete second alpha wave and exit report
```

**禁止事项 / 风险提示：**

- 未通过退出门槛就继续 Alpha，不用版本号掩盖问题。

\newpage

# 阶段 J：Content Complete Beta（v0.13.0-v0.13.5）

阶段出口：所有首发内容、文本、成就和授权内容锁定；平衡、单人/多人、本地化和存档兼容经过 Beta；不再添加首发内容，只修复与打磨。

## v0.13.0 Content Complete 内容锁定

**本次目标：** 冻结五章、敌人、Boss、物品、任务、剧情、语言和首发功能清单。

**Codex 编码范围：**

- 生成内容清单并验证所有 PrimaryAssetId、ItemId、MissionId、AchievementId 唯一。
- 删除不可达、未授权、占位和无引用的发行资产；保留测试资产在非发行目录。
- 锁定 SaveVersion 与内容迁移表，后续删除/重命名 ID 必须有 Redirect。
- 创建 `milestone-v0.13.0-content-complete` 标签。

**Codex 任务边界：** Codex 运行验证工具并生成差异清单，不自动删除不确定资产。

**UE 编辑器手工项：**

- 人工检查所有章节、Boss、物品图标、文本、字幕、Credits。
- 完成商店页实际功能核对。

**验收标准：**

- [ ] 所有首发内容能在正常流程获得/进入。
- [ ] 没有 Missing Asset、占位文字或测试立方体。
- [ ] 版权许可清单完整。
- [ ] 从旧 Alpha 存档可升级。

**建议分支与提交：**

```text
branch: feature/v0-13-0
commit: v0.13.0 Freeze content complete release scope
```

**禁止事项 / 风险提示：**

- 此后不新增职业、章节、Boss 或大型系统。

## v0.13.1 进度、掉落与制造经济平衡

**本次目标：** 让玩家在 8-12 小时主线中持续获得目标，不出现严重资源墙或无意义溢出。

**Codex 编码范围：**

- 将所有成本、掉落、升级和章节奖励导出为数据表/分析报告。
- 建立模拟器估算任务次数、资源收入、装备成长和最差随机情况。
- 调整低保、拆解返还、修船成本和难度奖励倍率。
- 记录 BalanceVersion 以便比较测试结果。

**Codex 任务边界：** Codex 可生成模拟和报告，最终数值由实际游玩决定。

**UE 编辑器手工项：**

- 使用新档进行至少三条路径测试：普通、保守搜刮、高难挑战。
- 邀请不同水平玩家测试。

**验收标准：**

- [ ] 主线不会因为必要资源纯随机而卡住。
- [ ] 高级难度奖励值得但不强制。
- [ ] 制造和掉落都有用途。
- [ ] 通关后仍有适量可重复目标。

**建议分支与提交：**

```text
branch: feature/v0-13-1
commit: v0.13.1 Balance progression loot crafting and ship economy
```

**禁止事项 / 风险提示：**

- 不要用无限刷取来延长时长；完整度优先于时长数字。

## v0.13.2 单人、2 人、4 人与难度平衡

**本次目标：** 确保每种队伍规模和三个难度都能完成且体验不同。

**Codex 编码范围：**

- 调整 ScalingSnapshot、敌人预算、目标时间、Boss 机制人数适配和救援规则。
- 为单人避免同时要求远距离并行操作；必要时无人机提供任务辅助。
- 为多人增加敌人组合/目标压力，而不是只加血。
- 收集每章任务时间、倒地、失败原因和职能胜率。

**Codex 任务边界：** 使用统一缩放配置，不在各关卡手工写人数分支。

**UE 编辑器手工项：**

- 建立固定测试 Build 和标准装备档案。
- 1/2/4 人分别完整通关代表难度。

**验收标准：**

- [ ] 任何职能可单人完成普通难度。
- [ ] 4 人不会因火力倍增而完全失去压力。
- [ ] Boss 机制不会因玩家数量软锁。
- [ ] 难度说明与实际一致。

**建议分支与提交：**

```text
branch: feature/v0-13-2
commit: v0.13.2 Balance solo two player four player and difficulties
```

**禁止事项 / 风险提示：**

- 不要通过极端敌人血量制造难度。

## v0.13.3 本地化、可访问性与 UX QA

**本次目标：** 对简中、英文、键鼠、手柄、不同分辨率和可访问性设置做完整回归。

**Codex 编码范围：**

- 扫描未本地化 FText、格式参数、复数/数字和字幕遗漏。
- 修复焦点循环、返回路径、弹窗层级、超宽屏安全区和文本溢出。
- 验证镜头震动、字幕、文本缩放、颜色替代提示和按住/切换设置。
- 建立语言/设备 QA 清单。

**Codex 任务边界：** Codex 可扫描代码和 Widget 数据接口，视觉验收必须人工。

**UE 编辑器手工项：**

- 人工校对所有中文和英文。
- 用至少两种显示比例和一款手柄完整游玩。

**验收标准：**

- [ ] 所有关键文本可读且术语一致。
- [ ] 只用手柄无死焦点。
- [ ] UI Scale 不导致按钮不可达。
- [ ] 重要战斗信息不只靠颜色传达。

**建议分支与提交：**

```text
branch: feature/v0-13-3
commit: v0.13.3 Complete localization accessibility and UX QA
```

**禁止事项 / 风险提示：**

- 翻译变更同步 Steam 商店和成就文本。

## v0.13.4 成就、Credits、法务、许可证与存档兼容锁定

**本次目标：** 完成发行所需的非玩法内容，并验证没有法律和兼容性遗漏。

**Codex 编码范围：**

- 运行所有成就触发测试并防止调试命令在 Shipping 解锁。
- 生成第三方许可证/归属清单，检查开源代码、插件、字体、音乐和 Marketplace 资产。
- 建立最终 EULA/隐私政策/支持信息占位与链接配置；实际法律文本需人工确认。
- 完成 Alpha -> Beta -> RC 的存档迁移测试。

**Codex 任务边界：** Codex 只生成清单和技术接入，不提供法律意见。

**UE 编辑器手工项：**

- 在 Steamworks 上传最终成就图标和本地化。
- 人工核对 Credits、公司/个人发行名称和版权年份。

**验收标准：**

- [ ] 所有成就可正常解锁且没有不可达项。
- [ ] 发行资产均有许可记录。
- [ ] 旧测试存档可升级或得到明确不兼容提示。
- [ ] 支持与隐私入口可访问。

**建议分支与提交：**

```text
branch: feature/v0-13-4
commit: v0.13.4 Lock achievements legal credits licenses and save compatibility
```

**禁止事项 / 风险提示：**

- 未经专业确认不要声称某法务文本“完全合规”。

## v0.13.5 公开 Beta 与 Bug Burn-down

**本次目标：** 扩大测试覆盖，集中清零崩溃、联网、存档和主线问题。

**Codex 编码范围：**

- 发布 Beta 分支/Playtest 新 Build，维护公开 Known Issues。
- 按严重度修复：崩溃 -> 存档/奖励 -> 加入/旅行 -> 主线软锁 -> 性能 -> UX。
- 每次修复补回归测试或明确复现脚本。
- 建立 Release Blocker Dashboard 和每日候选 Build。

**Codex 任务边界：** 一个 Commit 只解决一个可复现问题或紧密相关的一组问题。

**UE 编辑器手工项：**

- 组织至少一轮 4 人跨网络完整章节测试。
- 收集低配、不同驱动、Steam Deck/Proton 早期反馈。

**验收标准：**

- [ ] Blocker=0，Critical=0。
- [ ] 主线/Session/Save 无高频 Major。
- [ ] Beta Build 可连续运行两小时。
- [ ] 创建标签 `milestone-v0.13.5-beta-exit`。

**建议分支与提交：**

```text
branch: feature/v0-13-5
commit: v0.13.5 Complete public beta and critical bug burn down
```

**禁止事项 / 风险提示：**

- Beta 期间禁止内容扩张。

\newpage

# 阶段 K：性能、包体、兼容与最终打磨（v0.14.0-v0.14.5）

阶段出口：目标硬件、网络、加载和包体达到预算；Steam Deck/Proton 有明确兼容结果；Release Candidate 清单通过。

## v0.14.0 性能预算与基准场景

**本次目标：** 先建立可测预算，再做有证据的优化。

**Codex 编码范围：**

- 定义最低/推荐硬件目标、分辨率、画质档和 FPS 目标。
- 建立大厅、普通战斗、4 人高压、Boss、Inventory UI、加载等基准场景。
- 记录 GameThread、RenderThread、GPU、内存、VRAM、AI、网络带宽和加载时间。
- 自动输出 Build 间对比报告。

**Codex 任务边界：** Codex 可自动化命令和报告，不凭猜测修改渲染设置。

**UE 编辑器手工项：**

- 选择至少一台最低目标机和一台推荐机。
- 固定相机/任务 Seed 采样。

**验收标准：**

- [ ] 每个场景有可重复基线。
- [ ] 性能回归能定位到具体 Build。
- [ ] 画质档差异符合预期。
- [ ] 最低目标不是只在空地图测试。

**建议分支与提交：**

```text
branch: feature/v0-14-0
commit: v0.14.0 Define performance budgets and repeatable benchmarks
```

**禁止事项 / 风险提示：**

- 未经基准证明不要进行大规模“优化重构”。

## v0.14.1 CPU、GPU、AI 与网络优化

**本次目标：** 解决基准中最昂贵的实际瓶颈，并保持玩法一致。

**Codex 编码范围：**

- 优化 Tick、组件更新、感知频率、AI 路径、Actor 数量、复制频率和 Net Cull Distance。
- 使用对象池需谨慎，只在 Profile 证明 Spawn/Destroy 成为瓶颈时引入。
- 优化 Niagara、阴影、灯光、材质和远距离表现；建立 Scalability。
- Replication Graph/Iris 高级方案只有在 4 人+AI 预算证明需要时引入。

**Codex 任务边界：** 每项优化附前后数据；Codex 不得一次替换底层网络架构。

**UE 编辑器手工项：**

- 使用 Unreal Insights、Stat GPU、Network Profiler 对比前后。
- 逐章复测视觉和 AI 行为。

**验收标准：**

- [ ] 关键场景达到目标或有明确例外记录。
- [ ] 优化不改变任务/伤害结果。
- [ ] 网络带宽在 4 人高压场景可接受。
- [ ] AI 不因降频明显失去反应。

**建议分支与提交：**

```text
branch: feature/v0-14-1
commit: v0.14.1 Optimize CPU GPU AI and network from profiling data
```

**禁止事项 / 风险提示：**

- 不要过度降低画质掩盖 CPU/网络瓶颈。

## v0.14.2 内存、加载、着色器与 PSO

**本次目标：** 降低首启卡顿、章节加载和峰值内存，减少正式版着色器卡顿。

**Codex 编码范围：**

- 审计硬引用链、同步加载、重复贴图/材质、音频常驻和未释放 Widget。
- 使用软引用和异步加载预取职能、武器、章节与 Boss 资产。
- 建立 PSO/Shader Pipeline Cache 收集与发行流程（按 UE5.8 实际支持配置）。
- 检查旅行后旧世界、AI、UI 和资源是否被引用导致泄漏。

**Codex 任务边界：** 先用 Reference Viewer/Insights 查证，逐步替换硬引用。

**UE 编辑器手工项：**

- 干净机器首次启动、首次技能、首次 Boss 技能采样。
- 逐章记录加载和峰值内存。

**验收标准：**

- [ ] 重复旅行内存回落到稳定范围。
- [ ] 常用战斗表现不出现严重首次卡顿。
- [ ] 加载界面不会长时间假死。
- [ ] 打包后软引用资源不缺失。

**建议分支与提交：**

```text
branch: feature/v0-14-2
commit: v0.14.2 Optimize memory async loading shaders and PSO cache
```

**禁止事项 / 风险提示：**

- 不要在主线程同步加载大型章节资源。

## v0.14.3 Cook、Pak/IoStore、补丁体积与安装验证

**本次目标：** 让 Steam 更新大小、安装和回滚适合真实玩家。

**Codex 编码范围：**

- 清理无引用资产、EditorOnly 数据和开发测试内容。
- 按共享基础/章节/音频合理组织 Chunk 或容器，避免频繁变更文件打散。
- 比较全量安装、增量补丁和分支切换大小。
- 自动进行干净安装、升级安装、回滚安装和文件校验。

**Codex 任务边界：** 遵循 SteamPipe 对 pack file 布局和局部更新的建议；用实测 Depot 差异决策。

**UE 编辑器手工项：**

- 从 Steam internal 分支安装而不是只运行 Stage 目录。
- 模拟上一版本升级到当前版本。

**验收标准：**

- [ ] 干净安装可启动。
- [ ] 增量补丁大小合理且不重新下载绝大部分内容。
- [ ] 回滚后旧存档得到兼容处理。
- [ ] Steam Verify 后可正常运行。

**建议分支与提交：**

```text
branch: feature/v0-14-3
commit: v0.14.3 Optimize cook chunks patch size and installation validation
```

**禁止事项 / 风险提示：**

- 不要在临近发布时无理由更换全部压缩/容器策略。

## v0.14.4 最低配置、Steam Deck/Proton 与设备兼容

**本次目标：** 给商店页提供真实系统需求，并明确 Steam Deck 兼容状态。

**Codex 编码范围：**

- 检测 Windows 版本、GPU 功能、显存不足、缺失运行库和不支持配置的错误路径。
- 修复 Proton 下路径、大小写、影片编码、输入、云存档和 Overlay 问题。
- 提供可自动选择的低画质和安全启动参数。
- 记录 Steam Deck 结果；只申请实际达到的 Verified/Playable 状态。

**Codex 任务边界：** Codex 可修兼容代码和路径问题，硬件性能必须实机确认。

**UE 编辑器手工项：**

- 在最低目标 PC、推荐 PC 和 Steam Deck/Proton 设备实测。
- 验证 1280x800、手柄、虚拟键盘需求和文字尺寸。

**验收标准：**

- [ ] 最低配置能完成代表任务。
- [ ] Steam Deck 可启动、操作和读字，或有准确已知限制。
- [ ] 商店系统需求由实测数据支持。
- [ ] 无键盘情况下不会卡在必要文本输入。

**建议分支与提交：**

```text
branch: feature/v0-14-4
commit: v0.14.4 Validate minimum specs Steam Deck Proton and devices
```

**禁止事项 / 风险提示：**

- 不要在未通过 Valve 流程前宣传“Verified”。

## v0.14.5 最终音画、UI 打磨与 RC Gate

**本次目标：** 完成最终一致性检查，进入只允许发布阻断修复的 Release Candidate 阶段。

**Codex 编码范围：**

- 清除临时日志、调试水印、占位资源、开发作弊和非发行菜单。
- 统一混音、响度、字幕、VFX 可读性、Boss 前摇、UI 动画和 Loading。
- 运行全部自动化、五章通关、1/2/4 人、云存档、成就、分支、安装和回滚测试。
- 生成 RC Gate 报告与符号/崩溃调试包。

**Codex 任务边界：** Codex 只修复清单中的具体问题，所有变更必须经过完整回归。

**UE 编辑器手工项：**

- 进行最终截图级视觉巡检和音频监听。
- 建立 `release/1.0-rc` 分支和 `milestone-v0.14.5-rc-gate` 标签。

**验收标准：**

- [ ] 所有 RC 清单通过。
- [ ] Blocker/Critical=0。
- [ ] 没有未授权或占位内容。
- [ ] Shipping Build 在干净 Steam 安装中全流程通过。

**建议分支与提交：**

```text
branch: feature/v0-14-5
commit: v0.14.5 Pass final polish and release candidate gate
```

**禁止事项 / 风险提示：**

- 进入 RC 后冻结依赖、插件、UE 版本和大规模资产迁移。

\newpage

# 阶段 L：Steam 商店审核、Gold Master 与发布（v0.15.0-v1.0.x）

阶段出口：商店和构建通过 Valve 审核，Gold Master 可安装、可回滚、可支持；正式发布后有明确热修复和玩家支持机制。

## v0.15.0 商店页、预告片、截图、定价与发布资料定稿

**本次目标：** 让商店展示与实际游戏完全一致，并完成审核所需信息。

**Codex 编码范围：**

- 从构建自动输出版本号、系统需求、支持邮箱/页面和隐私/许可链接。
- 核对成就、语言、控制器、多人、云存档等 Store Feature 与实际实现一致。
- 生成媒体捕获构建，隐藏开发 UI 并提供可复现截图/预告片场景。
- 维护商店文本、游戏内文本和实际功能的最终一致性清单。

**Codex 任务边界：** Codex 可生成清单和导出版本信息，营销素材必须人工制作与审核。

**UE 编辑器手工项：**

- 完成胶囊图、库图、截图、短/长描述、预告片、定价、首发折扣计划、内容调查和系统需求。
- 确保 Coming Soon 已公开并积累愿望单。

**验收标准：**

- [ ] 商店声称的所有功能已在 Shipping Build 中可用。
- [ ] 截图和视频来自实际游戏。
- [ ] 中文/英文商店文本校对完成。
- [ ] 发行名称、版权和支持信息一致。

**建议分支与提交：**

```text
branch: feature/v0-15-0
commit: v0.15.0 Finalize Steam store assets pricing and release metadata
```

**禁止事项 / 风险提示：**

- 不要宣传未实装路线图、Steam Deck Verified 或 Dedicated Server。

## v0.15.1 RC1 构建、分支锁定与零阻断验证

**本次目标：** 生成准备送审的第一候选构建，并冻结默认分支以外的发布内容。

**Codex 编码范围：**

- 从干净 Tag 生成 Shipping RC1、Debug Symbols、Build Manifest 和校验值。
- 将 RC1 上传 `release-candidate` 分支，执行自动与人工全回归。
- 记录所有已知问题和是否影响审核/玩家。
- 确保 Release Notes、存档版本和网络 BuildVersion 固定。

**Codex 任务边界：** 任何修复从 RC 分支开短分支并 cherry-pick，禁止混入开发中功能。

**UE 编辑器手工项：**

- 在至少两台干净机器从 Steam 安装 RC1。
- 完成新档、旧档、单人、2 人、邀请、结局代表流程。

**验收标准：**

- [ ] RC1 可干净安装、更新、回滚。
- [ ] 无 Blocker/Critical。
- [ ] 所有 Store Feature 可验证。
- [ ] 符号和 Commit 可追溯。

**建议分支与提交：**

```text
branch: feature/v0-15-1
commit: v0.15.1 Build and validate Steam release candidate one
```

**禁止事项 / 风险提示：**

- RC1 失败就生成 RC2，不在同一 Build 上偷偷替换不可追溯文件。

## v0.15.2 提交 Steam 商店页与产品构建审核

**本次目标：** 按 Valve 流程提交接近最终状态的商店页和构建，并保留充足修复时间。

**Codex 编码范围：**

- 确认 Build 启动、支持的 OS、购买/所有权、控制器、语言和所有声称功能。
- 准备审核说明：如何开始单人、如何创建/加入多人、测试存档或快速路径。
- 锁定审核 BuildId 和商店页版本。
- 建立审核反馈 Issue 模板。

**Codex 任务边界：** 此版本主要是发行操作；Codex 只帮助检查配置和生成审核说明。

**UE 编辑器手工项：**

- 在 Steamworks 提交 Store Page Review 和 Build Review。
- 至少提前 7 个工作日安排，不把审核时间压到发售当天。

**验收标准：**

- [ ] 后台显示审核已提交。
- [ ] 审核人员可按说明访问全部关键功能。
- [ ] 提交内容与 RC1 BuildId 一致。
- [ ] 团队/个人预留修复窗口。

**建议分支与提交：**

```text
branch: feature/v0-15-2
commit: v0.15.2 Submit Steam store page and product build review
```

**禁止事项 / 风险提示：**

- 不要提交明显未完成、无法启动或商店功能虚假的构建。

## v0.15.3 审核反馈修复与 RC2

**本次目标：** 只处理 Valve 审核反馈和确认的发布阻断问题，生成可追溯的新候选版本。

**Codex 编码范围：**

- 每条反馈建立 Issue、复现、最小修复、回归测试和变更说明。
- 若涉及商店声称不一致，优先修功能或诚实修改商店页。
- 生成 RC2，上传候选分支并重新提交需要复审的部分。
- 验证 RC1 存档升级到 RC2。

**Codex 任务边界：** 禁止借审核修复加入新内容。

**UE 编辑器手工项：**

- 人工复核商店页、构建、语言和内容调查。
- 在干净机器验证 RC2。

**验收标准：**

- [ ] 所有审核反馈关闭。
- [ ] RC2 全回归通过。
- [ ] 旧 RC 存档可兼容。
- [ ] Steam 后台审核通过。

**建议分支与提交：**

```text
branch: feature/v0-15-3
commit: v0.15.3 Address Steam review feedback and validate RC2
```

**禁止事项 / 风险提示：**

- 如果审核要求与计划冲突，记录决定并更新商店页/路线，不隐瞒限制。

## v0.15.4 发布演练、回滚、支持与社区准备

**本次目标：** 在真正点击发布前完整演练上线、事故回滚、补丁和玩家支持。

**Codex 编码范围：**

- 建立 Launch Checklist、回滚 BuildId、Hotfix 分支、版本公告模板和支持 FAQ。
- 验证默认分支切换、回滚旧 Build、紧急补丁上传和存档兼容。
- 准备常见问题诊断：启动失败、无法加入、云存档冲突、黑屏、性能。
- 建立发布日监控：崩溃、评论、讨论区、支持邮箱和 Build 状态。

**Codex 任务边界：** Codex 可生成脚本/模板，但 Steam 后台操作必须人工双重确认。

**UE 编辑器手工项：**

- 使用不可公开的测试分支进行一次完整“假发布”。
- 准备公告、补丁说明、社区规则和已知问题。

**验收标准：**

- [ ] 30 分钟内可回滚到已知稳定 Build。
- [ ] Hotfix 流程在测试分支演练成功。
- [ ] 支持人员/本人能根据支持码定位 Build。
- [ ] 发布日职责和响应顺序明确。

**建议分支与提交：**

```text
branch: feature/v0-15-4
commit: v0.15.4 Rehearse launch rollback hotfix and player support
```

**禁止事项 / 风险提示：**

- 不要在发布日第一次尝试默认分支切换或回滚。

## v0.15.5 Gold Master

**本次目标：** 选择通过审核和全回归的最终 Build，冻结为 Gold Master。

**Codex 编码范围：**

- 创建签名/哈希、Build Manifest、Git Tag、符号归档、源代码快照和最终数据库/资产清单。
- 关闭开发者命令、测试账号入口和非发行分支引用。
- 确认默认分支尚未意外公开新 Build。
- 生成最终 Release Notes 和 Known Issues。

**Codex 任务边界：** 该版本原则上不再改代码；任何问题都必须决定延迟或生成新 Gold。

**UE 编辑器手工项：**

- 从 Steam 安装 Gold Build 并完成最终 Sanity Test。
- 创建标签 `v1.0.0-gold`。

**验收标准：**

- [ ] Gold Build 与审核通过 Build 一致。
- [ ] 安装、启动、单人、联机、存档、结局 Sanity 通过。
- [ ] 回滚目标存在。
- [ ] 所有发布资料归档。

**建议分支与提交：**

```text
branch: feature/v0-15-5
commit: v0.15.5 Freeze and archive the gold master build
```

**禁止事项 / 风险提示：**

- 不要在 Gold 后直接修改同一版本号的文件。

## v0.15.6 发布前最终检查

**本次目标：** 完成日期、价格、折扣、分支、公告、支持和后台发布按钮前的最后人工检查。

**Codex 编码范围：**

- 核对游戏内版本、Steam BuildId、Depot、默认分支、成就、云存档、语言和商店功能。
- 确认首发公告、补丁说明、支持地址、社区和已知问题均可访问。
- 确认发布窗口内有人可响应关键问题。
- 停止所有非 Hotfix 分支合并。

**Codex 任务边界：** 本版本不需要 Codex 主导，主要是人工发行确认。

**UE 编辑器手工项：**

- 在 Steamworks 按 Release Process 清单逐项确认。
- 备份后台关键配置截图/记录。

**验收标准：**

- [ ] 后台所有发布检查为绿色。
- [ ] Gold Build 已设为准备发布的默认候选。
- [ ] 价格/折扣/时间正确。
- [ ] 支持和回滚就绪。

**建议分支与提交：**

```text
branch: feature/v0-15-6
commit: v0.15.6 Complete final pre launch checks
```

**禁止事项 / 风险提示：**

- 发布按钮操作前再次核对 AppID，避免对错误应用操作。

# v1.0.0 Steam 正式发布

**目标：** 将 Gold Master 设为公开默认分支，完成 Steam 发布，并在发布窗口持续监控。

**发布日动作：**

```text
1. 最后一次安装并启动 Gold Build。
2. 按 Steamworks Release Process 正式发布。
3. 发布公告和 Known Issues。
4. 监控崩溃、无法启动、Session、云存档和支付/所有权相关反馈。
5. 只处理 Blocker/Critical；普通建议进入后续版本池。
6. 每个 Hotfix 都先走 internal/qa 分支，再推默认分支。
```

**v1.0.0 验收：**

- 商店可购买/下载，所有权检查正常。
- 新玩家可从安装到进入第一局。
- 好友可创建、邀请、加入和完成任务。
- 云存档、成就和 Rich Presence 正常。
- 支持与回滚流程可用。

# v1.0.1-v1.0.3 首发热修复窗口

## v1.0.1 启动、崩溃与严重兼容热修复

只修复无法启动、频繁崩溃、关键硬件不兼容、商店 Build 配置错误等问题。禁止改平衡和内容。必须保留 v1.0.0 存档兼容，并在 internal 分支完成 Sanity 后发布。

## v1.0.2 联机、存档与奖励热修复

处理高频 Session 失败、云冲突、结算丢失、奖励复制、主线软锁。每项修复必须有复现与回归测试。涉及奖励补偿时使用明确、幂等的补偿事务。

## v1.0.3 平衡与高频 UX 修复

在稳定性问题下降后，处理明显过强/过弱、任务时间、Boss 难度、UI 误导和本地化错误。不要在 Hotfix 中加入新系统；第一批新内容应进入 v1.1.0，并先重新评估玩家反馈与开发能力。

\newpage

# 并行 Steam 发行轨道

代码路线不是唯一工作。以下发行事项与开发并行进行，越早开始越安全。

## S0 - 现在立即开始

- 注册 Steamworks、完成数字文书、银行/税务/身份核验并为应用支付 Steam Direct 费用。
- Steam 官方当前要求每个新应用支付 100 美元费用；支付后还存在至少 30 天等待期，因此不要等游戏完成才开始。
- 确认发行主体、商店发行名称、版权归属、支持邮箱和资产许可档案。
- 创建 AppID、基础 Depot 与内部测试分支。

## S1 - v0.5-v0.8 期间

- 建立 Coming Soon 商店页草稿、胶囊图方向、描述和第一批真实截图。
- 早期确定游戏名称、Logo 和视觉识别，但不改 UE 工程模块名。
- 商店功能标签只能选择实际确定会完成的功能。
- 准备 Steam Playtest 子应用和测试者管理流程。

## S2 - 美术方向稳定后尽早公开 Coming Soon

- 新产品在正式发售前，Coming Soon 页面至少需要公开两周；实际项目应更早公开以积累愿望单和验证定位。
- 定期更新真实开发进度，不承诺未排入 1.0 的功能。

## S3 - v0.11-v0.12

- 完成真实 Steam 联机、Playtest、云存档、成就和 SteamPipe。
- 使用 Playtest 的独立子 AppID 做低风险测试，不把测试评价混入正式产品。
- 使用 internal、qa、playtest、release-candidate 分支，而不是直接覆盖公开默认分支。

## S4 - v0.13-v0.15

- 完成最终商店页、预告片、截图、定价、内容调查、语言和系统需求。
- Valve 的商店页与产品构建审核通常需要 3-5 个工作日；官方建议至少提前 7 个工作日提交。
- 提交审核的 Build 应接近最终版，能在所有声称支持的系统上启动，且所有商店声称的功能已实现。

## S5 - 发布后

- 保留至少一个已知稳定 Build 供回滚。
- Hotfix 先推 internal/qa，再推 default。
- 维护 Known Issues、补丁说明、支持渠道和存档兼容。

# 版本通用 Definition of Done

每个版本只有同时满足以下条件才算完成：

```text
[ ] 当前分支从干净状态可完整编译。
[ ] UE 编辑器能打开，默认地图能运行。
[ ] 单人 Smoke Test 通过。
[ ] 涉及网络时，2 人 Listen Server 通过；里程碑版本增加 4 人/打包测试。
[ ] 没有新的 Error Log、确保失败、未处理异步 Delegate 或重复 RPC。
[ ] 数据和存档变更有版本/迁移策略。
[ ] 修改文件、UE 手工步骤、测试结果和已知问题已记录。
[ ] Git commit 清晰，并在大里程碑创建 Tag。
```

# 发布门槛摘要

| Gate | 对应版本 | 必须通过 |
|---|---|---|
| Vertical Slice | v0.4.8 | 可重复多人闭环、Development 打包、无阻断 |
| Production Foundation | v0.5.6 | 数据驱动、版本存档、修船、调试和自动测试 |
| Core Feature Complete | v0.8.6 | 战斗、四职能、背包经济、任务、敌人与 Boss 框架 |
| Campaign Playable | v0.9.6 | 新档到结局完整可玩 |
| Steam Platform Complete | v0.11.5 | Steam 联机、云、成就、分支和上传链路 |
| Alpha Exit | v0.12.5 | 外部 Playtest、Blocker/Critical 清零 |
| Content Complete | v0.13.0 | 首发内容与 ID 锁定 |
| Beta Exit | v0.13.5 | 内容、平衡、本地化、存档兼容通过 |
| RC Gate | v0.14.5 | 性能、兼容、Shipping 全回归通过 |
| Gold Master | v0.15.5 | 审核通过、可发布、可回滚 |

# 可裁剪项与不可裁剪项

开发周期不设上限，但仍需控制范围。遇到产能不足时，优先延后以下内容到 v1.1：

- 侦察职能的复杂抓钩，可改为简单相位冲刺。
- 个别任务修正和词条数量。
- 第五章以外的额外地图、额外 Boss 和 New Game+ 深度。
- 高级外观自定义、排行榜和复杂统计页面。
- 更复杂的 Procedural Generation。

以下内容不能为了赶发布而裁掉：

- 单人可通关和 1-4 人稳定 Session。
- 服务端权威伤害、拾取、装备、任务和结算。
- 版本化存档、备份、迁移和云冲突处理。
- 大厅 -> 副本 -> 撤离 -> 结算 -> 修船的完整闭环。
- 设置、手柄、字幕、简中/英文、退出与错误处理。
- Shipping 构建、Steam 安装、审核、回滚和支持流程。

# 官方参考资料（截至 2026-07-10）

- Steamworks - Onboarding: <https://partner.steamgames.com/doc/gettingstarted/onboarding>
- Steam Direct Fee: <https://partner.steamgames.com/doc/gettingstarted/appfee>
- Steam Review Process: <https://partner.steamgames.com/doc/store/review_process>
- Coming Soon: <https://partner.steamgames.com/doc/store/coming_soon>
- Steam Playtest: <https://partner.steamgames.com/doc/features/playtest>
- SteamPipe Uploading: <https://partner.steamgames.com/doc/sdk/uploading>
- Unreal Engine 5.8 - Online Subsystem Steam: <https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine>
- Unreal Engine 5.8 - Packaging Projects: <https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-your-project>
- Unreal Engine - Gameplay Ability System: <https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-ability-system-for-unreal-engine>

# 最终执行建议

当前正确起点是 **v0.4.7 已完成基线**。不要回头重做 v0.1.0-v0.4.7，也不要让 Codex 从旧文档中再次创建任务目标、刷怪器、基础敌人、撤离点、结算或旅行闭环。

现在应按以下顺序开始：

```text
1. 给当前稳定提交打 baseline-v0.4.7 标签。
2. 执行一次单人和 2 人基线回归，记录现有类名、地图名、存档格式和已知问题。
3. 只实现 v0.4.8：稳定化、幂等结算、生命周期清理、打包和测试记录。
4. v0.4.8 通过后，再进入 v0.5.0 的长期生产架构。
5. 以后每次只把一个小版本交给 Codex，编译、PIE、打包或联机验收通过后再提交。
```

这样既最大限度保护你已经完成的 v0.4.7 代码，也能让后续开发从“可玩闭环”稳步升级为真正可在 Steam 销售、更新和维护的完整游戏。
