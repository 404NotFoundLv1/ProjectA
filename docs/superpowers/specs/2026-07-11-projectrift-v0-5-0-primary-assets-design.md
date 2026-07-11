# ProjectRift v0.5.0 Primary DataAsset 与 AssetManager 基础设计

日期：2026-07-11  
状态：已获用户逐段批准，等待书面规格复核  
版本边界：仅实现 v0.5.0，不规划或启动后续小版本

## 目标

以 UE 5.8 的 `UAssetManager` 与 `UPrimaryDataAsset` 建立 ProjectRift 的统一运行时资产发现、同步加载、异步加载和 Cook 基线。本版本只纳管已有的物品数据与掉落表，不新增任务、敌人或技能资产类型。

## 现状

- `UPRItemDataAsset` 已继承 `UPrimaryDataAsset`，Primary Asset 类型为 `ProjectRiftItem`，名称来自业务 `ItemId`。
- `UPRLootTableDataAsset` 已继承 `UPrimaryDataAsset`，Primary Asset 类型为 `ProjectRiftLootTable`，名称来自资产名称。
- `UPRInventoryComponent::FindItemData` 仍通过 `/Game/ProjectRift/Items/DA_<ItemId>` 硬编码路径同步加载。
- v0.4.8 使用 `DirectoriesToAlwaysCook=/Game/ProjectRift/Items` 保证运行时物品数据进入包体，尚未由 AssetManager 接管。

## 选定方案

新增项目全局 `UPRAssetManager : UAssetManager`。该类是 ProjectRift Primary Asset 的唯一类型化入口，负责构造 ID、查询、同步加载、异步加载及一致的错误日志。Inventory 和后续系统只使用业务 ID 或 PrimaryAssetId，不再了解磁盘路径。

没有选用静态工具库，因为它会让扫描、类型校验和错误处理继续分散；没有选用 `UGameInstanceSubsystem`，因为资产发现与 Cook 不应依赖 World 或 GameInstance 生命周期。

## 架构与配置

### 全局 AssetManager

- 新增 `UPRAssetManager`，通过 `DefaultEngine.ini` 中的 `AssetManagerClassName=/Script/ProjectA.PRAssetManager` 设为项目 AssetManager。
- 提供返回可空指针的 `static UPRAssetManager* Get()` 入口；若项目未使用该类型，记录错误并返回 `nullptr`，不执行不安全的强制转换。
- 覆盖 `StartInitialLoading()`：先调用父类，再验证两种 Primary Asset 类型已经注册并记录扫描数量；类型缺失或扫描结果为空时记录警告，但不阻止编辑器启动。
- 复用 UE 原生 Asset Registry、Primary Asset 扫描和 Streamable Handle，不建立第二套注册表或缓存。

### Primary Asset 类型

在 `DefaultGame.ini` 的 `/Script/Engine.AssetManagerSettings` 中配置：

- `ProjectRiftItem`
  - Base Class：`/Script/ProjectA.PRItemDataAsset`
  - Directory：`/Game/ProjectRift/Items`
  - Blueprint Classes：否
  - Editor Only：否
  - Cook Rule：`AlwaysCook`
- `ProjectRiftLootTable`
  - Base Class：`/Script/ProjectA.PRLootTableDataAsset`
  - Directory：`/Game/ProjectRift/Items`
  - Blueprint Classes：否
  - Editor Only：否
  - Cook Rule：`AlwaysCook`

删除 ProjectPackagingSettings 中的 `DirectoriesToAlwaysCook=/Game/ProjectRift/Items`。三张地图的显式 Cook 列表继续保留。此变更要求最终打包证明资产确实由 AssetManager 规则进入 Cook，而不是依赖旧兜底。

## 接口设计

`UPRAssetManager` 提供以下明确的 C++ 接口；异步接口保持为原生 C++，本版本不增加 Blueprint Async Action 节点：

```cpp
DECLARE_DELEGATE_OneParam(FPRItemDataLoadComplete, UPRItemDataAsset*);
DECLARE_DELEGATE_OneParam(FPRLootTableLoadComplete, UPRLootTableDataAsset*);

static UPRAssetManager* Get();
static FPrimaryAssetId MakeItemPrimaryAssetId(FName ItemId);
static FPrimaryAssetId MakeLootTablePrimaryAssetId(FName AssetName);

UPRItemDataAsset* GetLoadedItemData(FName ItemId) const;
UPRLootTableDataAsset* GetLoadedLootTable(FName AssetName) const;
UPRItemDataAsset* LoadItemDataSync(FName ItemId);
UPRLootTableDataAsset* LoadLootTableSync(FName AssetName);

TSharedPtr<FStreamableHandle> LoadItemDataAsync(
    FName ItemId,
    FPRItemDataLoadComplete Completion);
TSharedPtr<FStreamableHandle> LoadLootTableAsync(
    FName AssetName,
    FPRLootTableLoadComplete Completion);
```

空 ID 时异步完成委托在当前调用中执行一次并收到 `nullptr`，返回空 Handle。有效但尚未加载的 ID 通过 UE `LoadPrimaryAsset` 返回有效 Handle；完成后重新按同一 ID 获取并校验对象类型。

