#include "ARBranchWing.h"

#include "ARExpeditionSite.h"
#include "ARHealthComponent.h"
#include "ARInventoryComponent.h"
#include "ARProgression.h"
#include "ARPrototype.h"
#include "ARRunGameInstance.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr int32 Id(EARBranchRoom Room) { return static_cast<int32>(Room); }
    const FIntPoint Directions[] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };

    template <typename T>
    T* AttachInstance(AActor* Owner, USceneComponent* Parent)
    {
        auto* Component = NewObject<T>(Owner);
        Owner->AddInstanceComponent(Component);
        Component->SetupAttachment(Parent);
        return Component;
    }

    void Block(AActor* Room, USceneComponent* Root, const FVector& At, const FVector& Extent, float Yaw = 0)
    {
        auto* Shape = AttachInstance<UBoxComponent>(Room, Root);
        Shape->SetBoxExtent(Extent);
        Shape->SetRelativeLocationAndRotation(At, FRotator(0, Yaw, 0));
        Shape->SetCollisionProfileName(TEXT("BlockAll"));
        Shape->RegisterComponent();
    }
}

FARBranchPlan FARBranchPlan::Generate(int32 Seed, bool bStandalone)
{
    FRandomStream Random(Seed ^ 0x1973cd);
    const int32 Fork = Id(EARBranchRoom::Junction);
    const int32 Guard = Id(EARBranchRoom::Guard);
    const int32 Supplies = Id(EARBranchRoom::Supplies);
    const int32 Elite = Id(EARBranchRoom::Elite);
    const int32 Exit = Id(EARBranchRoom::Exit);
    const int32 Cache = Id(EARBranchRoom::Cache);
    const int32 Layout = Random.RandRange(0, 2);
    const int32 SupplyParent = Layout == 1 ? Guard : Fork;
    const int32 ExitParent = Layout == 2 ? Guard : Fork;
    const int32 Side = Random.RandRange(0, 1) == 0 ? -1 : 1;
    const FIntPoint Forward = Directions[Random.RandRange(0, 3)];
    const bool bCacheToShop = Layout == 2 || (Layout == 0 && Random.RandRange(0, 1) != 0);
    FARBranchPlan Plan;
    Plan.Cells = { FIntPoint(-2, 0), FIntPoint(-1, 0), FIntPoint(0, 0),
        FIntPoint(Layout == 1 ? -1 : 0, Side), FIntPoint(1, 0), FIntPoint(Layout == 2 ? -1 : 0, -Side),
        FIntPoint(1, bCacheToShop ? Side : -Side) };
    Plan.Edges = { FIntPoint(Id(EARBranchRoom::Entry), Guard), FIntPoint(Guard, Fork),
        FIntPoint(SupplyParent, Supplies), FIntPoint(Fork, Elite), FIntPoint(ExitParent, Exit),
        FIntPoint(Elite, Cache), FIntPoint(Cache, bCacheToShop ? Supplies : Exit) };
    if (bStandalone)
    {
        Plan.Cells.Add(FIntPoint(2, 0));
        Plan.Edges.Emplace(Elite, Id(EARBranchRoom::Boss));
        Plan.Cells.Add(FIntPoint(2, bCacheToShop ? -Side : Side));
        Plan.Edges.Emplace(Id(EARBranchRoom::Boss), Id(EARBranchRoom::DeepExit));
    }
    const FIntPoint Left(-Forward.Y, Forward.X);
    for (FIntPoint& Cell : Plan.Cells)
        Cell = FIntPoint(Cell.X * Forward.X + Cell.Y * Left.X, Cell.X * Forward.Y + Cell.Y * Left.Y);
    return Plan;
}

bool FARBranchPlan::Connects(int32 A, int32 B) const
{
    return Edges.Contains(FIntPoint(A, B)) || Edges.Contains(FIntPoint(B, A));
}

AARBranchCache::AARBranchCache()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Cache"));
    auto* Chest = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chest"));
    Chest->SetupAttachment(RootComponent);
    Chest->SetRelativeRotation(FRotator(0, 0, 90));
    Chest->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Chest->SetCanEverAffectNavigation(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> KitChest(TEXT("/Game/Environments/KayKitDungeon/Meshes/Props/SM_chest_gold.SM_chest_gold"));
    Chest->SetStaticMesh(KitChest.Object);
}

