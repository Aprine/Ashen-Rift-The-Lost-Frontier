# 0.7 中文 UMG 与可点击界面

当前 0.8 在主菜单与暂停菜单增加音效开关，并接入战斗动作与声音，见 `12_Combat_Feedback.md`。本文件的操作与暂停规则继续适用。

日期：2026-11-03。UE4.27.2，C++ 核心 + 蓝图。默认地图仍是 `/Game/AshenRift/Maps/L_RandomDungeon`，本轮没有修改地图或已有 Content 资源。

## 启动与操作

运行 `Tools/PlayPrototype.ps1` 打开独立窗口。主菜单选择战士或法师，点击“进入营地”，在营地界面准备装备、升级天赋，再点击出发。所有界面使用中文，已有快捷键保留。

| 界面 | 鼠标操作 | 快捷键 |
|---|---|---|
| 主菜单 | 选择职业、进入营地、退出 | 1 / 2 选职业，Enter 进入营地 |
| 营地 | 职业、天赋、五类物品的加减、出发、关闭 | B、1/2、T/Y、方向键、E |
| 背包 | 装卸职业武器/皮甲/护符、用药、滚动物品、关闭 | Tab、Z/X/C、Q、Esc |
| 商店 | 买药水、买本局攻击强化、关闭 | 1 / 2、E、Esc |
| 暂停 | 继续、查看按键说明、退出 | Esc 继续 |
| 撤离/死亡结算 | 查看收益或损失、物品清单、返回或重试保存 | R |

独立窗口中的 Esc 会暂停游戏；若当前有营地、背包或商店面板，会先关闭该面板。编辑器 PIE 可能优先用 Esc 停止试玩，本轮暂停验收在 `-game` 独立窗口进行，未修改编辑器全局快捷键。

## 界面行为

UMG 替代旧英文 Canvas HUD。左上显示中文生命、法力、职业、伤害加成、货币与冷却，底部显示提示及路线；敌人显示中文名称、血条和蓄势提示，Boss 有独立血条。物品 ID 在显示时转为中文，存档中仍保留原 ID。主菜单、营地和结算面板居中，背包与商店靠右；界面以 1280×720 为设计基准等比缩放。

界面打开时显示鼠标，阻止玩家移动、转动视角、跳跃、攻击和技能，避免点击按钮同时出招；关闭后恢复游戏输入。背包按钮允许用药和装卸持有装备，商店按钮调用原有购买检查。所有按钮复用已有事务接口，没有直接更改金币、仓库或天赋数值。

背包和商店不暂停世界，画面遮罩较浅，仍显示生命和战斗通知。受伤或走远仍按原规则关闭商店。暂停菜单使用真正的世界暂停，敌人、弹体、撤离倒计时、法力恢复和技能冷却随游戏时间冻结；点击继续或按 Esc 后恢复。退出菜单注明未撤离物品不会保存。

输入阻挡读取当前副本阶段与面板状态，避免返回营地后同帧开始下一局时，被旧结算界面继续阻挡。界面保留所有数据校验：错误职业、空库存、余额不足、满级天赋、非法阶段均不会绕过原系统。

## 实现与字体

- `ARGameWidget.h/.cpp`：原生 UUserWidget 控件树、中文文本、按钮委托、面板、暂停、输入焦点与敌人血条。
- `ARGameHUD.cpp`：HUD 创建与清理界面；`ARPrototype.cpp` 已移除旧 Canvas 界面绘制。
- `ARPrototype` / `ARProgression`：面板输入阻挡与 Esc 映射；原战斗与经济接口继续复用。
- `Tests/ARUserInterfaceTests.cpp`：通过真实 UButton 的 OnClicked 委托验证界面操作，另测真实暂停与恢复。

使用 UE4 自带 Slate 默认复合字体及 DroidSansFallback 中文回退；未复制 Windows 字体，也未下载安装外部字体。界面是 C++ 构造的 UMG 控件树，目前没有一套可直接在 UMG 设计器拖拽编辑的 WBP 资源。中文文本尚未整理为多语言 String Table / locres。

## 验证

本机完成：

- Development Editor 与 Development Game 编译成功，日志 `Saved/build_m6_editor.log`、`Saved/build_m6_game.log`。
- 完整 9 项自动化测试通过：3 项核心测试，以及 RunLoop、Progression、RandomDungeon、Camp、EnemyVariants、UserInterface 各 1 项集成测试。总日志 `Saved/test_m6_all.log`。
- UserInterface 集成测试调用真实 UButton 委托，覆盖职业、营地、错误阶段拒绝、空库存拒绝、出发、装备、用药、点击时不攻击、关闭后恢复技能、真实暂停/继续、背包不暂停、商店扣费、撤离入库、天赋扣符文、准备不扣库存、换职业取消错误武器、死亡结算、永久成长保留及返回营地恢复输入。
- GPU 模式 UI 测试通过，报告 `Saved/AutomationUserInterfaceRendered/index.json`，1 成功、0 失败、0 警告；已检查 `Saved/Screenshots/M6_MainMenu.png`、`M6_Camp.png`、`M6_Inventory.png`、`M6_Shop.png`、`M6_Pause.png`、`M6_Result.png`、`M6_Death.png`，1280×720 下中文、长按钮、清单和底部操作完整可见。
- 新中文 HUD 的敌人战斗及 Boss 流程另做 GPU 复验，报告 `Saved/AutomationUICombatRendered/index.json`、`Saved/AutomationUIBossRendered/index.json`；检查中文敌人名、蓄势、瞄准线、Boss 血条和红圈提示。最新战斗截图 `M5_Hexer.png`、`M5_Sentinel.png` 与 `M2_Boss.png` 已被本轮 HUD 复验更新。
- 项目蓝图编译完成：0 错误、0 警告、0 加载失败，日志 `Saved/compile_m6_blueprints.log`。
- 本机可见窗口实际点击“进入营地”、按 Esc 关闭面板、再次 Esc 暂停、鼠标点击“继续游戏”、按 B 重新打开营地均已验证。试玩停在营地准备界面，未选择携带或购买升级。测试档案使用独立 GUID，成功结束时删除自身档案；没有消费玩家的仓库物品或符文。

完整回归后追加了纯显示调整，并对 UI、死亡结算和战斗分别作 GPU 复验。没有执行最终打包、打包版重启存档验收或磁盘故障注入。

## 仍待完成

正式战斗动画、音效、风格资产、场景英文占位标牌替换、最终 Windows 打包与打包版存档验收仍待完成。没有新增音效或音量设置。没有对任意分辨率、手柄导航和完整无障碍支持进行验收。

本轮之前的源码、配置、工具与文档备份：`Backups/Source_0.6_20261103.zip`；Content 未包含在此备份，本轮没有修改 Content。
