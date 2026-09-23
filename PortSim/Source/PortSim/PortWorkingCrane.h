#pragma once

#include "CoreMinimal.h"
#include "PortEquipmentActor.h"
#include "PortWorkingCrane.generated.h"

class APortContainerActor;
class UTextRenderComponent;

/** Independently reserved two-slot crane job. Gantry/trolley/hoist are driven;
 * cargo is locked to the spreader during transport and physically released. */
UCLASS(Blueprintable)
class PORTSIM_API APortWorkingCrane : public APortEquipmentActor
{
    GENERATED_BODY()
public:
    APortWorkingCrane();
    void Configure(int32 Number, bool bQuayside, FVector Source, FVector Destination, bool bCreateCargo=true);
    void Advance(float Dt, bool bGlobalPaused);
    bool AssignCargo(APortContainerActor* Cargo, FVector Source, FVector Destination, bool SourceSupport, bool DestinationSupport);
    bool IsBusy() const { return bJobActive; }
    UFUNCTION(BlueprintCallable, Category="Operation") void ResetOperation();
    UFUNCTION(BlueprintCallable, Category="Operation") void SetOperationPaused(bool Paused);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    bool ValidateOperation(FString& Error) const;
    FVector HeadPosition() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Operation") bool bEnabled=true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Operation") bool bSTS=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Operation") bool bCarrying=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Operation") int32 CraneID=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Operation") int32 CompletedJobs=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Operation") int32 Stage=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Operation") FString Fault;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Operation") TObjectPtr<APortContainerActor> CargoActor;
private:
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Legs;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Beams;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Bogies;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> CrossBeams;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Ropes;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Pads;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Trolley;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Spreader;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mast;
    UPROPERTY() TObjectPtr<UTextRenderComponent> NameLabel;
    FVector Home=FVector::ZeroVector;
    FQuat Orientation=FQuat::Identity;
    FVector Slots[2];
    FVector Head=FVector::ZeroVector;
    FVector PausedVelocity=FVector::ZeroVector;
    FVector PausedAngularVelocity=FVector::ZeroVector;
    float BeamZ=2300.f;
    float SafeZ=1900.f;
    float Speed=0.f;
    float SettleTime=0.f;
    float StageTime=0.f;
    int32 SourceSlot=0;
    bool bPaused=false;
    bool bResumePhysics=false;
    bool bConfigured=false;
    bool bExternalJobs=false;
    bool bJobActive=false;
    FVector JobStartHead=FVector::ZeroVector;
    FVector Local(FVector World) const;
    bool MoveHead(FVector Target, float Dt);
    void UpdateParts();
    bool DestinationClear() const;
    void Stop(const FString& Reason);
};