void AARBranchCache::Configure(AAREnemy* InGuard, bool bGiveRunes)
{
    Guard = InGuard;
    bRunes = bGiveRunes;
}

bool AARBranchCache::HasLivingGuard() const
{
    const auto* Enemy = Guard.Get();
    return Enemy && !Enemy->Health->bDead;
}

bool AARBranchCache::IsNearby(const AARPlayerCharacter* Player) const
{
    if (!IsValid(Player) || !Player->CanPlay()) return false;
    const FVector Offset = Player->GetActorLocation() - GetActorLocation();
    return Offset.SizeSquared2D() <= FMath::Square(195.f) && FMath::Abs(Offset.Z) <= 190.f;
}

bool AARBranchCache::Claim(AARPlayerCharacter* Player)
{
    auto* State = GetWorld()->GetGameInstance<UARRunGameInstance>();
    if (!IsNearby(Player) || !State || State->Phase != EARRunPhase::InDungeon) return false;
    if (bClaimed || HasLivingGuard())
    {
        Player->ShowNotice(Prompt());
        return false;
    }
    // Inventory and currency changes notify UI listeners synchronously.
    // Set the flag first so the same chest cannot grant its reward twice.
    bClaimed = true;
    const bool bGiven = bRunes ? State->AddCurrency(0, RewardAmount) : Player->Inventory->TryAdd(TEXT("Potion"), RewardAmount);
    bClaimed = bGiven;
    if (bGiven) Player->ShowNotice(bRunes ? TEXT("储藏箱 · 获得 2 枚符文") : TEXT("储藏箱 · 获得 2 瓶治疗药水"));
    else Player->ShowNotice(bRunes ? TEXT("无法领取储藏箱奖励 · 本局符文已达上限") : TEXT("无法领取储藏箱奖励 · 请检查背包"));
    return bGiven;
}

FString AARBranchCache::Prompt() const
{
    if (bClaimed) return TEXT("储藏箱已取空");
    if (HasLivingGuard()) return TEXT("击败守卫后可打开储藏箱");
    return bRunes ? TEXT("领取储藏箱 · 2 枚符文") : TEXT("领取储藏箱 · 2 瓶治疗药水");
}

