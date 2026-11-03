# 首轮蓝图施工

历史路线：本文件记录 0.1 的纯蓝图施工建议。0.2 已使用 C++ + 蓝图完成首轮闭环，当前操作与接入以 `06_First_Playable.md` 为准，不要按此文件重复添加组件或伤害转发。

这份施工表对应尚未实现的 M1。必须在 UE4 编辑器创建和编译蓝图节点；文件夹和地图占位不会自动具备这些功能。每完成一节就 Play 测试，避免最后集中接线。

## 1 输入和 Health

Project Settings → Input 新增 Action Mapping：Attack=Left Mouse Button、Interact=E、Potion=Q、Inventory=Tab。暂时保留模板 Jump。

创建 Actor Component `BPC_Health`：MaxHealth=100、CurrentHealth=100、bDead=false；Dispatcher `OnHealthChanged(Current, Max)`、`OnDied`、`OnDamaged`。BeginPlay 设置 CurrentHealth=MaxHealth。

函数 `ReceiveDamage(Amount)`：若 bDead 或 Amount<=0 直接返回；CurrentHealth=Clamp(CurrentHealth-Amount,0,MaxHealth)；广播 OnHealthChanged、OnDamaged；若 CurrentHealth<=0，先设 bDead=true，再广播 OnDied，保证同帧重复伤害只死一次。

函数 `Heal(Amount)`：拒绝 bDead、非正数、满血；回复 Clamp 后返回实际回复量。药水只在实际回复量>0 时扣除。

将组件加入 BP_ARPlayer 和未来的 BP_EnemyBase。各自 Event AnyDamage 调用 ReceiveDamage。UI 绑定组件的事件，不在 Tick 中复制 HP。

## 2 玩家攻击

BP_ARPlayer 新增 AttackDamage=25、AttackRange=180、AttackCooldown=.6、bCanAttack=true。Attack Pressed：检查 Health.bDead=false、RunPhase=InDungeon、bCanAttack；设 bCanAttack=false；Set Timer by Event（不循环，.6 秒）恢复攻击。

第一版立刻判定命中：GetActorLocation + ForwardVector*90 为球心，Sphere Overlap Actors，半径 110，Object Types=Pawn、Actors To Ignore=Self。ForEach 仅处理 BP_EnemyBase 且与正前方点积>0 的目标；每个 Actor Apply Damage 一次，Damage Causer=Self，Instigator=GetController。先验证伤害，之后改成攻击 Montage 的 AnimNotify 驱动判定，并在每次挥击开始清空 HitActors，持续检测时 AddUnique 防止重复命中。

不要同时保留即时判定和 AnimNotify 判定，否则一次挥击会伤害两次。动画未接入前可以 Print String 显示攻击被触发，不能将打印视作战斗完成。

## 3 敌人和追击

新建 Character BP_EnemyBase，先使用可见胶囊或模板骨骼占位，添加 BPC_Health（MaxHealth=60），CharacterMovement MaxWalkSpeed=200。新建 BP_ARAIController，敌人 AI Controller Class 指向它，Auto Possess AI=Placed in World or Spawned。

在战斗区域放置 NavMeshBoundsVolume，覆盖所有地面，按 P 检查绿色导航。AIController OnPossess 启动 .2 秒循环 Timer：获取玩家；若双方存活且 Phase=InDungeon，Distance<=150 则 Stop Movement 并尝试攻击；Distance<=1000 则 AI MoveTo(Player)；其他停止追击。失败移动要记录，不要每个 Tick 发新路径。

敌人攻击：独立冷却 1.2 秒；计时结束/命中时重新检查双方存活与距离，Apply Damage 10。Health.OnDied：先停止计时器与移动，再关闭胶囊碰撞，触发一次掉落，最后 Destroy Actor。无论死亡动画播放多久，死亡敌人不能继续造成伤害。

## 4 金币和简易背包

新建 BP_ARGameInstance 并在 Project Settings → Maps & Modes 设置 Game Instance Class。M1 增加 RunGold、PotionCount、BankGold、RunPhase、bSettled。BeginRun 清空临时数值，将 RunPhase=InDungeon。测试入口可先由 BP_ARPlayer BeginPlay 调用一次 BeginRun；做正式主城后移到进入副本交互中。

BP_Loot 使用 Sphere Collision + 可见 Mesh。变量 ItemID、Quantity=10、bClaimed=false。Overlap 只接收玩家；拒绝 bClaimed 或非 InDungeon；设置 bClaimed=true，再调用 TryAddLoot；成功才 Destroy Actor，失败恢复 bClaimed=false。M1 Gold 增加 RunGold；Potion 增加 PotionCount，数量上限 5。敌人死时 SpawnActor BP_Loot，使用 Always Spawn 或调整高度避免地面碰撞失败。

Q：PotionCount>0 且 Health.Heal(30) 实际回复>0，才扣一次药水。以后再将 PotionCount 迁移为组件 Stack，避免先写完整装备词缀。

## 5 死亡与撤离

GameInstance 新建 SettleRun(bSuccess)，按开发规格中的快照、状态锁定和保存提交顺序实现。Player Health.OnDied 调用 SettleRun(false)，之后禁用移动与攻击；不要先 Open Level，否则会丢失结算所需引用。

创建 BP_ExtractionPortal：Box Collision、Mesh、bPlayerInside、bExtracting、TimerHandle；先在地图占位平面旁放实例。BeginOverlap/EndOverlap 都验证玩家；玩家 E 调用交互入口。E：检查 Inside、Phase=InDungeon、玩家存活；Phase=Extracting，启动一次性 3 秒 Timer。

EndOverlap 或 Player.OnDamaged：Clear and Invalidate Timer，恢复 Phase=InDungeon。Timer 到期：再次检查 Inside、Health、Phase，再调用 SettleRun(true)。锁定 Settling 后，死亡和读条回调都不得再次结算。若读条完成和伤害同帧发生，以首先锁定结算的事件为准，结果只能有一个。

## 6 HUD 和结算

WBP_HUD 显示 HP、RunGold、PotionCount、交互提示与撤离读条。Player BeginPlay Create Widget → Add to Viewport；保存 Widget 引用，避免反复创建。

SG_ARProfile 先只实现 SaveVersion=1、BankGold。GameInstance Init：Does Save Game Exist → Load Game From Slot → Cast；不存在则 Create Save Game Object。Cast 或加载失败时显示错误，不覆盖旧档。

WBP_Result 从 RunSummary 读取成功/失败、获得金币、带出金币、损失金币。显示界面时 Set Input Mode UI Only、Show Mouse Cursor=true、暂停战斗。Return 按钮：关闭结算 UI，恢复输入和鼠标，Phase=Hub；M1 可传送角色回 PlayerStart，清除剩余怪与掉落、恢复 Health。正式版分 L_Hub 与 L_Dungeon 后用 Open Level，GameInstance 保存 Summary，GameMode 只负责当前场景。

必须能先通过一次撤离与一次死亡，再制作背包网格、第二职业与 Boss。
