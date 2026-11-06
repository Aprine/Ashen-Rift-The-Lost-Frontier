# 当前验证记录

最新状态：0.9 已整合 Kuangxin 的地图、三个战斗房间蓝图及营地蓝图，默认关卡为 `L_FrontierDungeon`。默认回归配置 11 项和旧灰盒配置 10 项均通过；Editor / Game 与蓝图编译、GPU 新地图复验通过。当前证据见 [地图整合记录](15_Team_Map_Integration.md)。0.8 战斗反馈与 AudioMixer 历史分析见 `12_Combat_Feedback.md`。下文保留早期历史排障记录，其中未接入玩法的描述已被后续版本取代。

记录日期：2026-10-28。此文件区分磁盘上已有的产物与尚未通过的运行验证。

## 已完成

- 读取课程 proposal 的正文和表格内容；保留原文件不修改。
- 确认本机引擎 Build.version 为 4.27.2，路径为 D:/Epic Games/UE_4.27。
- 复制本机官方 Third Person Blueprint 模板及 Mannequin、Geometry、ThirdPerson 依赖。
- 使用完整编辑器的 ExecutePythonScript 模式生成 L_Prototype、BP_ARPlayer、BP_ARGameMode。日志包含 ASHEN_RIFT_PROTOTYPE_BUILD_SUCCESS，场景与蓝图已保存到 Content。
- 在添加 C++ 模块前运行 CompileAllBlueprints，IgnoreFolder=/Engine，进程退出码 0，项目蓝图未报告编译错误。日志位于 Saved/compile_project_blueprints.log。
- 写入 C++ 模块和 BlueprintCallable API；调整 GameInstanceClass 指向原生类。

## 未通过或未执行

- 原先 C++ 构建缺少 .NET Framework SDK 的阻塞已经解决；当前编译与测试结果见下节。
- 场景生成的首轮 Python commandlet 在创建地图后发生访问异常；改用完整编辑器模式成功生成。完整编辑器批处理随后在退出阶段发生 EditorScriptingUtilities/Slate 错误。初始化脚本已移除重复主动 quit 调用，但未重新运行覆盖当前地图；退出问题尚未验证解决。
- 曾出现引擎全局缓存目录写入失败，项目配置已将 Boot 和 Local DDC 放入项目 DerivedDataCache；随后项目蓝图编译检查退出码 0。
- 未进行可见编辑器 Play、角色移动路径、碰撞、动画、灯光观感验收。
- C++ Health 与 Inventory 已完成核心自动化运行测试；Run/Save 接口尚未完成运行测试；原生组件尚未接入玩家蓝图。
- 未执行 Windows 打包，未完成游戏闭环。

## 接下来执行

按照 04_CPP_Blueprint_Integration 给角色添加两个原生组件。按 02 中的战斗与交互步骤完成 M1，逐项验证 03 中死亡、撤离和重复结算场景。

## 工具链安装与复验

2026-10-28 约 22:00（Asia/Shanghai）完成：

- 安装微软官方 VS2019 Build Tools 16.11.37823.30 至 D:/Development/VS2019BuildTools。vswhere：isComplete=true、isRebootRequired=false。安装器退出码 0。
- MSVC v142 工具目录 14.29.30133；UE4 编译日志报告工具链 14.29.30159。
- Windows 10 SDK 10.0.19041.0；.NET Framework 4.8 SDK 与 Targeting Pack。
- 安装器包 SHA256 D645C400C7E05906B8B73CD608CD7C9BA5734417A602E9E9DAFFB02F02F8E515 与微软下载 URL 一致；入口 setup.exe 的 Microsoft Authenticode 签名有效。
- AshenRiftEditor Win64 Development 编译退出码 0，生成 Binaries/Win64/UE4Editor-AshenRift.dll。日志：Saved/build_cpp.log。
- AshenRift Win64 Development 编译退出码 0，生成 Binaries/Win64/AshenRift.exe。日志：Saved/build_game.log。此 exe 是未烘焙构建产物，不能代替最终打包发行版。
- 本机 UE4.27.2 加载项目并运行 AshenRift.Core.Health、AshenRift.Core.Inventory，两项均 Passed，进程退出码 0。日志：Saved/core_tests.log；报告：Saved/Automation。
- Health 测试覆盖满血用药、负伤害、致死伤害、死亡后治疗和重置；Inventory 测试覆盖非法数量、容量、堆叠、失败不修改数据、移除与清空。
- 首次测试文件编译发生头文件路径错误，改为相对于 Tests 目录的路径后重新构建成功。

参考配置：Tools/UE4_2019.vsconfig；构建脚本：Tools/BuildProject.ps1。该脚本明确传播失败退出码，避免控制台输出成功掩盖底层编译失败。

官方资料：

- https://dev.epicgames.com/documentation/unreal-engine/setting-up-visual-studio-for-unreal-engine?application_version=4.27
- https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools
- https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio
