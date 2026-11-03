#include "ARPrototype.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

bool AAREnemy::CanPursue(AActor* Target)
{
    const float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
    if (!bNavigationEnabled) return Distance <= DetectRange && ClearSight(Target);
    if (!bHasTarget && Distance <= DetectRange && (ClearSight(Target) || Distance <= 650)) bHasTarget = true;
    if (Distance > DetectRange * 1.6f)
    {
        bHasTarget = false;
        PathPoints.Empty();
        NavigationPointCount = 0;
    }
    if (!bHasTarget) GetCharacterMovement()->StopMovementImmediately();
    return bHasTarget;
}
void AAREnemy::Pursue(AActor* Target)
{
    if (!bNavigationEnabled) { AddMovementInput((Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), 1, true); return; }
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextPathAt)
    {
        NextPathAt = Now + .45f;
        auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, GetActorLocation(), Target->GetActorLocation(), this);
        PathPoints.Empty();
        if (Path && Path->IsValid() && !Path->IsPartial()) PathPoints = Path->PathPoints;
        NavigationPointCount = PathPoints.Num();
        PathIndex = 1;
    }
    while (PathIndex < PathPoints.Num() && FVector::Dist2D(GetActorLocation(), PathPoints[PathIndex]) < 15) ++PathIndex;
    if (PathIndex >= PathPoints.Num()) { GetCharacterMovement()->StopMovementImmediately(); return; }
    const FVector Direction = (PathPoints[PathIndex] - GetActorLocation()).GetSafeNormal2D();
    SetActorRotation(FRotator(0, Direction.Rotation().Yaw, 0));
    AddMovementInput(Direction, 1, true);
}
