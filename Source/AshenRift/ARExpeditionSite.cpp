#include "ARExpeditionSite.h"
#include "ARBranchWing.h"
#include "ARPrototype.h"
#include "ARRunGameInstance.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Materials/MaterialInterface.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    const TCHAR* TownMap = TEXT("/Game/AshenRift/Maps/Map_Hub");
    const TCHAR* DungeonMaps[] = {
        TEXT("/Game/AshenRift/Maps/Map_Dungeon1"),
        TEXT("/Game/AshenRift/Maps/Map_Dungeon2"),
        TEXT("/Game/AshenRift/Maps/Map_Dungeon3")
    };
}

int32 ARExpedition::GoldPercent(int32 Tier)
{
    return 100 + 30 * FMath::Max(0, Tier - 1);
}

int32 ARExpedition::EquipmentChance(int32 Tier)
{
    return 25 + 20 * FMath::Max(0, Tier - 1);
}

int32 ARExpedition::ChooseDungeon(int32 Seed, int32 MapCount)
{
    return MapCount > 0 ? FRandomStream(Seed ^ 0x4fa2c3).RandRange(0, MapCount - 1) : INDEX_NONE;
}

TArray<EARExpeditionEnemy> ARExpedition::MakeEncounters(int32 Count, int32 Seed, int32 Tier)
{
    TArray<EARExpeditionEnemy> Plan;
    Plan.Init(EARExpeditionEnemy::Melee, Count);
    if (Count < 2) return Plan;
    if (Tier == 0)
    {
        Plan[1] = EARExpeditionEnemy::Ranged;
        if (Count > 2) Plan[2] = EARExpeditionEnemy::Elite;
        return Plan;
    }
    Plan[0] = EARExpeditionEnemy::Ranged;
    for (int32 Index = 1; Index <= FMath::Min(Count - 1, Tier - 1); ++Index)
        Plan[Index] = EARExpeditionEnemy::Elite;
    FRandomStream Random(Seed ^ 0x3bb89a);
    for (int32 Index = Plan.Num() - 1; Index > 0; --Index)
        Plan.Swap(Index, Random.RandRange(0, Index));
    return Plan;
}

FName ARExpedition::ChooseEquipment(FRandomStream& Random, EARPlayerClass PlayerClass)
{
    const int32 Roll = Random.RandRange(0, 2);
    if (Roll == 0) return TEXT("LeatherArmor");
    if (Roll == 1) return TEXT("RunicCharm");
    return PlayerClass == EARPlayerClass::Mage ? TEXT("OakStaff") : TEXT("IronSword");
}

AARExpeditionSite::AARExpeditionSite()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Site"));
}

