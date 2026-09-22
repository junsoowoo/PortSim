#include "QuayCrane.h"
#include "Components/StaticMeshComponent.h"
#include "TerminalLayout.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"

namespace Fleet
{
    constexpr float DeckTop = 220.f;
    constexpr float CargoOffset = DeckTop + 129.5f;
    constexpr float LiftOffset = 154.5f;
    constexpr float SafeZ = 1900.f;
    constexpr float QuayX = 3000.f;
    constexpr float YardX = 6000.f;
    constexpr float ParkX = 4500.f;
}

void AQuayCrane::BuildFleet()
{
    auto Box = [this](const FString& Name, USceneComponent* Parent, FVector P, FVector Size, bool Collision)
    {
        auto* C = NewObject<UStaticMeshComponent>(this, FName(*Name));
        AddInstanceComponent(C); C->SetupAttachment(Parent);
        C->SetStaticMesh(TrolleyMesh->GetStaticMesh()); C->SetMobility(EComponentMobility::Movable);
        C->SetRelativeLocation(P); C->SetRelativeScale3D(Size/100.f);
        C->SetCollisionProfileName(Collision ? TEXT("BlockAllDynamic") : TEXT("NoCollision"));
        C->RegisterComponent(); return C;
    };
    FActorSpawnParameters Params;
    Params.Owner=this;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (int32 I=0; I<3; ++I)
    {
        auto* Vehicle=GetWorld()->SpawnActor<APortAGVActor>(FVector(Fleet::ParkX,-2400+2400*I,0),FRotator::ZeroRotator,Params);
        check(Vehicle);
        Vehicle->InitializeVehicle(I+1);
        AGVActors.Add(Vehicle);
        const float Y=-2400.f+I*2400.f;
        Box(FString::Printf(TEXT("Road_AGV_%d"),I),RootComponent,FVector(4500,Y,22),FVector(3800,1500,3),false);
        for (int32 Dash=0;Dash<8;++Dash)
            Box(FString::Printf(TEXT("RoadMark_%d_%d"),I,Dash),RootComponent,FVector(2800+Dash*470,Y+760,25),FVector(220,20,3),false);
    }
    for (int32 Side:{-1,1})
    {
        const float X=Side<0?TerminalLayout::NearRailX:TerminalLayout::FarRailX;
        Box(FString::Printf(TEXT("Rail_RMG_%d"),Side),RootComponent,FVector(X,0,30),FVector(35,TerminalLayout::RailLength,20),false);
    }
    RMGActor=GetWorld()->SpawnActor<APortRMGActor>(FVector(TerminalLayout::BridgeCenterX,0,0),FRotator::ZeroRotator,Params);
    check(RMGActor);
    RMGSpreader=RMGActor->GetSpreader();
#if WITH_EDITOR
    RMGActor->SetActorLabel(TEXT("RMG_01"));
    RMGActor->SetFolderPath(TEXT("PortSim/Equipment"));
#endif
    CameraArm->TargetArmLength=30000.f;
    CameraArm->SetRelativeLocation(FVector(5500,0,500));
    CameraArm->SetRelativeRotation(FRotator(-52,-38,0));
    ResetFleet();
}

FVector AQuayCrane::AGVCargoPosition() const
{
    return AGVActors[ActiveAGV]->CargoPosition();
}

void AQuayCrane::ResetFleet()
{
    ActiveAGV=0; FleetStep=0; RMGStep=0;
    bAGVHasCargo=false; bRMGHasCargo=false; FleetSettle=0;
    for (int32 I=0;I<AGVActors.Num();++I)
        AGVActors[I]->ResetVehicle(FVector(Fleet::ParkX,-2400+2400*I,0));
    RMGActor->ResetCrane();
}

bool AQuayCrane::MoveAGV(float X,float Dt)
{
    const bool Arrived=AGVActors[ActiveAGV]->MoveToX(X,Dt);
    if (bAGVHasCargo) Cargo->SetWorldLocationAndRotation(AGVCargoPosition(),FRotator::ZeroRotator);
    return Arrived;
}

void AQuayCrane::HoldFleetCargo(bool OnAGV)
{
    Cargo->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Cargo->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    Cargo->SetSimulatePhysics(false);
    bAGVHasCargo=OnAGV; bRMGHasCargo=!OnAGV;
    ContainerActors[ActiveCargoIndex]->LocationOwner=OnAGV?ECargoOwner::AGV:ECargoOwner::RMG;
}

void AQuayCrane::ReleaseFleetCargo()
{
    bAGVHasCargo=false; bRMGHasCargo=false;
    Cargo->SetSimulatePhysics(true);
    Cargo->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Cargo->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    Cargo->WakeAllRigidBodies();
}

