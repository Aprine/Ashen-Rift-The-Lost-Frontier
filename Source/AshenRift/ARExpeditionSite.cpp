#include "ARExpeditionSite.h"
#include "ARPrototype.h"
#include "ARRunGameInstance.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
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
    if (!IsValid(Player) || !IsValid(Destination) || Destination == this) return false;
    const FVector Delta = Player->GetActorLocation() - GetActorLocation();
    return Delta.SizeSquared2D() <= FMath::Square(175.f) && FMath::Abs(Delta.Z) < 180.f;
}

bool AARExpeditionPassage::Use(AARPlayerCharacter* Player) const
{
    if (!CanUse(Player)) return false;
    const FVector Arrival = Destination->GetActorTransform().TransformPositionNoScale(Destination->ArrivalOffset);
    const float Yaw = Destination->GetActorRotation().Yaw;
    if (!Player->TeleportTo(Arrival, FRotator(0, Yaw, 0))) return false;
    Player->GetCharacterMovement()->StopMovementImmediately();
    if (auto* Controller = Player->GetController()) Controller->SetControlRotation(FRotator(-15, Yaw, 0));
    return true;
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
    if (!IsExpeditionTown() || !IsValid(Player) || !Player->IsAtCamp() || !State || !State->Profile ||
        !State->PendingExpedition.IsNone() || Destination < 0 || Destination >= UE_ARRAY_COUNT(DungeonMaps)) return false;

    const TCHAR* Map = DungeonMaps[Destination];
    if (!FPackageName::DoesPackageExist(Map))
    {
        Player->ShowNotice(TEXT("Dungeon map is unavailable"));
        return false;
    }
    State->PendingExpedition = FName(Map);
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
    State->PendingExpedition = NAME_None;
    if (GetWorld()->GetStreamingLevels().Num() > 0) GetWorld()->FlushLevelStreaming();
    if (auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())) Navigation->Build();
    if (!EnterDungeon(Player))
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
