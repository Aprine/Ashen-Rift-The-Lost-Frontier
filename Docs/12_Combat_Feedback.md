# 0.8 基础战斗动作与音效反馈

日期：2026-11-03。UE4.27.2，C++ 核心 + 蓝图，默认关卡保持 `/Game/AshenRift/Maps/L_RandomDungeon`。

此页记录 0.8 历史版本。0.9 的默认关卡已改为 `L_FrontierDungeon`，新增环境与验证见 [地图整合记录](15_Team_Map_Integration.md)；战斗反馈逻辑继续使用本页实现。

## 游玩变化

- 战士普攻、法师施法、旋风斩、敌人重击与受击都播放实际骨骼动作。
- 命中会出现短暂的身体闪烁、火花和实际损失生命数值；玩家受击另有轻微镜头位移反馈。
- 攻击、施法、命中、重击、受伤、治疗、拾取、撤离、死亡和界面点击加入音效。
- 主菜单及 Esc 暂停菜单增加“音效：开启/关闭”按钮；关闭会停止已有声音并阻止新声音，本次运行内有效。
- 修复同一帧普攻与范围法术互相碰撞的问题；同一施法者的法术现在彼此忽略，可独立飞出。

操作与伤害、消耗、掉落规则延续 0.7。背包、商店不暂停战斗，Esc 菜单会冻结游戏时间、动画进度、闪烁、镜头反馈及临时效果；界面点击音效仍可播放。

## 动画与表现实现

`/Game/AshenRift/Animations/ABP_ARCombat` 从模板动画蓝图复制，保留移动、跳跃状态机，在最终输出前加入 DefaultSlot。五段本项目生成的局部叠加动画为 Slash、Cast、Heavy、Hurt、Whirlwind，各有 25 帧关键帧。叠加形式使下肢移动和跳跃可继续运行。

这些是基础程序生成动作，并非最终手工精修或动捕动画。当前伤害判定仍使用已有逻辑，尚未迁移到 AnimNotify 或逐帧武器碰撞；受击动作也不会取消敌人的既有攻击计时。后续可进一步调整动作曲线与命中时机。

新动画、动画蓝图和两份特效材质都位于 `/Game/AshenRift`。原模板 `ThirdPerson_AnimBP` 与 `UE4_Mannequin_Skeleton` 的磁盘哈希已在生成前后核对，未改写。

`UARCombatFeedbackComponent` 管理动作、0.12 秒受击闪烁、材质原样恢复与 0.16 秒轻微镜头反馈。镜头反馈可通过组件参数关闭；重生时复位相机并停止动作。为保证离屏及无渲染测试中的骨骼更新，目前角色始终刷新骨骼姿态，尚未做大型场景的性能优化。

`AARImpactEffect` 是纯表现 Actor，含六个无碰撞、无导航影响的火花和朝向镜头的数字，0.55 秒后自动清理。同时最多 24 个，返回营地时一起清除。伤害数字表示实际扣除的生命，而不是溢出的原始伤害。

## 音频

11 个短音效由 `Scripts/generate_combat_audio.py` 原创合成，源文件在 `Data/AudioSource`，导入资产在 `/Game/AshenRift/Audio`。没有下载第三方音频，也没有读取麦克风。

`UARFeedbackSubsystem` 管理音效开关、资源、空间衰减与声音数量。空间声音在约 600 cm 内保持音量，之后逐渐衰减；最多同时保留 12 个声音，同类声音有 25 ms 的防重叠间隔。界面点击使用 2D 声音，其余事件使用空间声音。当前只有开关，没有音量滑块或背景音乐。

生成源 WAV 为 22050 Hz 单声道 PCM16，峰值约 0.58；运行时播放音量另有降低。Animations 与 Audio 目录已加入 AlwaysCook，但尚未执行最终打包验收。

## 代码与工具

- `ARCombatAssets.h/.cpp`：编辑器内生成独立动画和 Slot 蓝图副本；游戏目标不包含编辑器实现。
- `ARCombatFeedback.h/.cpp`：音效子系统、反馈组件、火花与伤害数字。
- `ARPrototype`、`ARProgression`、`AREnemyVariants`、`ARRunGameInstance`：在已有成功事件上触发反馈，不另算伤害或掉落。
- `ARGameWidget.cpp`：音效开关和按钮点击声。
- `Scripts/create_combat_feedback_assets.py`：导入音频并新增材质，遇到已存在资源时复用，不覆盖旧资源。
- `Tests/ARCombatFeedbackTests.cpp`：真实动作、骨骼姿态、闪烁、暂停、恢复、消耗及音效验证。

## 验证

本机完成：

- Development Editor 与 Development Game 编译成功，日志 `Saved/build_m7_editor.log`、`Saved/build_m7_game.log`。
- `Tools/TestPrototype.ps1` 全部 10 项测试通过：3 项核心测试，RunLoop、Progression、RandomDungeon、Camp、EnemyVariants、UserInterface、CombatFeedback 各 1 项集成测试。组合施法修复后已重新跑完整套，日志 `Saved/test_m7_all.log`。
- 新反馈测试验证真实动态 Montage 播放、手部骨骼位置变化、一次伤害与冷却、实际生命损失数字、闪烁恢复原材质、镜头复位、暂停冻结动作与闪烁、法力消耗不变、同帧两法术不互撞，以及重生清理。
- GPU 模式反馈流程通过：1 成功、0 失败、0 警告，报告 `Saved/AutomationCombatFeedbackRendered/index.json`。已检查 `Saved/Screenshots/M7_HitFlash.png`、`M7_SlashHit.png`、`M7_MageCast.png`，可见命中闪烁、火花、数字、挥砍抬臂及施法弹体。
- 使用 UE AudioMixer 自身的输出录制导出 `Saved/AudioCaptures/M7_CombatMix.wav`，分析记录 `Saved/audio_validation_m7.json`：48000 Hz、8 通道、约 1.024 秒，峰值约 0.2403、RMS 约 0.01155，确认非静音且无削波。本轮录制的是游戏自身混音，不是麦克风或系统全局音频。
- 全项目蓝图编译成功，含新 ABP_ARCombat，0 错误、0 警告、0 加载失败，日志 `Saved/compile_m7_blueprints.log`。
- 原模板动画蓝图与骨骼文件最终哈希与生成前记录一致，基线文件 `Saved/template_hashes_m7_before.json`。
- 本机可见 0.8 主菜单中实际点击关闭、再开启音效均成功，按钮文案即时更新；窗口停在主菜单，音效为开启状态，未进入副本或消费玩家档案。

自动化测试使用 GUID 隔离存档，结束时删除自身档案；未消费或覆盖玩家档案。没有进行主观音质评审、性能基准或最终打包验收。

## 仍待完成

正式武器与环境美术、动作精修和命中同步、音效混音打磨、背景音乐、性能基准及最终 Windows 打包仍待完成。本轮没有改变永久档案格式。

0.7 源码、配置、脚本、工具和文档备份：`Backups/Source_0.7_20261103.zip`。该备份不包含 Content；本轮只新增战斗动画、动画蓝图副本、音频及特效材质，保留原模板资源。
