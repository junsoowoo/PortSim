#include "QuayCrane.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

APortContainerActor* AQuayCrane::SpawnContainer(int32 Number,FVector Position)
{
    FActorSpawnParameters Params;
    Params.Owner=this; // Lifetime ownership only; no transform attachment to the STS.
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Container=GetWorld()->SpawnActor<APortContainerActor>(Position,FRotator::ZeroRotator,Params);
    check(Container);
    Container->InitializeContainer(Number);
    ContainerActors.Add(Container);
    return Container;
}

bool AQuayCrane::ValidateTerminalActors(FString& Error) const
{
    const int32 Expected=bTerminalMode?24:1;
    if (ContainerActors.Num()!=Expected) { Error=TEXT("Container actor count mismatch"); return false; }
    TSet<FName> IDs;
    TSet<const AActor*> Actors;
    for (int32 I=0;I<ContainerActors.Num();++I)
    {
        const auto* Container=ContainerActors[I].Get();
        if (!IsValid(Container) || Container->GetOwner()!=this || Container->GetAttachParentActor() ||
            Container->GetBody()->GetOwner()!=Container || Container->GetRootComponent()!=Container->GetBody() ||
            Container->Visual->GetOwner()!=Container || Container->ContainerID.IsNone() ||
            IDs.Contains(Container->ContainerID) || Actors.Contains(Container) ||
            !Container->GetActorLocation().Equals(Container->GetBody()->GetComponentLocation(),.01f))
        { Error=TEXT("Container ID, actor transform or component ownership mismatch"); return false; }
        if (bTerminalMode && (!CargoBodies.IsValidIndex(I) || CargoBodies[I]!=Container->GetBody()))
        { Error=TEXT("Container manifest references the wrong actor"); return false; }
        IDs.Add(Container->ContainerID); Actors.Add(Container);
    }
    // Verify there are no orphan/duplicate container actors after reset.
    int32 SpawnedContainers=0;
    for (TActorIterator<APortContainerActor> It(GetWorld());It;++It)
        if (It->GetOwner()==this) ++SpawnedContainers;
    if (SpawnedContainers!=Expected) { Error=TEXT("Duplicate spawned container actor"); return false; }
    if (!bTerminalMode) return true;
    if (AGVActors.Num()!=3 || !IsValid(RMGActor) || !IsValid(ShipActor) ||
        RMGSpreader->GetOwner()!=RMGActor || RMGActor->GetOwner()!=this || ShipActor->GetOwner()!=this)
    { Error=TEXT("Fleet/ship actor ownership mismatch"); return false; }
    TArray<const AActor*> Equipment={RMGActor.Get(),ShipActor.Get()};
    for (int32 I=0;I<AGVActors.Num();++I)
    {
        if (!IsValid(AGVActors[I]) || AGVActors[I]->VehicleID!=I+1)
        { Error=TEXT("AGV actor/ID mismatch"); return false; }
        Equipment.Add(AGVActors[I]);
    }
    for (const auto* Actor:Equipment)
    {
        if (Actor->GetOwner()!=this || Actor->GetAttachParentActor() || Actors.Contains(Actor))
        { Error=TEXT("Equipment is attached to STS or duplicated"); return false; }
        TInlineComponentArray<UStaticMeshComponent*> Parts(Actor);
        for (const auto* Part:Parts)
            if (Part->GetOwner()!=Actor) { Error=TEXT("Equipment component belongs to another actor"); return false; }
        Actors.Add(Actor);
    }
    return true;
}

void AQuayCrane::DestroyTerminalActors()
{
    // Constraints span actors; release them before destroying their physics bodies.
    if (Suspension) Suspension->BreakConstraint();
    if (TwistLock) TwistLock->BreakConstraint();
    auto Destroy=[](AActor* Actor) { if (IsValid(Actor) && !Actor->IsActorBeingDestroyed()) Actor->Destroy(); };
    for (const auto& Container:ContainerActors) Destroy(Container);
    for (const auto& Vehicle:AGVActors) Destroy(Vehicle);
    Destroy(RMGActor); Destroy(ShipActor);
    ContainerActors.Reset(); CargoBodies.Reset(); AGVActors.Reset();
    RMGActor=nullptr; RMGSpreader=nullptr; ShipActor=nullptr; Cargo=nullptr;
}
