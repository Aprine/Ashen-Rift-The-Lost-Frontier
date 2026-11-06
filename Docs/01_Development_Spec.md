# 开发规格

进度更新：0.9 已在现有玩法与 0.8 战斗反馈基础上整合组员 Kuangxin 的 KayKit 地牢、可编辑战斗房间与营地；默认地图为 `L_FrontierDungeon`，原灰盒保留。入口见 [地图整合记录](15_Team_Map_Integration.md)，战斗表现与机制细节见 08–12。本文件的里程碑表和蓝图表保留早期总体设计；动作与命中同步、最终美术、混音及最终打包仍待完成。

## 已确认与暂定

用户确认 UE4、单人、暗黑中世纪主题、动作 RPG、Roguelite、撤离风险机制，并已选择 C++ 核心 + 蓝图。原 proposal 的 Blueprint-based gameplay 描述应同步修改。采用本机已安装 UE4.27.2，第三人称视角为当前实现选择，可在投入战斗开发前调整。

以下数值均为首轮测试建议，不是 proposal 已确定的设定。

## 里程碑

| 阶段 | 可交付行为 | 通过条件 |
|---|---|---|
| M0 当前底座 | 角色通过四个灰盒区 | 地图可加载、角色动画依赖齐全、蓝图可编译；移动另做 PIE 验收 |
| M1 首个闭环 | 战士击杀骷髅、捡金币与药水、死亡或撤离、返回主城 | 一次成功撤离和一次失败死亡均能正确结算 |
| M2 内容扩展 | 背包、商店、法师、天赋与存档 | 购买和装备有事务校验，重启后永久成长保留 |
| M3 副本挑战 | 4–6 个预制房间、2–3 种敌人、1 个 Boss | 同一种子可复现地图，Boss 可正常击败，撤离可提前发生 |
| M4 课程交付 | UI、音效、风格资产、报告和演示 | Windows 打包版本完整跑通，关键失败路径通过 |

不要等到第六周才第一次测试死亡与撤离。第二个职业和更多内容在 M1 通过后制作。原 proposal 第三周做随机房间与用户中文计划第六周做随机房间存在排期差异；采用先闭环再随机房间的依赖顺序。

## 蓝图职责

| 资源 | 父类 | 数据与职责 |
|---|---|---|
| BP_ARGameInstance | GameInstance | 跨关卡保存当前 Run、结算快照和已加载的永久档案 |
| BP_ARGameMode | GameModeBase | 当前世界开始、失败与成功结算入口；每次换图会重建 |
| BP_ARPlayer | Character | 输入、移动、攻击与交互；当前已有第三人称副本 |
| BPC_Health | ActorComponent | MaxHealth、CurrentHealth、bDead；统一受伤和死亡事件 |
| BPC_Inventory | ActorComponent | 容量、堆叠、增减物品、装备操作；不直接写永久档 |
| BP_EnemyBase | Character | 攻击状态、掉落配置、BPC_Health |
| BP_ARAIController | AIController | 导航、追击与攻击距离决策 |
| BP_Loot | Actor | 物品 ID、数量；背包成功接收后才销毁 |
| BP_ExtractionPortal | Actor | 区域判定、交互、延时取消、单次撤离请求 |
| BP_Shop | Actor | 商品列表与购买入口，不直接修改 UI 数字 |
| SG_ARProfile | SaveGame | SaveVersion、BankGold、BankRunes、仓库、天赋等级 |
| WBP_HUD / WBP_Result | UserWidget | 显示数据、发送操作请求；不拥有结算业务 |

表中 BP_ARGameMode、BP_ARPlayer 已创建。Health、Inventory、GameInstance、SaveGame 已改为对应 C++ 类型，源码和接线以 04_CPP_Blueprint_Integration 为准，其他蓝图仍待创建。组件通过 Event Dispatcher 更新 UI，交互使用 BPI_Interactable。原生 Health 自动监听 AnyDamage，蓝图不要二次转发伤害。

## 数据约定

`ST_ItemStack`：ItemID（Name）、Quantity（Integer）。M1 仅用 Gold、Potion，后续再引入装备实例 ID 和随机属性。

`ST_ItemDefinition`：ItemID、DisplayName、Type、MaxStack、BuyPrice、SellPrice、Power、Icon（Soft Object Reference）。建议 DataTable 集中管理。金币先放在 RunWallet，不占背包格；药水占格。

`ST_RunState`：RunID（GUID）、Seed、Phase、RunGold、RunRunes、RunInventory、bSettled。`ST_RunSummary` 另存本次获得、带出、损失和失败原因，避免清空 Run 后结算界面全为零。

`E_RunPhase`：Hub → InDungeon → Extracting → Settling → Result → Hub。死亡可从 InDungeon 或 Extracting 进入 Settling。进入 Settling 后拒绝拾取、购买、伤害、重复撤离。

## M1 经济与死亡规则

- StarterSword 为职业自带物品，不进入本局战利品列表，下一局重新发放。
- 本局拾取的金币、药水、装备、未带出的符文，死亡全部损失。
- 已存银行金币、已存符文、未携带的仓库物品和已购买永久天赋，死亡不改变。0.5 的携带物品在出发时从仓库扣除并存盘，进入副本后承担死亡损失。
- 撤离成功后将 RunGold、RunRunes 与 RunInventory 转入永久档案，保存成功后清空本局。
- 暂不做尸体回收。中文描述“大部分掉落”在 M1 中明确为全部本局战利品损失，避免实现比例不一致；之后可扩展保底。
- M1 未开放仓库带入；0.5 已增加携带准备：先检查数量、职业和容量，再保存扣除候选，成功后进入副本；撤离将剩余携带物品随战利品一起存回，不设保险。

`SettleRun` 只有一个入口。先检查 bSettled 与 Phase，再锁定状态并拍摄 Summary 快照。成功撤离：构造候选 Profile → Save Game To Slot → 检查返回值 → 成功后提交内存 Profile 并显示 Result。保存失败保留 Summary 与待提交候选，允许重试，不能重复叠加银行金额。死亡不改永久 Profile，清空临时战利品并显示损失。

程序强退时不恢复进行中的副本，已完成永久存档保留；启动只加载已提交 Profile。不同 SaveVersion 必须显式兼容或提示备份重置，禁止静默覆盖旧档。

## 随机房间方案

最终版使用预制房间插槽与同规格门口。固定起点、撤离点与 Boss 房间，战斗房从房间池用 Random Stream 抽取；只在主城确定 Seed，副本内不能反复初始化流。第一轮采用固定直线路径，后续可用关卡流送或房间 Actor。随机化阶段须保证出生到撤离、出生到 Boss 均连通，导航覆盖所有战斗地面。

## 建议测试数值

战士 HP 100，近战伤害 25，间隔 0.6 秒，距离 180 cm；骷髅 HP 60，伤害 10，攻击间隔 1.2 秒，攻击距离 150 cm，追击半径 1000 cm；每只掉落 10 Gold；药水回复 30 HP，满血不消耗。撤离读条 3 秒，离开区域或受伤取消。首轮一个 Boss 以后再加入。

## 分工接口

成员 1 拥有 GameInstance、GameMode、存档与结算；成员 2 拥有关卡与房间；成员 3 拥有玩家与攻击；成员 4 拥有物品、Inventory、商店；成员 5 拥有敌人、AI 与 Boss；成员 6 拥有 Widget、音效和验收记录。Health 组件建议由成员 3 维护，成员 5 调用。UI 由成员 6 编辑，成员 4 提供数据和事件，解决原分工中 Inventory UI 归属重叠。
