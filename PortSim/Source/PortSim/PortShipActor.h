#pragma once

#include "CoreMinimal.h"
#include "PortEquipmentActor.h"
#include "PortShipActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(Blueprintable)
class PORTSIM_API APortShipActor : public APortEquipmentActor
{
    GENERATED_BODY()
public:
    APortShipActor();
};
