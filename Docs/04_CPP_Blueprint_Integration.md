# C++ 核心与蓝图集成

历史记录：0.2 已自动集成玩家、敌人、HUD 与撤离逻辑。当前入口为 `06_First_Playable.md`；本文件中“尚未接线”和手工添加组件步骤描述的是 0.1 状态。

用户已确认使用 C++ 核心 + 蓝图，优先于原 proposal 中的纯蓝图描述。同步课程 proposal 时，将实现路线改成“C++ 实现状态与数据规则，Blueprint 配置和组合角色、敌人、交互与 UI”。

## 已写入的源码

| C++ 类型 | 蓝图用途 |
|---|---|
| ARHealthComponent | Add Component 搜索 ARHealth，自动监听宿主 Actor 的 AnyDamage；提供 ReceiveDamage、Heal、ResetHealth 和死亡/受伤 Dispatcher |
| ARInventoryComponent | Add Component 搜索 ARInventory；TryAdd、TryRemove、Count、Clear；12 个不同 ID 槽位，每 ID 上限 99 |
| ARRunGameInstance | Get Game Instance 后 Cast；BeginRun、AddCurrency、BeginExtraction、CancelExtraction、SettleRun、RetrySave、ReturnToHub |
| ARProfile | SaveGame 对象；银行、符文、仓库、天赋等级字段；现阶段只实现结算存档，没有天赋购买逻辑 |

编辑器模块与游戏目标已在 2026-10-28 编译通过，Health 和 Inventory 的两项自动化测试通过。仍须在编辑器接线和进行 Play 验收。当前角色副本仍是模板角色，**没有自动添加 Health/Inventory 组件**；尚未完成战斗、敌人、拾取与撤离接线。

## 首次编译

UE4.27 官方默认工具链为 Visual Studio 2019。安装或使用含 MSVC v142 与 Windows 10 SDK 的兼容环境。只安装较新 MSVC 不保证 UE4.27 可以识别。

本机已经安装：VS2019 Build Tools 16.11.37823.30，路径 D:/Development/VS2019BuildTools；MSVC 工具目录 14.29.30133（UE4 报告工具链版本 14.29.30159）；Windows SDK 10.0.19041.0；.NET Framework 4.8 SDK 和 Targeting Pack。两个 Target.cs 均固定 VS2019 与 SDK 19041。安装器退出码 0，vswhere 报告安装完整、无需重启，原 VS2026 Build Tools 保留。

在 PowerShell 中运行：

```powershell
& 'D:\Epic Games\UE_4.27\Engine\Build\BatchFiles\Build.bat' AshenRiftEditor Win64 Development 'C:\Users\Administrator\Desktop\4900PJ\AshenRift\AshenRift.uproject' -WaitMutex -NoHotReloadFromIDE
```

编译成功后打开 uproject。修改头文件、UPROPERTY 或 UFUNCTION 后，建议退出编辑器重新编译，避免热重载对象混用。

## 替换蓝图施工表中的重复实现

`02_First_Playable_Blueprints.md` 是初始纯蓝图路线的节点说明。选定混合路线后按本文件替换数据部分，保留攻击判定、AI、地图、交互与 UI 步骤：

1. 不再创建蓝图 BPC_Health、BPC_Inventory、BP_ARGameInstance 和 SG_ARProfile。在角色和敌人上添加 C++ ARHealth；玩家再添加 ARInventory。
2. **不要再用蓝图 Event AnyDamage 转发 ReceiveDamage**。组件自动绑定 OnTakeAnyDamage，重复转发会使伤害翻倍。
3. 读血条用 Health.CurrentHealth / MaxHealth，绑定 OnHealthChanged；死亡绑定 OnDied。Q 用药先检查 Inventory.Count(Potion)>0，再 Heal(30)，实际回复量>0 后 TryRemove(Potion,1)。
4. BeginRun(Seed) 只能在 Hub 调用，返回 true 后才开始副本。金币拾取调用 AddCurrency(10,0)，返回 true 后销毁金币；其他物品 TryAdd 成功后销毁。
5. Portal 交互调用 BeginExtraction，成功才启动读条。离开和受伤调用 CancelExtraction 并取消 Timer；Timer 完成调用 SettleRun(true, 玩家 Inventory)。死亡调用 SettleRun(false, 玩家 Inventory)。
6. SettleRun 返回 false 时读取 Phase 和 LastError：Settling 表示保存待处理，禁止换关卡，显示重试按钮调用 RetrySave；不要再次调用 SettleRun。
7. Phase=Result 时显示 Summary；成功 Loot 是带出列表，失败 Loot 是损失列表。点击返回先 ReturnToHub，再重生或换主城关卡。
8. 游戏模式配置已指向原生 ARRunGameInstance。UI 使用 OnChanged 刷新，不再在蓝图复制一套 RunGold 或 BankGold。

## 边界与后续工作

- 组件库存是 M1 简化模型，不支持装备唯一实例、每种物品不同堆叠上限、排序和装备槽。上述功能需要后续扩展。
- 本局货币保存在 GameInstance，物品保存在玩家组件。**首轮副本全程使用同一张地图**；接入跨地图或更换 Pawn 前，必须把 Inventory 快照迁入 GameInstance 并在新 Pawn 恢复，不能直接 Open Level 丢掉物品。
- 死亡清空临时物品，成功存档后清空物品；永久字段不因死亡改变。SaveGame 同步保存只适合课程小档案，数据增大后改异步并保留单次提交状态。
- Profile 加载失败时 BeginRun 拒绝启动，UI 显示 LastError；不覆盖失败档案。存档数值溢出也会保持 Settling，需要显式维护处理，普通保存重试无法解决溢出。
- 蓝图必须根据 Phase 拒绝结算后的输入、购买与伤害，Health 组件本身不访问 GameInstance，方便敌人与玩家复用。
- 此版本没有商店交易、天赋购买或装备系统。已有 Health 与 Inventory 核心自动化测试；副本结算与存档失败重试尚未运行测试。