同步与异步实现必须共用 PrimaryAssetId 和 UE AssetManager 路径解析，不允许回退到字符串拼接的对象路径。

## 数据流

### Inventory 同步查找

1. `UPRInventoryComponent::FindItemData` 先检查显式注入的 `ItemDataAssets`，保留现有测试和覆盖能力。
2. 未命中时把 `ItemId` 交给 `UPRAssetManager`。
3. AssetManager 构造 `ProjectRiftItem:<ItemId>`，先检查已加载对象。
4. 未加载时解析该 PrimaryAssetId 的软对象路径并同步加载。
5. 成功返回 `UPRItemDataAsset*`；失败返回 `nullptr`。
6. Inventory 保留现有兼容行为：缺失数据时使用无限堆叠上限，不崩溃、不伪造资产。

### 异步加载

1. 调用者提交 ItemId 或 LootTable 名称。
2. `UPRAssetManager` 验证 ID 并调用 UE `LoadPrimaryAsset`。
3. 返回原生 `FStreamableHandle`，由调用者决定持有周期或取消请求。
4. 完成时按相同 PrimaryAssetId 取得类型安全对象并调用完成委托。
5. 加载失败时完成委托仍被调用，但对象为 `nullptr`。

## 标识规则

- Item：`ProjectRiftItem:HealthInjector`、`ProjectRiftItem:ShieldPack`、`ProjectRiftItem:EnergyCrystal`、`ProjectRiftItem:CommonChip`。
- LootTable：`ProjectRiftLootTable:DA_TestLootTable`。
- 空 ItemId 或资产名生成无效 PrimaryAssetId。
- 本版本不提供别名、重命名迁移、模糊搜索或按文件名猜测 ID。
- 不重命名现有 C++ 类、模块、资产、地图或 Blueprint。

## 错误处理与日志

- 空 ID：直接返回 `nullptr`；异步接口返回空 Handle，并以 `nullptr` 完成回调。
- AssetManager 未注册该 ID：记录包含 PrimaryAssetId 的 `LogProjectA` 警告并返回 `nullptr`。
- 注册对象类型不匹配：记录期望类型与实际对象路径，返回 `nullptr`。
- 同步加载失败：记录 PrimaryAssetId 与解析到的软路径，不回退硬编码路径。
- 异步加载失败或 Handle 无效：保证完成回调最多调用一次且结果为 `nullptr`。
- 错误不得导致客户端获得权限行为，也不得改变现有 Inventory 复制模型。

## 测试策略

所有新行为遵循 RED→GREEN：先写会因缺失类型、配置或行为而失败的测试，再实现最小代码。

### 自动化测试

- 项目实际的全局 AssetManager 类型是 `UPRAssetManager`。
- AssetManagerSettings 注册 `ProjectRiftItem` 与 `ProjectRiftLootTable`，两者均为 `AlwaysCook`。
- 旧 `DirectoriesToAlwaysCook=/Game/ProjectRift/Items` 已移除。
- 四个 Item DataAsset 均能按标准 PrimaryAssetId 被发现并同步加载。
- `DA_TestLootTable` 能按标准 PrimaryAssetId 被发现并同步加载。
- 空、未知及类型错误的 ID 安全返回 `nullptr`。
- Item 与 LootTable 异步加载返回有效 Handle，并在完成后提供正确类型对象。
- Inventory 未注入资产时通过 AssetManager 解析；注入资产仍优先于全局查询。
- 所有既有 ProjectRift 测试继续通过。

### MCP 资产验证

- 只使用 ProjectA MCP `http://127.0.0.1:8001/mcp`。
- 检查四个 Item DataAsset 与 `DA_TestLootTable` 的类型、路径和 PrimaryAssetId。
- 编译受原生类变更影响的 Blueprint，警告视为错误。
- 不保存未修改资产；最终应无脏 ProjectRift 资产与重定向器。

### 构建与打包

- 完整 ProjectAEditor Win64 Development 编译。
- 全量 ProjectRift 自动化测试。
- Win64 Development Build/Cook/Stage/Pak/Archive，AutomationTool 必须返回 0。
- 从打包版启动 `L_Rift_Test`，验证 Item 与 LootTable PrimaryAssetId 可发现并加载。
- 打包日志不得出现 ProjectRift Item/LootTable 缺失、类型不匹配、Fatal 或 Error。

## 文档与交付

- 项目版本更新为 `0.5.0`。
- 更新 `CHANGELOG.md`。
- 新增 v0.5.0 测试记录，包含 RED/GREEN、构建、MCP、Cook、打包烟测与已知问题。
- 变更直接保留在现有 `main`，不暂存、不提交、不建分支。
- 完成 v0.5.0 后停止，等待用户验收和提交；没有明确命令不得开始下一版本。

## 明确不在范围内

- 新增 Mission、Enemy、Ability 或其他 Primary DataAsset 类型。
- 资产热更新、分块下载、DLC、加密、签名或远程 Asset Registry。
- 重写 Inventory 为全异步模型。
- 修改现有联网权限、复制、结算或游戏循环。
- 清理与本版本无关的目录、资产或代码。
