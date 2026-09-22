#pragma once

#include "CoreMinimal.h"
#include "PortEquipmentActor.h"
#include "PortRMGActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(Blueprintable)
class PORTSIM_API APortRMGActor : public APortEquipmentActor
{
    GENERATED_BODY()
public:
    APortRMGActor();
    void ResetCrane();
    bool MoveSpreaderTo(FVector Target, float Dt);
    void UpdateVisuals();
    UStaticMeshComponent* GetSpreader() const { return Spreader; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RMG") float Speed=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RMG") TObjectPtr<UStaticMeshComponent> Trolley;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RMG") TObjectPtr<UStaticMeshComponent> Spreader;
private:
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Ropes;
};
