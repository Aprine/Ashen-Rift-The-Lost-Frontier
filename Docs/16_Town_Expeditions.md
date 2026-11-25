# 0.10 主城与双地牢整合

2026-11-25 · Unreal Engine 4.27.2 · Windows Development

Kuangxin 在 0.9 地图之后新增了 [主城 `Map_Hub`](https://github.com/kuangxin114514/Ashen-Rift-The-Lost-Frontier/commit/504548eab670760d630f93ac4df2280e31786df3) 和 [地牢 `Map_Dungeon1`、`Map_Dungeon2`](https://github.com/kuangxin114514/Ashen-Rift-The-Lost-Frontier/commit/28409168f8af23470a03b9a3b0da41e12dc404e6)。三张地图分别随其原包路径下的 104 个环境依赖导入。来源和资产署名见 [第三方资源记录](ThirdPartyAssets.md)。

`Map_Hub` 是默认启动关卡。主菜单进入主城，在出生点附近可用 `B` 打开准备面板，购买天赋、调整携装，再选择出发前往地牢一或地牢二。`E` 前往地牢一。每次出发是单独的一局，两张新地牢使用固定布局。各有敌人、Boss、商店和撤离点。成功撤离或死亡结算后按 `R` 返回主城。`Tools/PlayPrototype.ps1 -Classic` 可进入 0.9 的 `L_FrontierDungeon`，继续试玩种子控制的房间排列。

切换关卡时角色职业和待携装备由本次程序会话保存。目标关卡加载并生成当前玩家后，原有保存事务才扣除准备携带的仓库物品并开始本局。两个地牢继续使用同一套商店、掉落、战斗反馈、撤离与结算代码，沿用项目存档格式。主城的地图摆设和地牢的墙体、道具、主要空间布局来自组员原作。整合脚本补充了配置点、敌人和结算交互，并修正会阻挡通行的门框、装饰门和地板碰撞。`Map_Dungeon1` 的外部角色、敌人和 GameMode 失效引用已由主工程相应功能替代。主城使用动态光照，脱离第三人称示例图的烘焙数据。

后续调整遭遇位置时，可以选中不同关卡里的 `ExpeditionSite` 修改出生点附近的敌人、Boss 与初始补给偏移。商店和撤离点是地图演员，可在关卡编辑器中移动。两张新地牢及主城已加入地图烘焙清单。

当前版本的 [主城](Images/town-castle-and-market.png)、[地牢一](Images/dungeon-1-trader-and-extraction.png)和[地牢二](Images/dungeon-2-ashen-warden.png)截图由 UE4.27.2 使用独立测试存档重新拍摄，分辨率为 1920 × 1080。镜头展示了主城全景、地牢一的商人和撤离点，以及地牢二的灰烬守望者。拍摄时隐藏了 HUD，并微调了地牢镜头的曝光。中英文 README 使用相同的仓库图片文件与相对路径。0.9 的截图留在对应历史说明中。

这次实机截图检查发现，入口火把不足以照清人物，两个房间的敌人名字也会叠在一起或盖住 HUD。地牢增加了低强度补光，保留原来火把的暖光。界面优先显示距离较近、位置不和其他标签及 HUD 重叠的敌人名称与血条。

| 验证 | 结果与本地日志 |
|---|---|
| 默认回归 | 12 项通过，覆盖此次跨关卡流程和原有经典地牢玩法。`Saved/test_nov25_full.log` |
| 新地牢流程 | 同一测试存档从主城选择地牢一，验证法师和法杖随行、商店交互、撤离入库及回城，然后选择地牢二并验证死亡回城。两张地牢的商店、撤离点和 Boss 均通过 NavMesh 连通性检查。`Saved/TestReport_L_FrontierDungeon_Integration_Expeditions/index.json` |
| Development Editor / Game 构建 | 两个目标均成功。`Saved/build_nov25_editor.log`、`Saved/build_nov25_game.log` |
| 最终渲染复验 | 双地牢流程通过，0 失败、0 测试警告。检查了三张 1280×720 实机截图和 HUD 标签避让。`Saved/ExpeditionsRendered/index.json` |
| 蓝图编译与三张新图烘焙 | 均通过，UE4 汇总 0 错误、0 警告。`Saved/compile_nov25_blueprints.log`、`Saved/cook_nov25.log` |

烘焙验证覆盖地图及其资源依赖，正式 Windows 打包与发布安装测试仍待进行。
