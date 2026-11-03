#include "ARDungeon.h"
#include "ARPrototype.h"
#include "ARProgression.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "NavigationSystem.h"
#if WITH_EDITOR
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Builders/CubeBuilder.h"
#include "Model.h"
#include "Components/BrushComponent.h"
#endif

AARDungeonRoom::AARDungeonRoom()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Cube = Shape.Object;
}
void AARDungeonRoom::Block(const FVector& Position, const FVector& Size)
{
    auto* Mesh = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Mesh);
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(Cube);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetCanEverAffectNavigation(true);
    Mesh->SetRelativeLocation(Position);
    Mesh->SetRelativeScale3D(Size / 100);
    Mesh->RegisterComponent();
    if (auto* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AshenRift/Materials/M_Wall.M_Wall"))) Mesh->SetMaterial(0, Mat);
}
void AARDungeonRoom::BuildInterior(int32 Kind)
{
    RoomKind = Kind;
    if (Kind == 0) // Four pillars, central route open.
        for (float X : {-450.f, 450.f}) for (float Y : {-500.f, 500.f}) Block(FVector(X, Y, 120), FVector(200, 200, 240));
    else if (Kind == 1) // A central barrier requires a real detour.
        Block(FVector(0, 0, 120), FVector(200, 1000, 240));
    else // Alternating barricades make an S-shaped route.
    {
        Block(FVector(-450, -350, 120), FVector(200, 800, 240));
        Block(FVector(450, 350, 120), FVector(200, 800, 240));
    }
}
AARDungeonDirector::AARDungeonDirector() { RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root")); }
TArray<int32> AARDungeonDirector::MakeRoomOrder(int32 Seed)
{
    TArray<int32> Result = {0, 1, 2};
    FRandomStream Stream(Seed);
    for (int32 I = Result.Num() - 1; I > 0; --I) Result.Swap(I, Stream.RandRange(0, I));
    return Result;
}
FString AARDungeonDirector::RoomName(int32 Kind)
{ return Kind == 0 ? TEXT("Pillars") : Kind == 1 ? TEXT("Barricade") : TEXT("Cross"); }
void AARDungeonDirector::Generate(int32 Seed)
{
    for (auto* Room : Rooms) if (IsValid(Room)) Room->Destroy();
    Rooms.Empty();
    CurrentSeed = Seed;
    RoomOrder = MakeRoomOrder(Seed);
    const int32 Slots[] = {1, 2, 4};
    for (int32 I = 0; I < 3; ++I)
    {
        auto* Room = GetWorld()->SpawnActor<AARDungeonRoom>(AARDungeonRoom::StaticClass(), FVector(Slots[I] * 2400, 0, 0), FRotator::ZeroRotator);
        if (Room) { Room->BuildInterior(RoomOrder[I]); Rooms.Add(Room); }
    }
    if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())) Nav->Build();
}
void AARDungeonDirector::Populate(AARPlayerCharacter* Player, TSubclassOf<AAREnemy> EnemyClass, TSubclassOf<AAREnemy> RangedClass, TSubclassOf<AAREnemy> EliteClass, TSubclassOf<AARBoss> BossClass)
{
    const int32 Slots[] = {1, 2, 4};
    for (int32 I = 0; I < 3; ++I)
    {
        const float X = Slots[I] * 2400;
        for (int32 J = 0; J < 2; ++J)
        {
            const FTransform Transform(FRotator(0, 180, 0), FVector(X + (J == 0 ? 650 : 800), J == 0 ? -300 : 500, 100));
            const TSubclassOf<AAREnemy> SpawnClass = J == 1 && RangedClass ? RangedClass : I == 2 && J == 0 && EliteClass ? EliteClass : EnemyClass;
            auto* Enemy = GetWorld()->SpawnActorDeferred<AAREnemy>(SpawnClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
            if (!Enemy) continue;
            if (J == 0) Enemy->DropItemID = I == 0 ? TEXT("LeatherArmor") : I == 1 ? TEXT("RunicCharm") : Player->SelectedClass == EARPlayerClass::Mage ? TEXT("OakStaff") : TEXT("IronSword");
            UGameplayStatics::FinishSpawningActor(Enemy, Transform);
        }
    }
    if (BossClass) GetWorld()->SpawnActor<AARBoss>(BossClass, FVector(12000, 0, 150), FRotator(0, 180, 0));
    const FTransform WeaponTransform(FVector(1750, -600, 55));
    auto* Weapon = GetWorld()->SpawnActorDeferred<AARLoot>(AARLoot::StaticClass(), WeaponTransform);
    if (Weapon)
    {
        Weapon->ItemID = Player->SelectedClass == EARPlayerClass::Mage ? TEXT("OakStaff") : TEXT("IronSword");
        Weapon->Quantity = 1;
        UGameplayStatics::FinishSpawningActor(Weapon, WeaponTransform);
    }
}
AActor* UARDungeonEditorLibrary::CreateNavigationBounds(UObject* WorldContext, FVector Center, FVector Extent)
{
#if WITH_EDITOR
    if (!WorldContext || !WorldContext->GetWorld() || Extent.GetMin() <= 0) return nullptr;
    UWorld* World = WorldContext->GetWorld();
    auto* Bounds = World->SpawnActor<ANavMeshBoundsVolume>(ANavMeshBoundsVolume::StaticClass(), Center, FRotator::ZeroRotator);
    if (!Bounds) return nullptr;
    if (!ResizeNavigationBounds(Bounds, Center, Extent)) { Bounds->Destroy(); return nullptr; }
    return Bounds;
#else
    return nullptr;
#endif
}
bool UARDungeonEditorLibrary::ResizeNavigationBounds(AActor* Actor, FVector Center, FVector Extent)
{
#if WITH_EDITOR
    auto* Bounds = Cast<ANavMeshBoundsVolume>(Actor);
    if (!Bounds || Extent.GetMin() <= 0) return false;
    UWorld* World = Bounds->GetWorld();
    Bounds->Modify();
    if (!Bounds->Brush)
    {
        Bounds->Brush = NewObject<UModel>(Bounds, NAME_None, RF_Transactional);
        Bounds->Brush->Initialize(Bounds, true);
    }
    Bounds->GetBrushComponent()->Brush = Bounds->Brush;
    auto* Builder = NewObject<UCubeBuilder>(Bounds);
    Builder->X = Extent.X * 2; Builder->Y = Extent.Y * 2; Builder->Z = Extent.Z * 2;
    Bounds->BrushBuilder = Builder;
    if (!Builder->Build(World, Bounds)) return false;
    Bounds->GetBrushComponent()->BuildSimpleBrushCollision();
    Bounds->SetActorLocation(Center);
    Bounds->SetActorHiddenInGame(true);
    Bounds->ReregisterAllComponents();
    Bounds->GetBrushComponent()->UpdateBounds();
    if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)) Nav->OnNavigationBoundsUpdated(Bounds);
    return Bounds->GetComponentsBoundingBox(true).GetExtent().GetMin() > 0;
#else
    return false;
#endif
}
