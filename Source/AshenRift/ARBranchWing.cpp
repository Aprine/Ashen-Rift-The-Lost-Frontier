#include "ARBranchWing.h"

#include "ARExpeditionSite.h"
#include "ARProgression.h"
#include "ARPrototype.h"
#include "ARRunGameInstance.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
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

FARBranchPlan FARBranchPlan::Generate(int32 Seed)
{
    FRandomStream Random(Seed ^ 0x1973cd);
    const int32 Fork = Id(EARBranchRoom::Junction);
    const int32 Guard = Id(EARBranchRoom::Guard);
    const int32 Supplies = Id(EARBranchRoom::Supplies);
    const int32 Elite = Id(EARBranchRoom::Elite);
    const int32 Exit = Id(EARBranchRoom::Exit);
    const int32 Layout = Random.RandRange(0, 2);
    const int32 SupplyParent = Layout == 1 ? Guard : Fork;
    const int32 ExitParent = Layout == 2 ? Guard : Fork;
    const int32 Side = Random.RandRange(0, 1) == 0 ? -1 : 1;
    FARBranchPlan Plan;
    Plan.Cells = { FIntPoint(-2, 0), FIntPoint(-1, 0), FIntPoint(0, 0),
        FIntPoint(Layout == 1 ? -1 : 0, Side), FIntPoint(1, 0), FIntPoint(Layout == 2 ? -1 : 0, -Side) };
    Plan.Edges = { FIntPoint(Id(EARBranchRoom::Entry), Guard), FIntPoint(Guard, Fork),
        FIntPoint(SupplyParent, Supplies), FIntPoint(Fork, Elite), FIntPoint(ExitParent, Exit) };
    const FIntPoint Forward = Directions[Random.RandRange(0, 3)];
    const FIntPoint Left(-Forward.Y, Forward.X);
    for (FIntPoint& Cell : Plan.Cells)
        Cell = FIntPoint(Cell.X * Forward.X + Cell.Y * Left.X, Cell.X * Forward.Y + Cell.Y * Left.Y);
    return Plan;
}

bool FARBranchPlan::Connects(int32 A, int32 B) const
{
    return Edges.Contains(FIntPoint(A, B)) || Edges.Contains(FIntPoint(B, A));
}

AARExpeditionBranchWing::AARExpeditionBranchWing()
{
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
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gold(TEXT("/Game/AshenRift/Materials/M_Trader.M_Trader"));
    FloorMesh = Floor.Object;
    WallMesh = Wall.Object;
    FrameMesh = Frame.Object;
    PortalMaterial = Blue.Object;
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
    RoomStyles[Id(EARBranchRoom::Entry)].Light = FColor(148, 191, 225);
    RoomStyles[Id(EARBranchRoom::Elite)].Light = FColor(245, 132, 105);
    RoomStyles[Id(EARBranchRoom::Exit)].Light = FColor(107, 186, 238);
}

void AARExpeditionBranchWing::Prepare(int32 Seed, EARPlayerClass PlayerClass)
{
    if (bPrepared || !IsValid(Entrance)) return;
    Layout = FARBranchPlan::Generate(Seed);
    FRandomStream Random(Seed ^ 0x50751);
    Reward = ARExpedition::ChooseEquipment(Random, PlayerClass);
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
    return Center(Room);
}

const TCHAR* AARExpeditionBranchWing::Name(int32 Room)
{
    const TCHAR* Names[] = { TEXT("入口"), TEXT("战斗"), TEXT("岔路"), TEXT("补给"), TEXT("精英"), TEXT("撤离") };
    return Room >= 0 && Room < Id(EARBranchRoom::Count) ? Names[Room] : TEXT("随机支路");
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

bool AARExpeditionBranchWing::Build(AARPlayerCharacter* Player)
{
    if (bBuilt) return IsValid(Return);
    if (!bPrepared || !IsValid(Player) || !IsValid(Entrance) || !FloorMesh || !WallMesh || !FrameMesh ||
        RoomStyles.Num() != Layout.Cells.Num()) return false;
    auto* World = GetWorld();
    auto* Mode = World->GetAuthGameMode<AARPrototypeGameMode>();
    auto* State = World->GetGameInstance<UARRunGameInstance>();
    if (!Mode || !State || State->Phase != EARRunPhase::InDungeon) return false;

    TArray<AActor*> Pending;
    for (int32 Room = 0; Room < Layout.Cells.Num(); ++Room)
    {
        if (AActor* Shell = BuildRoom(Room)) Pending.Add(Shell);
    }
    const FVector Forward = (Center(Id(EARBranchRoom::Guard)) - Center(Id(EARBranchRoom::Entry))).GetSafeNormal2D();
    Return = World->SpawnActor<AARExpeditionPassage>(AARExpeditionPassage::StaticClass(),
        Center(Id(EARBranchRoom::Entry)) - Forward * 610 + FVector(0, 0, 14), Forward.Rotation());
    Shop = World->SpawnActor<AARShop>(AARShop::StaticClass(),
        Center(Id(EARBranchRoom::Supplies)) + FVector(420, -375, 60), FRotator::ZeroRotator);
    Exit = World->SpawnActor<AARPortal>(AARPortal::StaticClass(),
        Center(Id(EARBranchRoom::Exit)) + FVector(0, 0, 15), FRotator::ZeroRotator);
    if (Pending.Num() != Layout.Cells.Num() || !Return || !Shop || !Exit)
    {
        for (AActor* Actor : Pending) Actor->Destroy();
        for (AActor* Actor : {static_cast<AActor*>(Return), static_cast<AActor*>(Shop), static_cast<AActor*>(Exit)})
            if (IsValid(Actor)) Actor->Destroy();
        Return = nullptr;
        Shop = nullptr;
        Exit = nullptr;
        return false;
    }
    Return->Prompt = FText::FromString(TEXT("返回地牢三大厅"));
    Return->Destination = Entrance;
    Entrance->Destination = Return;
    Return->Mesh->SetMaterial(0, Entrance->Mesh->GetMaterial(0));
    Shop->SetActorScale3D(FVector(.42, .42, .22));
    Shop->Mesh->SetMaterial(0, TraderMaterial);
    Shop->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Exit->Mesh->SetMaterial(0, PortalMaterial);

    const FVector GuardAt = Center(Id(EARBranchRoom::Guard));
    const FVector EliteAt = Center(Id(EARBranchRoom::Elite)) + FVector(0, 0, 110);
    Mode->SpawnExpeditionEnemy(Mode->EnemyClass, GuardAt + FVector(-240, -95, 110), Center(0), NAME_None, State->ExpeditionTier);
    Mode->SpawnExpeditionEnemy(Mode->RangedEnemyClass, GuardAt + FVector(255, 260, 110), Center(0), NAME_None, State->ExpeditionTier);
    Mode->SpawnExpeditionEnemy(Mode->EliteEnemyClass, EliteAt, Center(Id(EARBranchRoom::Junction)), Reward, State->ExpeditionTier);
    bBuilt = true;
    if (auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)) Navigation->Build();
    return true;
}
