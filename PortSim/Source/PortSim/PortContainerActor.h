#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortContainerActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ECargoOwner : uint8 { Ship, STS, AGV, RMG, Yard };

UCLASS(Blueprintable)
class PORTSIM_API APortContainerActor : public AActor
{
    GENERATED_BODY()
public:
    APortContainerActor();
    virtual void BeginPlay() override;
    void InitializeContainer(int32 Number);
    void ResetCargo(FVector Position);
    void ApplyContainerAppearance();
    UStaticMeshComponent* GetBody() const { return Body; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Container") FName ContainerID;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Container") ECargoOwner LocationOwner=ECargoOwner::Ship;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Container") TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Container") TObjectPtr<UStaticMeshComponent> Visual;
};
