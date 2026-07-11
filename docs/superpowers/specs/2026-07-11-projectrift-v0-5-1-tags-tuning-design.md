# ProjectRift v0.5.1 GameplayTag 合同与项目调参设置设计

日期：2026-07-11  
状态：已获用户逐段批准，等待书面规格复核  
版本边界：仅实现 v0.5.1，不规划或启动后续小版本

## 目标

把 ProjectRift 当前 GameplayTag 从分散字符串与 INI 声明升级为编译期 Native GameplayTag 合同，并把 Vertical Slice 的任务、目标、刷怪与撤离参数集中到唯一的 `UDeveloperSettings` 项目设置中。运行时行为、联网权限、资产名称和现有公共查询接口保持兼容。

## 现状

- `Config/Tags/ProjectRiftGameplayTags.ini` 声明 15 个标签。
- Character、Ability、Enemy 和 GameplayEffect 代码在运行时多次调用 `RequestGameplayTag(TEXT("..."), false)`。
- Rift GameMode、Hold Objective、Objective、SpawnManager 和 ExtractionZone 分别持有本地可编辑参数。
- 相同合同依靠字符串拼写与各 Actor 默认值维持，编译器不能发现拼写漂移，项目调参也没有统一入口。

## 选定方案

采用两个无 World 生命周期依赖的项目级基础设施：

1. `ProjectRiftGameplayTags` Native 标签命名空间。
2. `UPRProjectSettings : UDeveloperSettings` 的 Game Config 默认对象。

所有生产消费者直接引用 Native 标签常量，并通过 `GetDefault<UPRProjectSettings>()` 读取调参。没有 GameInstanceSubsystem、额外缓存、难度 DataAsset 或局部 Actor 覆盖层。

## Native GameplayTag 合同

新增 `Source/ProjectA/Core/PRGameplayTags.h/.cpp`，使用 `UE_DECLARE_GAMEPLAY_TAG_EXTERN` 和 `UE_DEFINE_GAMEPLAY_TAG_COMMENT` 注册以下 15 个标签：

- `Input.Ability.Primary`
- `Input.Ability.Dodge`
- `Input.Ability.Skill.Q`
- `Input.Ability.Skill.E`
- `Input.Ability.Skill.R`
- `Ability.Role.Assault`
- `Ability.Role.Engineer`
- `Ability.Role.Medic`
- `Data.Damage`
- `State.Dead`
- `State.Downed`
- `State.Stunned`
- `Cooldown.Skill.Q`
- `Cooldown.Skill.E`
- `Cooldown.Skill.R`

C++ 标识符采用下划线映射，例如 `Input_Ability_Primary`、`State_Downed`、`Data_Damage`。每个常量保留当前 INI DevComment 的语义。

### 标签迁移规则

- 生产代码不再调用 `RequestGameplayTag` 获取 ProjectRift 标签。
- `FSetByCallerFloat` 的伤害键只使用 `ProjectRiftGameplayTags::Data_Damage`，不再保留字符串 `DataName`。
- 测试可以通过字符串反查 GameplayTagsManager，以证明 Native 常量和公开名字一致。
- 删除 `Config/Tags/ProjectRiftGameplayTags.ini` 中重复的 15 个声明，并删除 `DefaultGame.ini` 对该文件的 Staging 白名单。
- 本版本不增加、改名或重定向任何标签，也不迁移 GameplayTag 到 DataAsset。

## ProjectRift DeveloperSettings

新增 `Source/ProjectA/Settings/PRProjectSettings.h/.cpp`：

```cpp
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "ProjectRift Gameplay"))
class PROJECTA_API UPRProjectSettings : public UDeveloperSettings
```

设置显示在 Project Settings 的 ProjectRift/Gameplay 区域，并从 `DefaultGame.ini` 的 `/Script/ProjectA.PRProjectSettings` 节读取。

### 唯一纳管的 11 个参数

| 分类 | 属性 | 默认值 | 范围 |
|---|---|---:|---:|
| Mission | `InitialRiftStability` | 100.0 | 0–100 |
| Mission | `RiftStabilityDrainPerSecond` | 1.0 | ≥0 |
| Mission | `FailedResourceRetentionRate` | 0.5 | 0–1 |
| Mission | `ReturnToLobbyDelayAfterSettlement` | 4.0 | ≥0 |
| Objective | `ObjectiveHoldDuration` | 30.0 | ≥0.1 |
| Objective | `ObjectiveInteractionRadius` | 250.0 | ≥1.0 |
| Spawning | `BaseEnemiesPerWave` | 2 | ≥0 |
| Spawning | `EnemiesPerAdditionalPlayer` | 1 | ≥0 |
| Spawning | `MaxAliveEnemies` | 8 | ≥1 |
| Spawning | `WaveInterval` | 6.0 | ≥0.1 |
| Extraction | `ExtractionRadius` | 320.0 | ≥1.0 |

每个属性使用 `UPROPERTY(Config, EditAnywhere, BlueprintReadOnly)`、明确 Category、ClampMin/ClampMax 和 UI 限制。`DefaultGame.ini` 显式保存全部默认值，避免 C++ 默认与项目配置含义不清。

