# ProjectRift v0.5.2 调试 UI 分区布局设计

## 目标与范围

本修正只处理 v0.5.2 验收界面的可读性，不改变保存逻辑、正式 UI 或下一版本内容。开发版把三类调试信息分配到互不重叠的屏幕区域；Shipping 行为保持不变。

## 采用方案

- 左上：保留 GAS 状态面板，使用既有半透明深色底板和固定安全边距。
- 右上：档案验收面板使用右上锚点、固定宽度、深色底板、清晰字号和纵向操作分组；档案列表放入限高滚动区。
- 左下：把大厅 Ready 文本从不可定位的引擎屏幕消息改为纯 C++ 原生调试 Widget，使用左下锚点和深色底板。
- 中央区域继续留给背包、结算等交互式界面，不改变其现有锚点和输入模式。

未采用的方案包括：在主菜单隐藏 GAS/Ready 信息（会损失现有验收信息），以及把所有调试内容合并成一个总面板（耦合更高且仍容易拥挤）。

## 组件与生命周期

- `UPRProfileDebugWidget` 只负责档案内容排版；`UPRGameInstance` 负责将其锚定到右上角。
- 新增轻量 `UPRLobbyReadyDebugWidget`，只显示由 `APRPlayerController` 提供的 Ready 文本。
- `APRPlayerController` 创建、刷新和销毁 GAS/Ready 两个调试 Widget。离开大厅时 Ready Widget 隐藏或移除，不遗留旧文本。
- 所有调试 Widget 继续使用原生 C++，不新增 Blueprint 或资产依赖。

## 可读性与错误处理

- 统一使用 14–18pt 字号、半透明黑色底板、12–20px 内边距和 24px 屏幕安全边距。
- 档案 GUID 允许自动换行；操作按钮分为两行，避免横向挤压。
- PlayerState/GameState 暂不可用时显示等待状态，不崩溃、不保留过期 Ready 列表。

## 验证

- 先增加布局合同测试，验证三个区域的锚点、对齐和位置互不重叠，并观察其在实现前失败。
- 重新构建 `ProjectAEditor Win64 Development`。
- 运行新增/相关 `ProjectRift` 自动化回归。
- 仅通过 ProjectA 8001 MCP 在 `L_MainMenu` PIE 截图复核右上档案、左上 GAS、左下 Ready，检查日志后恢复 `L_ShipLobby`。

