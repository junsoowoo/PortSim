#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortEquipmentActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(Abstract)
class PORTSIM_API APortEquipmentActor : public AActor
{
    GENERATED_BODY()
public:
    APortEquipmentActor();
protected:
    UStaticMeshComponent* Box(const TCHAR* Name, USceneComponent* Parent, FVector Position, FVector Size, bool Collision=false);
    UTextRenderComponent* Label(const TCHAR* Name, FVector Position);
};
