#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "STSOperatingProfile.h"
#include "PortSiteLogistics.generated.h"

class APortWorkingCrane;
class APortAGVActor;
class APortContainerActor;
class UHierarchicalInstancedStaticMeshComponent;

struct FSiteYardSlot
{
    FVector Position=FVector::ZeroVector;
    int32 Block=0, Half=0, Color=0, Instance=INDEX_NONE;
    bool Reserved=false, Occupied=true;
    TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Mesh;
};
struct FSiteShipCargo
{
    FTransform Transform;
    TWeakObjectPtr<APortContainerActor> Actor;
    int32 STS=0, ID=0, State=0; // same actor: 0 aboard, 1 transfer, 2 yard
    uint8 HandoverMask=0; // STS->AGV, AGV->RMG, RMG->yard
};
struct FSiteTransfer
{
    int32 Cargo=INDEX_NONE, Slot=INDEX_NONE, RMG=INDEX_NONE, Stage=0, Waypoint=0;
    double Time=0, StartedAt=0, PausedSeconds=0, HandoverAt=-1;
    TWeakObjectPtr<APortContainerActor> Actor;
    TArray<FVector> Route;
};

/** One manifest, conserved cargo IDs and reserved yard slots across STS -> AGV -> RMG. */
UCLASS()
class PORTSIM_API APortSiteLogistics : public AActor
{
    GENERATED_BODY()
public:
    APortSiteLogistics();
    void SetSTSProfile(const FSTSOperatingProfile& Profile) { STSProfile=Profile; }
    void AddShipCargo(FVector Position,int32 STS);
    void Initialize(const TArray<TObjectPtr<APortWorkingCrane>>& Cranes,TArray<FSiteYardSlot> Slots,
        const TArray<UHierarchicalInstancedStaticMeshComponent*>& Palette,int32 CentralCargo,int32 FixedYard);
    void Advance(float Dt,bool Paused);
    void ResetLogistics();
    bool Validate(FString& Error) const;
    int32 ShipRemaining() const;
    int32 InTransit() const;
    bool IsIdle() const;
    TArray<FVector> Snapshot() const;
    int32 InitialShipCount() const { return Manifest.Num()+CentralCount; }
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 BaselineYard=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 InitialYard=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Delivered=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 DispatchLimit=MAX_int32;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString Fault;
    UPROPERTY() TArray<TObjectPtr<APortAGVActor>> Vehicles;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<APortContainerActor>> ShipContainers;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<APortContainerActor>> PlacedContainers;
    UPROPERTY() TArray<TObjectPtr<APortWorkingCrane>> Equipment;
private:
    FSTSOperatingProfile STSProfile;
    double SimulationTime=0;
    FString ReportBase, ResultsCsv;
    bool SaveReports() const;
    void BeginReport();
    TArray<FSiteShipCargo> Manifest;
    TArray<FSiteYardSlot> Yard;
    TArray<FSiteTransfer> Jobs;
    TArray<bool> BlocksBusy;
    TArray<bool> SlotAssigned;
    int32 CorridorOwner=INDEX_NONE, NextRMG=0, CentralCount=0, Dispatched=0;
    bool bReady=false, bWasPaused=false;
    void Dispatch(int32 Lane);
    void PrepareRoute(int32 Lane,bool Return);
    bool Drive(int32 Lane,float Dt);
    void Freeze(bool Paused);
    void Stop(const FString& Reason);
    FVector QuayPark(int32 Lane) const;
    FVector YardHandover(const FSiteYardSlot& Slot) const;
};