AARExpeditionBranchWing::AARExpeditionBranchWing()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickInterval = .15f;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("BranchOrigin"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Floor(TEXT("/Game/Environments/KayKitDungeon/Meshes/Floors/SM_floor_tile_large.SM_floor_tile_large"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Wall(TEXT("/Game/Environments/KayKitDungeon/Meshes/Walls/SM_wall_scaffold.SM_wall_scaffold"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Frame(TEXT("/Game/Environments/KayKitDungeon/Meshes/Walls/SM_wall_open_scaffold.SM_wall_open_scaffold"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Boxes(TEXT("/Game/Environments/KayKitDungeon/Meshes/Props/SM_crates_stacked.SM_crates_stacked"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Rubble(TEXT("/Game/Environments/KayKitDungeon/Meshes/Props/SM_rubble_half.SM_rubble_half"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Pillar(TEXT("/Game/Environments/KayKitDungeon/Meshes/Props/SM_pillar_decorated.SM_pillar_decorated"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Counter(TEXT("/Game/Environments/KayKitDungeon/Meshes/Props/SM_table_medium_decorated_A.SM_table_medium_decorated_A"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Keg(TEXT("/Game/Environments/KayKitDungeon/Meshes/Props/SM_keg_decorated.SM_keg_decorated"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Chest(TEXT("/Game/Environments/KayKitDungeon/Meshes/Props/SM_chest_gold.SM_chest_gold"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Blue(TEXT("/Game/AshenRift/Materials/M_Portal.M_Portal"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Amber(TEXT("/Game/AshenRift/Materials/M_Gold.M_Gold"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gold(TEXT("/Game/AshenRift/Materials/M_Trader.M_Trader"));
    FloorMesh = Floor.Object;
    WallMesh = Wall.Object;
    FrameMesh = Frame.Object;
    PortalMaterial = Blue.Object;
    DeepPortalMaterial = Amber.Object;
    TraderMaterial = Gold.Object;
    RoomStyles.SetNum(Id(EARBranchRoom::Count));
    const auto Prop = [&](EARBranchRoom Room, UStaticMesh* Mesh, float X, float Y, FVector Extent = FVector::ZeroVector)
    {
        auto& Detail = RoomStyles[Id(Room)].Props.AddDefaulted_GetRef();
        Detail.Mesh = Mesh;
        Detail.Location = FVector(X, Y, 0);
        Detail.CollisionExtent = Extent;
    };
    Prop(EARBranchRoom::Entry, Boxes.Object, -720, -680);
    Prop(EARBranchRoom::Guard, Rubble.Object, -430, 465, FVector(105, 100, 48));
    Prop(EARBranchRoom::Guard, Pillar.Object, 500, -370, FVector(76, 76, 175));
    Prop(EARBranchRoom::Junction, Pillar.Object, -580, -550, FVector(76, 76, 175));
    Prop(EARBranchRoom::Junction, Pillar.Object, 580, 550, FVector(76, 76, 175));
    Prop(EARBranchRoom::Supplies, Counter.Object, 470, -385);
    Prop(EARBranchRoom::Supplies, Keg.Object, 690, -690);
    Prop(EARBranchRoom::Elite, Pillar.Object, -500, 465, FVector(76, 76, 175));
    Prop(EARBranchRoom::Elite, Rubble.Object, 545, -450, FVector(105, 100, 48));
    Prop(EARBranchRoom::Exit, Chest.Object, 635, 590);
    Prop(EARBranchRoom::Cache, Keg.Object, -700, 630);
    Prop(EARBranchRoom::Cache, Boxes.Object, 595, -620);
    Prop(EARBranchRoom::Boss, Pillar.Object, -720, 690, FVector(90, 85, 180));
    Prop(EARBranchRoom::Boss, Pillar.Object, 720, -690, FVector(90, 85, 180));
    Prop(EARBranchRoom::DeepExit, Pillar.Object, -630, 650, FVector(76, 76, 175));
    Prop(EARBranchRoom::DeepExit, Chest.Object, 680, -610);
    RoomStyles[Id(EARBranchRoom::Entry)].Light = FColor(148, 191, 225);
    RoomStyles[Id(EARBranchRoom::Elite)].Light = FColor(245, 132, 105);
    RoomStyles[Id(EARBranchRoom::Exit)].Light = FColor(107, 186, 238);
    RoomStyles[Id(EARBranchRoom::Cache)].Light = FColor(205, 214, 141);
    RoomStyles[Id(EARBranchRoom::Boss)].Light = FColor(248, 144, 88);
    RoomStyles[Id(EARBranchRoom::DeepExit)].Light = FColor(252, 199, 97);
}

void AARExpeditionBranchWing::PostLoad()
{
    Super::PostLoad();
    // Keep existing instance edits and add styles introduced after the map was saved.
    if (RoomStyles.Num() < Id(EARBranchRoom::Count))
    {
        const auto& Defaults = GetDefault<AARExpeditionBranchWing>()->RoomStyles;
        for (int32 Room = RoomStyles.Num(); Room < Defaults.Num(); ++Room)
            RoomStyles.Add(Defaults[Room]);
    }
}

void AARExpeditionBranchWing::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (const auto* Player = UGameplayStatics::GetPlayerPawn(this, 0)) RecordVisit(Player->GetActorLocation());
}

void AARExpeditionBranchWing::RecordVisit(const FVector& Position)
{
    const int32 Room = RoomAt(Position);
    if (Room != INDEX_NONE) Visited.Add(Room);
}

void AARExpeditionBranchWing::Prepare(int32 Seed, EARPlayerClass PlayerClass)
{
    if (bPrepared || (!bStandalone && !IsValid(Entrance))) return;
    Layout = FARBranchPlan::Generate(Seed, bStandalone);
    FRandomStream Random(Seed ^ 0x50751);
    Reward = ARExpedition::ChooseEquipment(Random, PlayerClass);
    bCacheRunes = FRandomStream(Seed ^ 0x106c8b).RandRange(0, 1) != 0;
    bPrepared = true;
}

FVector AARExpeditionBranchWing::Center(int32 Room) const
{
    if (!Layout.Cells.IsValidIndex(Room)) return GetActorLocation();
    const FIntPoint Cell = Layout.Cells[Room];
    return GetActorLocation() + FVector(Cell.X * RoomSize, Cell.Y * RoomSize, 0);
}

int32 AARExpeditionBranchWing::RoomAt(const FVector& Position) const
{
    const FVector Relative = Position - GetActorLocation();
    if (!bBuilt || Relative.Z < -50 || Relative.Z > 490) return INDEX_NONE;
    return Layout.Cells.IndexOfByKey(FIntPoint(
        FMath::FloorToInt(Relative.X / RoomSize + .5f), FMath::FloorToInt(Relative.Y / RoomSize + .5f)));
}

FVector AARExpeditionBranchWing::Goal(int32 Room) const
{
    if (Room == Id(EARBranchRoom::Entry) && IsValid(Return)) return Return->GetActorLocation();
    if (Room == Id(EARBranchRoom::Supplies) && IsValid(Shop)) return Shop->GetActorLocation();
    if (Room == Id(EARBranchRoom::Exit) && IsValid(Exit)) return Exit->GetActorLocation();
    if (Room == Id(EARBranchRoom::DeepExit) && IsValid(DeepExit)) return DeepExit->GetActorLocation();
    if (Room == Id(EARBranchRoom::Cache) && IsValid(Cache)) return Cache->GetActorLocation();
    return Center(Room);
}

FVector AARExpeditionBranchWing::StartingLocation() const
{
    const FVector Forward = (Center(Id(EARBranchRoom::Guard)) - Center(Id(EARBranchRoom::Entry))).GetSafeNormal2D();
    return Center(Id(EARBranchRoom::Entry)) - Forward * 405 + FVector(0, 0, 105);
}

float AARExpeditionBranchWing::StartingYaw() const
{
    return (Center(Id(EARBranchRoom::Guard)) - Center(Id(EARBranchRoom::Entry))).Rotation().Yaw;
}

const TCHAR* AARExpeditionBranchWing::Name(int32 Room)
{
    const TCHAR* Names[] = { TEXT("入口"), TEXT("战斗"), TEXT("岔路"), TEXT("补给"), TEXT("精英"),
        TEXT("撤离"), TEXT("储藏"), TEXT("首领"), TEXT("深层撤离") };
    return Room >= 0 && Room < Id(EARBranchRoom::Count) ? Names[Room] : TEXT("随机支路");
}

bool AARExpeditionBranchWing::IsEncounter(int32 Room)
{
    return Room == Id(EARBranchRoom::Guard) || Room == Id(EARBranchRoom::Elite) ||
        Room == Id(EARBranchRoom::Cache) || Room == Id(EARBranchRoom::Boss) || Room == Id(EARBranchRoom::DeepExit);
}

int32 AARExpeditionBranchWing::LivingEnemies(int32 Room) const
{
    if (!Layout.Cells.IsValidIndex(Room) || !IsEncounter(Room)) return 0;
    if (!bBuilt) return Room == Id(EARBranchRoom::Guard) ? 2 : 1;
    int32 Count = 0;
    for (const TWeakObjectPtr<AAREnemy>& Enemy : RoomEnemies[Room])
        if (const auto* Actor = Enemy.Get())
            if (!Actor->Health->bDead) ++Count;
    return Count;
}

bool AARExpeditionBranchWing::WasCacheClaimed() const
{
    return IsValid(Cache) && Cache->IsClaimed();
}

AActor* AARExpeditionBranchWing::BuildRoom(int32 Room)
{
    auto* Shell = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());
    if (!Shell) return nullptr;
    Shell->Tags.Add(TEXT("BranchModule"));
    auto* Root = NewObject<USceneComponent>(Shell, TEXT("Room"));
    Shell->AddInstanceComponent(Root);
    Shell->SetRootComponent(Root);
    Root->RegisterComponent();
    Shell->SetActorLocation(Center(Room));
    constexpr float Half = RoomSize / 2.f;
    Block(Shell, Root, FVector(0, 0, -20), FVector(Half + 1, Half + 1, 20));

    const auto MeshBatch = [&](UStaticMesh* Mesh)
    {
        auto* Batch = AttachInstance<UInstancedStaticMeshComponent>(Shell, Root);
        Batch->SetMobility(EComponentMobility::Movable);
        Batch->SetStaticMesh(Mesh);
        Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Batch->SetCanEverAffectNavigation(false);
        Batch->RegisterComponent();
        return Batch;
    };
    auto* Floor = MeshBatch(FloorMesh);
    auto* Walls = MeshBatch(WallMesh);
    auto* Doors = MeshBatch(FrameMesh);
    for (int32 X = -800; X <= 800; X += 400)
        for (int32 Y = -800; Y <= 800; Y += 400)
            Floor->AddInstance(FTransform(FRotator(0, 0, 90), FVector(X, Y, 0)));
    for (const FIntPoint& Direction : Directions)
    {
        const int32 Neighbor = Layout.Cells.IndexOfByKey(Layout.Cells[Room] + Direction);
        if (Neighbor != INDEX_NONE && Neighbor < Room) continue;
        const bool bDoor = Neighbor != INDEX_NONE && Layout.Connects(Room, Neighbor);
        const FVector Normal(Direction.X, Direction.Y, 0);
        const FVector Along(-Normal.Y, Normal.X, 0);
        const FVector Edge = Normal * Half;
        const float Yaw = Along.Rotation().Yaw;
        for (int32 Offset = -800; Offset <= 800; Offset += 400)
        {
            auto* Mesh = bDoor && Offset == 0 ? Doors : Walls;
            Mesh->AddInstance(FTransform(FRotator(0, Yaw, 90), Edge + Along * Offset));
        }
        if (bDoor)
        {
            Block(Shell, Root, Edge + Along * 600 + FVector(0, 0, 200), FVector(400, 50, 200), Yaw);
            Block(Shell, Root, Edge - Along * 600 + FVector(0, 0, 200), FVector(400, 50, 200), Yaw);
        }
        else Block(Shell, Root, Edge + FVector(0, 0, 200), FVector(Half, 50, 200), Yaw);
    }
    const FARBranchRoomStyle& Style = RoomStyles[Room];
    for (const FARBranchProp& Detail : Style.Props)
    {
        if (!Detail.Mesh) continue;
        auto* Prop = MeshBatch(Detail.Mesh);
        Prop->AddInstance(FTransform(FRotator(0, Detail.Yaw, 90), Detail.Location));
        if (Detail.CollisionExtent.GetMin() > 0)
            Block(Shell, Root, Detail.Location + FVector(0, 0, Detail.CollisionExtent.Z), Detail.CollisionExtent, Detail.Yaw);
    }
    for (float X : {-485.f, 485.f})
    {
        auto* Light = AttachInstance<UPointLightComponent>(Shell, Root);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetRelativeLocation(FVector(X, 0, 318));
        Light->SetIntensityUnits(ELightUnits::Candelas);
        Light->SetIntensity(1450);
        Light->SetAttenuationRadius(1500);
        Light->SetLightColor(FLinearColor(Style.Light));
        Light->SetCastShadows(false);
        Light->RegisterComponent();
    }
    return Shell;
}

bool AARExpeditionBranchWing::Build(AARPlayerCharacter* Player, int32 Tier)
{
    if (bBuilt) return bStandalone || IsValid(Return);
    if (!bPrepared || !IsValid(Player) || (!bStandalone && !IsValid(Entrance)) || !FloorMesh || !WallMesh || !FrameMesh ||
        RoomStyles.Num() < Layout.Cells.Num()) return false;
    auto* World = GetWorld();
    auto* Mode = World->GetAuthGameMode<AARPrototypeGameMode>();
    auto* State = World->GetGameInstance<UARRunGameInstance>();
    if (!Mode || !State || State->Phase != (bStandalone ? EARRunPhase::Hub : EARRunPhase::InDungeon)) return false;

    TArray<AActor*> Pending;
    for (int32 Room = 0; Room < Layout.Cells.Num(); ++Room)
    {
        if (AActor* Shell = BuildRoom(Room)) Pending.Add(Shell);
    }
    if (!bStandalone)
    {
        const FVector Forward = (Center(Id(EARBranchRoom::Guard)) - Center(Id(EARBranchRoom::Entry))).GetSafeNormal2D();
        Return = World->SpawnActor<AARExpeditionPassage>(AARExpeditionPassage::StaticClass(),
            Center(Id(EARBranchRoom::Entry)) - Forward * 610 + FVector(0, 0, 14), Forward.Rotation());
    }
    Shop = World->SpawnActor<AARShop>(AARShop::StaticClass(),
        Center(Id(EARBranchRoom::Supplies)) + FVector(420, -375, 60), FRotator::ZeroRotator);
    Exit = World->SpawnActor<AARPortal>(AARPortal::StaticClass(),
        Center(Id(EARBranchRoom::Exit)) + FVector(0, 0, 15), FRotator::ZeroRotator);
    Cache = World->SpawnActor<AARBranchCache>(AARBranchCache::StaticClass(),
        Center(Id(EARBranchRoom::Cache)) + FVector(430, 440, 4), FRotator(0, 180, 0));
    const FVector GuardAt = Center(Id(EARBranchRoom::Guard));
    const FVector EliteAt = Center(Id(EARBranchRoom::Elite)) + FVector(0, 0, 110);
    const FVector CacheAt = Center(Id(EARBranchRoom::Cache)) + FVector(-180, -140, 110);
    auto* Guard = Mode->SpawnExpeditionEnemy(Mode->EnemyClass, GuardAt + FVector(-240, -95, 110), Center(0), NAME_None, Tier);
    auto* Hexer = Mode->SpawnExpeditionEnemy(Mode->RangedEnemyClass, GuardAt + FVector(255, 260, 110), Center(0), NAME_None, Tier);
    auto* Elite = Mode->SpawnExpeditionEnemy(Mode->EliteEnemyClass, EliteAt, Center(Id(EARBranchRoom::Junction)), Reward, Tier);
    auto* CacheGuard = Mode->SpawnExpeditionEnemy(bCacheRunes ? Mode->RangedEnemyClass : Mode->EnemyClass,
        CacheAt, Center(Id(EARBranchRoom::Elite)), NAME_None, Tier);
    auto* Boss = bStandalone ? Mode->SpawnExpeditionEnemy(Mode->BossClass.Get(),
        Center(Id(EARBranchRoom::Boss)) + FVector(0, 0, 160), EliteAt, BossReward(), Tier) : nullptr;
    AAREnemy* DeepGuard = nullptr;
    if (bStandalone)
    {
        const FVector DeepSite = Center(Id(EARBranchRoom::DeepExit));
        DeepExit = World->SpawnActor<AARPortal>(AARPortal::StaticClass(), DeepSite + FVector(0, 0, 15), FRotator::ZeroRotator);
        DeepGuard = Mode->SpawnExpeditionEnemy(Mode->RangedEnemyClass, DeepSite + FVector(465, 420, 110), DeepSite, NAME_None, Tier);
    }
    if (Pending.Num() != Layout.Cells.Num() || (bStandalone ? (!Boss || !DeepExit || !DeepGuard) : !Return) ||
        !Shop || !Exit || !Cache || !Guard || !Hexer || !Elite || !CacheGuard)
    {
        for (AActor* Actor : Pending) Actor->Destroy();
        AActor* Spawned[] = {Return, Shop, Exit, Cache, Guard, Hexer, Elite, CacheGuard, Boss, DeepExit, DeepGuard};
        for (AActor* Actor : Spawned)
            if (IsValid(Actor)) Actor->Destroy();
        Return = nullptr;
        Shop = nullptr;
        Exit = nullptr;
        DeepExit = nullptr;
        Cache = nullptr;
        return false;
    }
    if (Return)
    {
        Return->Prompt = FText::FromString(TEXT("返回地牢三大厅"));
        Return->Destination = Entrance;
        Entrance->Destination = Return;
        Return->Mesh->SetMaterial(0, Entrance->Mesh->GetMaterial(0));
    }
    Shop->SetActorScale3D(FVector(.42, .42, .22));
    Shop->Mesh->SetMaterial(0, TraderMaterial);
    Shop->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Exit->Mesh->SetMaterial(0, PortalMaterial);
    Cache->Configure(CacheGuard, bCacheRunes);
    RoomEnemies.SetNum(Layout.Cells.Num());
    RoomEnemies[Id(EARBranchRoom::Guard)] = { Guard, Hexer };
    RoomEnemies[Id(EARBranchRoom::Elite)].Add(Elite);
    RoomEnemies[Id(EARBranchRoom::Cache)].Add(CacheGuard);
    if (Boss) RoomEnemies[Id(EARBranchRoom::Boss)].Add(Boss);
    if (DeepExit)
    {
        DeepExit->ConfigureDeep(Elite->Health, Tier);
        DeepExit->Mesh->SetMaterial(0, DeepPortalMaterial);
        RoomEnemies[Id(EARBranchRoom::DeepExit)].Add(DeepGuard);
    }
    bBuilt = true;
    SetActorTickEnabled(true);
    if (auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)) Navigation->Build();
    return true;
}
