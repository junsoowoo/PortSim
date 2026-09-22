#include "PortRMGActor.h"
#include "TerminalLayout.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APortRMGActor::APortRMGActor()
{
    const float Center=TerminalLayout::BridgeCenterX;
    for (int32 Side:{-1,1})
    {
        const float X=(Side<0?TerminalLayout::NearRailX:TerminalLayout::FarRailX)-Center;
        for (int32 End:{-1,1})
        {
            Box(*FString::Printf(TEXT("RMG_Leg_%d_%d"),Side,End),RootComponent,FVector(X,End*820,1330),FVector(100,100,2560),true);
            Box(*FString::Printf(TEXT("Bogie_RMG_%d_%d"),Side,End),RootComponent,FVector(X,End*820,100),FVector(160,320,160));
        }
        Box(*FString::Printf(TEXT("RMG_EndBeam_%d"),Side),RootComponent,FVector(X,0,2580),FVector(160,1800,160));
        Box(*FString::Printf(TEXT("RMG_Bridge_%d"),Side),RootComponent,FVector(0,Side*650,2600),FVector(TerminalLayout::BridgeWidth,120,180));
    }
    Trolley=Box(TEXT("RMG_Trolley"),RootComponent,FVector(6000-Center,0,2650),FVector(420,1450,120));
    Spreader=Box(TEXT("RMG_Spreader"),RootComponent,FVector(6000-Center,0,1900),FVector(244,1220,50));
    for (int32 I=0;I<4;++I)
        Ropes.Add(Box(*FString::Printf(TEXT("Rope_RMG_%d"),I),RootComponent,FVector::ZeroVector,FVector(6)));
    Label(TEXT("RMGLabel"),FVector(0,0,2750))->SetText(FText::FromString(TEXT("RMG 1")));
    Tags.Add(TEXT("PortSim.RMG"));
}

void APortRMGActor::ResetCrane()
{
    Speed=0;
    SetActorLocationAndRotation(FVector(TerminalLayout::BridgeCenterX,0,0),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
    Spreader->SetWorldLocation(FVector(6000,0,1900));
    UpdateVisuals();
}

void APortRMGActor::UpdateVisuals()
{
    const FVector P=Spreader->GetComponentLocation();
    // Move the ACTOR with rail travel, then restore the independently positioned spreader.
    SetActorLocation(FVector(TerminalLayout::BridgeCenterX,P.Y,0));
    Spreader->SetWorldLocation(P);
    Trolley->SetRelativeLocation(FVector(P.X-TerminalLayout::BridgeCenterX,0,2650));
    for (int32 I=0;I<Ropes.Num();++I)
    {
        const FVector A(P.X+(I<2?-100:100),P.Y+(I%2?-550:550),2590);
        const FVector B(A.X,A.Y,P.Z+25);
        Ropes[I]->SetWorldLocation((A+B)*.5f);
        Ropes[I]->SetWorldScale3D(FVector(.06f,.06f,(A.Z-B.Z)/100.f));
    }
}

bool APortRMGActor::MoveSpreaderTo(FVector Target,float Dt)
{
    const FVector P=Spreader->GetComponentLocation();
    const float Distance=FVector::Dist(P,Target);
    const float SpeedLimit=FMath::Abs(Target.Z-P.Z)>1.f?250.f:400.f;
    Speed=FMath::FInterpConstantTo(Speed,FMath::Min(SpeedLimit,FMath::Sqrt(2.f*200.f*Distance)),Dt,200.f);
    const FVector Next=FMath::VInterpConstantTo(P,Target,Dt,Speed);
    Spreader->SetWorldLocation(Next);
    UpdateVisuals();
    if (FVector::Dist(Next,Target)>.1f) return false;
    Speed=0;
    return true;
}