bool AQuayCrane::TickRMGTransfer(FVector Source,FVector Destination,float Dt)
{
    FVector Target=RMGSpreader->GetComponentLocation();
    switch(RMGStep)
    {
    case 0: Target.Z=Fleet::SafeZ; break;
    case 1: Target=FVector(Source.X,Source.Y,Fleet::SafeZ); break;
    case 2: Target=Source+FVector(0,0,Fleet::LiftOffset); break;
    case 3: Target=FVector(Source.X,Source.Y,Fleet::SafeZ); break;
    case 4: Target=FVector(Destination.X,Destination.Y,Fleet::SafeZ); break;
    case 5: Target=Destination+FVector(0,0,Fleet::LiftOffset); break;
    case 6: Target=FVector(Destination.X,Destination.Y,Fleet::SafeZ); break;
    default: return true;
    }
    const bool Arrived=RMGActor->MoveSpreaderTo(Target,Dt);
    if (bRMGHasCargo) Cargo->SetWorldLocationAndRotation(RMGSpreader->GetComponentLocation()-FVector(0,0,Fleet::LiftOffset),FRotator::ZeroRotator);
    if (!Arrived) return false;
    if (RMGStep==2)
    {
        if (bLocked || AGVActors[ActiveAGV]->Speed>0 || FVector::Dist(Cargo->GetComponentLocation(),Source)>20.f || Cargo->GetUpVector().Z<.99f)
        { StopAutomatic(TEXT("RMG pickup interlock: cargo alignment / AGV stop.")); return false; }
        HoldFleetCargo(false);
    }
    if (RMGStep==5)
    {
        if (!bAutoLoading && !IsTerminalSlotAvailable(ActiveCargoIndex,false))
        { StopAutomatic(TEXT("RMG destination slot occupied.")); return false; }
        if (bAutoLoading) { bRMGHasCargo=false; bAGVHasCargo=true; ContainerActors[ActiveCargoIndex]->LocationOwner=ECargoOwner::AGV; }
        else ReleaseFleetCargo();
    }
    ++RMGStep;
    return RMGStep>6;
}

void AQuayCrane::TickFleet(float Dt)
{
    // A single job reserves the STS/AGV/RMG handover corridor until committed.
    // Three separated lanes never intersect. Vehicles are omnidirectional shuttle blockouts.
    if (AutoStage==ETerminalStage::FleetPrepare)
    {
        if (FleetStep==0)
        {
            if (!MoveAGV(bAutoLoading?Fleet::YardX:Fleet::QuayX,Dt)) return;
            if (!bAutoLoading) { SetAutoStage(ETerminalStage::RaiseEmpty); return; }
            FleetStep=1;
        }
        else if (FleetStep==1)
        {
            if (TickRMGTransfer(TerminalSlot(ActiveCargoIndex,false),AGVCargoPosition(),Dt)) FleetStep=2;
        }
        else if (FleetStep==2)
        {
            if (MoveAGV(Fleet::QuayX,Dt)) { ReleaseFleetCargo(); FleetStep=3; FleetSettle=0; }
        }
        else if (FleetStep==3)
        {
            FleetSettle+=Dt;
            if (FleetSettle>1.f && Cargo->GetPhysicsLinearVelocity().Size()<8.f)
            {
                if (FVector::Dist(Cargo->GetComponentLocation(),AGVCargoPosition())>20.f)
                { StopAutomatic(TEXT("AGV load shifted before STS pickup.")); return; }
                SetAutoStage(ETerminalStage::RaiseEmpty);
            }
        }
    }
    else if (AutoStage==ETerminalStage::FleetDeliver)
    {
        if (bAutoLoading)
        {
            if (MoveAGV(Fleet::ParkX,Dt)) CompleteAutomaticJob();
            return;
        }
        if (FleetStep==0)
        {
            if (MoveAGV(Fleet::YardX,Dt)) { FleetStep=1; RMGStep=0; }
        }
        else if (FleetStep==1)
        {
            if (TickRMGTransfer(AGVCargoPosition(),TerminalSlot(ActiveCargoIndex,false),Dt)) { FleetStep=2; FleetSettle=0; }
        }
        else if (FleetStep==2)
        {
            const FVector Destination=TerminalSlot(ActiveCargoIndex,false);
            const bool Stable=FVector::Dist(Cargo->GetComponentLocation(),Destination)<20.f && Cargo->GetPhysicsLinearVelocity().Size()<8.f && Cargo->GetUpVector().Z>.99f;
            FleetSettle=Stable?FleetSettle+Dt:0;
            if (FleetSettle>1.f) { ContainerActors[ActiveCargoIndex]->LocationOwner=ECargoOwner::Yard; FleetStep=3; }
        }
        else if (FleetStep==3 && MoveAGV(Fleet::ParkX,Dt)) CompleteAutomaticJob();
    }
}

FString AQuayCrane::GetFleetStatus() const
{
    if (AGVActors.IsEmpty()) return TEXT("");
    return FString::Printf(TEXT("AGV 1/2/3 jobs %d / %d / %d | Active AGV %d | RMG %s | step %d"),
        AGVActors[0]->CompletedJobs,AGVActors[1]->CompletedJobs,AGVActors[2]->CompletedJobs,ActiveAGV+1,bRMGHasCargo?TEXT("CARRY"):TEXT("READY"),RMGStep);
}
