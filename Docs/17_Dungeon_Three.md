# 0.11 地牢三、商店与 Boss 区

2026-11-26 · Unreal Engine 4.27.2 · Windows Development

本次整合使用 Kuangxin 提交的地牢三、商店与最终版 Boss 房，并随布局导入 32 个 KayKit Dungeon 资源包，保留各自的原始包路径。[地牢三提交](https://github.com/kuangxin114514/Ashen-Rift-The-Lost-Frontier/commit/15c2120635a37e7285c373b23412fa01257c7419) · [商店与 Boss 房提交](https://github.com/kuangxin114514/Ashen-Rift-The-Lost-Frontier/commit/d8f8feeb7786576b4d124ea5dfbf2df646f7b2dd) · [Boss 房更新提交](https://github.com/kuangxin114514/Ashen-Rift-The-Lost-Frontier/commit/fa4c9a63c40b4514480f947fb323d7dd97d2ab91) · [第三方资源记录](ThirdPartyAssets.md)

## 游玩流程

1. 进入主城，在出生点附近按 **B** 打开准备面板，选择 **前往地牢三**。
2. 探索地牢并收集战利品。靠近**金色**传送点按 **E** 进入补给区，靠近商人按 **E** 交易。
3. 靠近**紫色**传送点按 **E** 进入首领区，深入竞技场挑战灰烬守望者。两个区域都可通过各自的返程点回到地牢三。
4. 在地牢三入口附近的**蓝色**撤离传送阵内按 **E**，停留 3 秒完成撤离。结算后按 **R** 回到主城。

## 编辑关卡

`Map_Dungeon3` 是主关卡。`Map_Shop` 和 `Map_Boss` 在 UE4 的 Levels 面板里作为子关卡加载，进出时共用同一个玩家和副本状态。商店或首领区返程后，生命值、背包、本局货币和商店购买状态连续保留。

主关卡中的 `ExpeditionSite` 配置出生点、敌人、补给和灰烬守望者的位置。成对的 `ARExpeditionPassage` 演员配置交互提示、目标和落点。地牢三、商店和首领区均为手工制作的路线，可在完整的 UE4.27.2 编辑器中运行脚本以重建整合摆位。[配置脚本](../Scripts/configure_dungeon_three.py)

## 验证

| 验证项目 | 结果与本地记录 |
|---|---|
| 默认回归 | 12 项测试通过，覆盖原版流程、经典模式和三张地牢。见 `Saved/test_nov26_full.log`。 |
| 三地牢专项测试 | `Integration.Expeditions` 通过。检查地牢三到达商店与 Boss 区的导航、往返后的状态连续性，以及购买、击败 Boss、撤离入库和回城。另用实际渲染复跑一次，0 失败、0 测试警告。见 `Saved/Nov26Rendered/index.json`。 |
| C++ 和蓝图 | Development Editor 与 Game 构建成功，蓝图编译 0 错误、0 警告。见 `Saved/build_nov26_game.log` 与 `Saved/compile_nov26_blueprints.log`。 |
| Windows 地图烘焙 | 主城、三张地牢、商店和 Boss 房均完成 WindowsNoEditor 烘焙，0 错误、0 警告。见 `Saved/cook_nov26.log`。正式安装包验收待进行。 |