AARExpeditionPassage::AARExpeditionPassage()
{
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Pad(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Mesh->SetStaticMesh(Pad.Object);
    Mesh->SetWorldScale3D(FVector(2.55f, 2.55f, .08f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCanEverAffectNavigation(false);
}

bool AARExpeditionPassage::CanUse(const AARPlayerCharacter* Player) const
{
    if (!IsValid(Player) || Destination == this ||
        (!IsValid(Destination) && (!IsValid(BranchWing) || !BranchWing->IsPrepared()))) return false;
    const FVector Delta = Player->GetActorLocation() - GetActorLocation();
    return Delta.SizeSquared2D() <= FMath::Square(175.f) && FMath::Abs(Delta.Z) < 180.f;
}

bool AARExpeditionPassage::Use(AARPlayerCharacter* Player)
{
    if (!CanUse(Player)) return false;
    if (!IsValid(Destination) && (!BranchWing->Build(Player) || !IsValid(Destination))) return false;
    const FVector Arrival = Destination->GetActorTransform().TransformPositionNoScale(Destination->ArrivalOffset);
    const float Yaw = Destination->GetActorRotation().Yaw;
    if (!Player->TeleportTo(Arrival, FRotator(0, Yaw, 0))) return false;
    Player->GetCharacterMovement()->StopMovementImmediately();
    if (auto* Controller = Player->GetController()) Controller->SetControlRotation(FRotator(-15, Yaw, 0));
    return true;
}

AARExpeditionRouteBoard::AARExpeditionRouteBoard()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    auto* Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RouteMarker"));
    Marker->SetupAttachment(RootComponent);
    Marker->SetRelativeLocation(FVector(0, 0, 62));
    Marker->SetRelativeScale3D(FVector(.25f, 1.35f, 1.24f));
    Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Marker->SetCanEverAffectNavigation(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Border(TEXT("/Game/AshenRift/Materials/M_Trader.M_Trader"));
    Marker->SetStaticMesh(Cube.Object);
    Marker->SetMaterial(0, Border.Object);

    auto* Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RouteSign"));
    Sign->SetupAttachment(RootComponent);
    Sign->SetRelativeLocation(FVector(13.5f, 0, 87));
    Sign->SetText(FText::FromString(TEXT("ROUTES")));
    Sign->SetHorizontalAlignment(EHTA_Center);
    Sign->SetWorldSize(21.f);
    Sign->SetTextRenderColor(FColor(255, 222, 152));
}

void AARExpeditionRouteBoard::InitializeRun(int32 Seed, EARPlayerClass PlayerClass)
{
    if (bReady || !IsValid(ShopPassage) || !IsValid(EntrancePortal) || EliteOffsets.Num() == 0) return;
    if (IsValid(BranchWing)) BranchWing->Prepare(Seed, PlayerClass);
    FRandomStream Random(Seed ^ 0x5641c9);
    const FTransform Transform = GetActorTransform();
    ElitePosition = Transform.TransformPositionNoScale(EliteOffsets[Random.RandRange(0, EliteOffsets.Num() - 1)]);
    RewardItem = ARExpedition::ChooseEquipment(Random, PlayerClass);
    // Include the entrance as one candidate. It always remains available.
    const int32 Exit = Random.RandRange(0, ExtractionOffsets.Num());
    if (Exit > 0)
    {
        const FVector Position = Transform.TransformPositionNoScale(ExtractionOffsets[Exit - 1]);
        RoutePortal = GetWorld()->SpawnActor<AARPortal>(AARPortal::StaticClass(), Position, FRotator::ZeroRotator);
        if (RoutePortal && EntrancePortal->Mesh) RoutePortal->Mesh->SetMaterial(0, EntrancePortal->Mesh->GetMaterial(0));
    }
    bReady = true;
}

bool AARExpeditionRouteBoard::CanOpen(const AARPlayerCharacter* Player) const
{
    if (!bReady || !IsValid(Player) || !Player->CanPlay()) return false;
    const auto* State = GetWorld()->GetGameInstance<UARRunGameInstance>();
    const FVector Offset = Player->GetActorLocation() - GetActorLocation();
    return State && State->Phase == EARRunPhase::InDungeon &&
        Offset.SizeSquared2D() <= FMath::Square(265.f) && FMath::Abs(Offset.Z) < 180.f;
}

bool AARExpeditionRouteBoard::Choose(EARExpeditionRoute Route, AARPlayerCharacter* Player)
{
    if (!CanOpen(Player)) return false;
    if (Route == EARExpeditionRoute::Supplies || Route == EARExpeditionRoute::Extract) return true;
    if (Route != EARExpeditionRoute::Elite) return false;
    if (bEliteStarted) return true;
    auto* Mode = GetWorld()->GetAuthGameMode<AARPrototypeGameMode>();
    Elite = Mode ? Mode->SpawnRouteElite(Player, ElitePosition, RewardItem) : nullptr;
    bEliteStarted = Elite.IsValid();
    return bEliteStarted;
}

FVector AARExpeditionRouteBoard::Target(EARExpeditionRoute Route) const
{
    if (Route == EARExpeditionRoute::Elite) return Elite.IsValid() ? Elite->GetActorLocation() : ElitePosition;
    if (Route == EARExpeditionRoute::Supplies && IsValid(ShopPassage)) return ShopPassage->GetActorLocation();
    if (Route == EARExpeditionRoute::Extract && IsValid(EntrancePortal))
        return IsValid(RoutePortal) ? RoutePortal->GetActorLocation() : EntrancePortal->GetActorLocation();
    return GetActorLocation();
}

void AARPrototypeGameMode::StartPlay()
{
    for (TActorIterator<AARExpeditionSite> It(GetWorld()); It; ++It)
    {
        ExpeditionSite = *It;
        break;
    }
    Super::StartPlay();
    if (!ExpeditionSite) return;

    auto* State = GetGameInstance<UARRunGameInstance>();
    if (!State) return;
    if (IsExpeditionTown()) State->PendingExpedition = NAME_None;
    else GetWorldTimerManager().SetTimerForNextTick(this, &AARPrototypeGameMode::BeginAuthoredDungeon);
}

bool AARPrototypeGameMode::IsExpeditionTown() const
{
    return ExpeditionSite && ExpeditionSite->bTown;
}

FVector AARPrototypeGameMode::CampLocation() const
{
    return ExpeditionSite ? ExpeditionSite->GetActorLocation() : FVector(-700, 0, 100);
}

float AARPrototypeGameMode::CampYaw() const
{
    return ExpeditionSite ? ExpeditionSite->GetActorRotation().Yaw : 0.f;
}

bool AARPrototypeGameMode::DepartTown(AARPlayerCharacter* Player, int32 Destination)
{
    auto* State = GetGameInstance<UARRunGameInstance>();
    const int32 MapCount = UE_ARRAY_COUNT(DungeonMaps);
    const bool bRandom = Destination == INDEX_NONE;
    if (!IsExpeditionTown() || !IsValid(Player) || !Player->IsAtCamp() || !State || !State->Profile ||
        State->Phase != EARRunPhase::Hub || !State->PendingExpedition.IsNone() ||
        (!bRandom && (Destination < 0 || Destination >= MapCount))) return false;

    int32 Seed = FMath::Rand();
#if !UE_BUILD_SHIPPING
    if (bRandom && State->GetProfileSlot().StartsWith(TEXT("AshenRift_Automation_")))
        FParse::Value(FCommandLine::Get(), TEXT("ARExpeditionSeed="), Seed);
#endif
    if (bRandom) Destination = ARExpedition::ChooseDungeon(Seed, MapCount);
    const TCHAR* Map = DungeonMaps[Destination];
    if (!FPackageName::DoesPackageExist(Map))
    {
        Player->ShowNotice(TEXT("Dungeon map is unavailable"));
        return false;
    }
    State->PendingExpedition = FName(Map);
    State->PendingExpeditionSeed = Seed;
    State->PendingExpeditionTier = bRandom ? State->SelectedExpeditionTier() : 0;
    State->TravelClass = Player->SelectedClass;
    State->bHasDepartedTown = true;
    UGameplayStatics::OpenLevel(this, State->PendingExpedition);
    return true;
}

void AARPrototypeGameMode::BeginAuthoredDungeon()
{
    auto* State = GetGameInstance<UARRunGameInstance>();
    auto* Player = Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!State || State->Phase != EARRunPhase::Hub || !Player) return;

    // The destination pawn exists before BeginRun attempts the stash debit.
    // A direct editor launch also starts a run at the authored PlayerStart.
    const bool bFromTown = !State->PendingExpedition.IsNone() &&
        GetWorld()->GetMapName().EndsWith(FPackageName::GetShortName(State->PendingExpedition));
    const int32 Seed = bFromTown ? State->PendingExpeditionSeed : FMath::Rand();
    const int32 Tier = bFromTown ? State->PendingExpeditionTier : 0;
    State->PendingExpedition = NAME_None;
    if (GetWorld()->GetStreamingLevels().Num() > 0) GetWorld()->FlushLevelStreaming();
    if (auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())) Navigation->Build();
    if (!EnterDungeon(Player, Seed, Tier))
    {
        if (State->LastError.IsEmpty()) State->LastError = TEXT("Could not start the dungeon run");
        ReturnFromExpedition(Player);
    }
}

bool AARPrototypeGameMode::ReturnFromExpedition(AARPlayerCharacter* Player)
{
    if (!ExpeditionSite || ExpeditionSite->bTown || !FPackageName::DoesPackageExist(TownMap)) return false;
    if (auto* State = GetGameInstance<UARRunGameInstance>())
    {
        State->PendingExpedition = NAME_None;
        State->bHasDepartedTown = true;
        State->TravelClass = Player->SelectedClass;
    }
    UGameplayStatics::OpenLevel(this, FName(TownMap));
    return true;
}
