#pragma once

#include "CoreMinimal.h"
#include "PortEquipmentActor.h"
#include "PortAGVActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(Blueprintable)
class PORTSIM_API APortAGVActor : public APortEquipmentActor
{
    GENERATED_BODY()
public:
    APortAGVActor();
    void InitializeVehicle(int32 Number);
    void ResetVehicle(FVector Position);
    bool MoveToX(float X, float Dt);
    bool MoveToPosition(FVector Target, float Dt);
    FVector CargoPosition() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AGV") int32 VehicleID=1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AGV") float Speed=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AGV") int32 CompletedJobs=0;
private:
    UPROPERTY() TObjectPtr<UTextRenderComponent> NameLabel;
};