### 不纳管的值

- `ReturnToLobbyMapPath`
- `MissionId`
- `SpawnedEnemyClass`
- 资产、地图、输入映射、技能伤害和冷却
- 任何后续版本的难度预设或远程配置

## 运行时数据流

### Rift GameMode

- `StartRiftMission` 使用设置的初始稳定度。
- `DrainRiftStability` 使用设置的每秒消耗。
- `CalculateRetainedResourceCount` 使用设置的失败保留率。
- 返回大厅的 getter、日志和 Timer 使用设置的结算后延迟。
- 删除这四个本地 EditDefaultsOnly 数值属性，保留公共查询接口。

### Objective

- `APRRiftObjectiveActor::GetInteractionRadius` 返回项目设置值。
- 构造、交互检测和提示范围使用同一个 getter。
- `APRHoldObjectiveActor::GetHoldDuration` 返回项目设置值；Tick、进度和测试都使用该 getter。
- 删除本地 `InteractionRadius` 和 `HoldDuration` 属性。

### SpawnManager

- 波次间隔、每波数量、每玩家追加数量和最大存活数在每次计算/启动时读取项目设置。
- 删除四个局部 EditAnywhere 属性。
- Enemy Class、运行状态、计数器和生成点保持原样。

### ExtractionZone

- `GetExtractionRadius` 返回项目设置值。
- 构造、OnConstruction、标记缩放、检测范围和提示范围统一调用 getter。
- 删除本地 `ExtractionRadius` 属性。

## 兼容性与错误处理

- `GetDefault<UPRProjectSettings>()` 是唯一配置来源；项目模块加载后该 CDO 必须存在。
- 每个消费者在读取后仍执行运行时安全 Clamp，防止手工编辑 INI 绕过编辑器元数据。
- 如果设置 CDO 意外不可用，记录 `LogProjectA` Error 并使用上表中的代码默认值，不读取已删除的 Actor 属性。
- 公共 Blueprint getter 保持名称与返回类型不变。
- 旧 Blueprint/地图中的继承属性序列化数据不再控制运行时；MCP 编译必须确认没有断图或脏资产。
- 不改变服务端权威、复制、结算幂等、Inventory 或 AssetManager 行为。

## 测试策略

所有新增或迁移行为遵循 RED→GREEN。

### GameplayTag 合同测试

- 15 个 Native 常量全部有效。
- 每个常量的 `ToString()` 与合同名称完全一致。
- GameplayTagsManager 按相同字符串反查得到同一标签。
- 项目生产源码不再包含 `RequestGameplayTag(TEXT("` 的 ProjectRift 字符串请求。
- 旧 INI 标签声明与 Staging 白名单已删除。

### DeveloperSettings 合同测试

- `UPRProjectSettings` 继承 `UDeveloperSettings`，使用 Game Config，并暴露 11 个 Config 属性。
- 默认值和 Clamp 元数据与上表一致。
- `DefaultGame.ini` 显式包含 11 个默认值与 `ProjectVersion=0.5.1`。

### 消费者行为测试

- 作用域保护临时修改 Mutable Default，并在测试结束时恢复。
- GameMode 使用配置的初始稳定度、消耗率、失败保留率和返回延迟。
- Hold Objective 使用配置的持续时间，Objective 使用配置的交互半径。
- SpawnManager 使用配置的数量、上限与波次间隔。
- ExtractionZone 使用配置的撤离半径。
- 所有既有 ProjectRift 测试迁移后继续通过，不依赖已删除属性反射。

## MCP 与构建验证

- 只使用 ProjectA MCP `http://127.0.0.1:8001/mcp`。
- 编译受标签与属性迁移影响的 Character、Ability、Enemy、GameMode、Objective、SpawnManager、Extraction 和 UI Blueprint，警告视为错误。
- 检查三张地图和相关资产没有断图、脏资产或重定向器。
- 完整执行 ProjectAEditor/ProjectA Win64 Development 编译和全部 ProjectRift 自动化测试。
- 完整执行 Win64 Development Build/Cook/Stage/Pak/Archive。
- 打包版启动 `L_Rift_Test`，日志确认 15 个 Native 标签有效、11 个项目设置加载，并且没有缺失标签、配置、Fatal 或 Error。

## 文档与交付

- 项目版本更新为 `0.5.1`。
- 更新 `CHANGELOG.md` 和 `docs/projectrift/known-issues.md`。
- 新增 `docs/projectrift/v0.5.1-test-record.md`，记录 RED/GREEN、标签扫描、设置值、MCP、构建、打包与烟测证据。
- 变更直接保留在现有 `main`，不暂存、不提交、不建分支。
- 完成 v0.5.1 后停止，等待用户验收和提交；没有明确命令不得开始下一版本。

## 明确不在范围内

- 新增 GameplayTag、改名、重定向或兼容别名。
- GAS 技能伤害、能量消耗、冷却、攻击范围等战斗调参。
- 多难度配置、DataAsset 调参集、在线热更新或控制台远程配置。
- UI 设置页或运行时玩家可编辑调参。
- 修改联网权限、复制模型、游戏循环或 v0.5.0 AssetManager。
