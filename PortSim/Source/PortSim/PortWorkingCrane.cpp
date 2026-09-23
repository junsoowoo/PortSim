#include "PortWorkingCrane.h"
#include "PortContainerActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

namespace WorkingCrane { constexpr float LiftOffset=154.5f; }

APortWorkingCrane::APortWorkingCrane()
{
    for (int32 I=0;I<4;++I)
    {
        Legs.Add(Box(*FString::Printf(TEXT("Leg_%d"),I),RootComponent,FVector::ZeroVector,FVector(100),true));
        Ropes.Add(Box(*FString::Printf(TEXT("Rope_%d"),I),RootComponent,FVector::ZeroVector,FVector(6)));
    }
    for (int32 I=0;I<2;++I)
    {
        Beams.Add(Box(*FString::Printf(TEXT("Beam_%d"),I),RootComponent,FVector::ZeroVector,FVector(100)));
        auto* Pad=Box(*FString::Printf(TEXT("Slot_%d"),I),RootComponent,FVector::ZeroVector,FVector(100),true);
        Pad->SetAbsolute(true,true,true); // Fixed supports do not follow gantry travel.
        Pads.Add(Pad);
    }
    Trolley=Box(TEXT("Trolley"),RootComponent,FVector::ZeroVector,FVector(420,1450,120));
    Spreader=Box(TEXT("Spreader"),RootComponent,FVector::ZeroVector,FVector(244,1220,50));
    Mast=Box(TEXT("Mast"),RootComponent,FVector::ZeroVector,FVector(100));
    NameLabel=Label(TEXT("CraneName"),FVector::ZeroVector);
    Tags.Add(TEXT("PortSim.WorkingCrane"));
}

void APortWorkingCrane::Configure(int32 Number,bool bQuayside,FVector Source,FVector Destination,bool bCreateCargo)
{
    check(!bConfigured);
    bExternalJobs=!bCreateCargo; CraneID=Number; bSTS=bQuayside; Home=GetActorLocation(); Orientation=GetActorQuat();
    Slots[0]=Source; Slots[1]=Destination;
    BeamZ=bSTS?5700.f:2300.f; SafeZ=bSTS?5000.f:1900.f;
    const float HalfGauge=bSTS?1500.f:1600.f;
    const float HalfBase=bSTS?1200.f:600.f;
    const float Height=bSTS?5600.f:2200.f;
    for (int32 I=0;I<4;++I)
    {
        Legs[I]->SetRelativeLocation(FVector((I<2?-1:1)*HalfGauge,(I%2?-1:1)*HalfBase,20+Height*.5f));
        Legs[I]->SetRelativeScale3D(FVector(bSTS?2.f:1.f,bSTS?2.f:1.f,Height/100.f));
    }
    for (int32 I=0;I<2;++I)
    {
        Beams[I]->SetRelativeLocation(FVector(bSTS?-1500.f:0.f,(I?-1:1)*HalfBase,BeamZ));
        Beams[I]->SetRelativeScale3D(FVector(bSTS?110.f:34.f,1.5f,1.8f));
        Pads[I]->SetWorldLocationAndRotation(Slots[I]-FVector(0,0,139.5f),Orientation);
        Pads[I]->SetWorldScale3D(FVector(3.1f,13.f,.2f));
    }
    Mast->SetRelativeLocation(FVector(0,0,BeamZ+1200));
    Mast->SetRelativeScale3D(FVector(3,3,24));
    Mast->SetVisibility(bSTS);
    NameLabel->SetRelativeLocation(FVector(0,0,BeamZ+150));
    NameLabel->SetWorldSize(170);
    NameLabel->SetText(FText::FromString(FString::Printf(TEXT("%s %02d"),bSTS?TEXT("STS"):TEXT("RMG"),CraneID)));
    auto Material=[](const TCHAR* Name) { return LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/PortSim/Assets/Materials/M_%s.M_%s"),Name,Name)); };
    for (const auto& Part:Legs) Part->SetMaterial(0,Material(bSTS?TEXT("SiteBlue"):TEXT("SiteOrange")));
    for (const auto& Part:Beams) Part->SetMaterial(0,Material(bSTS?TEXT("SiteBlue"):TEXT("SiteOrange")));
    for (const auto& Part:Ropes) Part->SetMaterial(0,Material(TEXT("SiteRoad")));
    for (const auto& Part:Pads) Part->SetMaterial(0,Material(TEXT("SiteWhite")));
    Mast->SetMaterial(0,Material(TEXT("SiteBlue")));
    Trolley->SetMaterial(0,Material(TEXT("SiteBlue")));
    Spreader->SetMaterial(0,Material(TEXT("SiteOrange")));
    if (bCreateCargo)
    {
    FActorSpawnParameters Params;
    Params.Owner=this;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    CargoActor=GetWorld()->SpawnActor<APortContainerActor>(Source,Orientation.Rotator(),Params);
    check(CargoActor);
    CargoActor->InitializeContainer(1000+Number);
    }
    bConfigured=true;
#if WITH_EDITOR
    SetActorLabel(FString::Printf(TEXT("Working_%s_%02d"),bSTS?TEXT("STS"):TEXT("RMG"),Number));
    SetFolderPath(TEXT("PortSim/WorkingEquipment"));
#endif
    ResetOperation();
}

FVector APortWorkingCrane::Local(FVector World) const { return Orientation.UnrotateVector(World-Home); }
FVector APortWorkingCrane::HeadPosition() const { return Home+Orientation.RotateVector(Head); }

void APortWorkingCrane::ResetOperation()
{
    if (!bConfigured) return;
    if (bExternalJobs)
    {
        if (IsValid(CargoActor)) CargoActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        CargoActor=nullptr; bJobActive=bCarrying=bPaused=bResumePhysics=false;
        SourceSlot=Stage=CompletedJobs=0; Fault.Empty(); Speed=StageTime=SettleTime=0;
        Head=Local(Slots[0]); Head.Z=SafeZ; JobStartHead=Head; UpdateParts();
        for (const auto& Pad:Pads) { Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision); Pad->SetVisibility(false); }
        return;
    }
    bJobActive=true;
    CargoActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    auto* Body=CargoActor->GetBody();
    Body->SetSimulatePhysics(false);
    CargoActor->SetActorLocationAndRotation(Slots[0],Orientation.Rotator(),false,nullptr,ETeleportType::TeleportPhysics);
    Body->SetSimulatePhysics(true);
    Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    CargoActor->LocationOwner=bSTS?ECargoOwner::Ship:ECargoOwner::Yard;
    SourceSlot=Stage=CompletedJobs=0;
    bCarrying=bPaused=bResumePhysics=false;
    Fault.Empty(); Speed=StageTime=0; SettleTime=0;
    Head=Local(Slots[0]); Head.Z=SafeZ;
    UpdateParts();
}

void APortWorkingCrane::SetOperationPaused(bool Paused)
{
    if (!bConfigured || bPaused==Paused) return;
    bPaused=Paused;
    if (!IsValid(CargoActor)) return;
    auto* Body=CargoActor->GetBody();
    if (Paused)
    {
        bResumePhysics=Body->IsSimulatingPhysics();
        PausedVelocity=Body->GetPhysicsLinearVelocity();
        PausedAngularVelocity=Body->GetPhysicsAngularVelocityInDegrees();
        if (bResumePhysics) Body->SetSimulatePhysics(false);
    }
    else if (bResumePhysics)
    {
        Body->SetSimulatePhysics(true);
        Body->SetPhysicsLinearVelocity(PausedVelocity);
        Body->SetPhysicsAngularVelocityInDegrees(PausedAngularVelocity);
        bResumePhysics=false;
    }
}

void APortWorkingCrane::UpdateParts()
{
    SetActorLocation(Home+Orientation.RotateVector(FVector(0,Head.Y,0)));
    Trolley->SetRelativeLocation(FVector(Head.X,0,BeamZ));
    Spreader->SetRelativeLocation(FVector(Head.X,0,Head.Z));
    for (int32 I=0;I<4;++I)
    {
        const float Bottom=Head.Z+25;
        Ropes[I]->SetRelativeLocation(FVector(Head.X+(I<2?-100:100),I%2?-520:520,(BeamZ+Bottom)*.5f));
        Ropes[I]->SetRelativeScale3D(FVector(.06f,.06f,FMath::Max(1.f,BeamZ-Bottom)/100.f));
    }
}

bool APortWorkingCrane::MoveHead(FVector Target,float Dt)
{
    const float Distance=FVector::Dist(Head,Target);
    const float Limit=FMath::Abs(Target.Z-Head.Z)>1.f?200.f:350.f;
    Speed=FMath::FInterpConstantTo(Speed,FMath::Min(Limit,FMath::Sqrt(2*150.f*Distance)),Dt,150.f);
    Head=FMath::VInterpConstantTo(Head,Target,Dt,Speed);
    UpdateParts();
    if (!Head.Equals(Target,.1f)) return false;
    Head=Target; Speed=0; UpdateParts(); return true;
}

bool APortWorkingCrane::DestinationClear() const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(WorkingCraneSlot),false);
    Query.AddIgnoredActor(CargoActor);
    // Shrink by 2 cm so the supporting pad/ground is not treated as an obstruction.
    return !GetWorld()->OverlapBlockingTestByChannel(Slots[1-SourceSlot],Orientation,ECC_WorldDynamic,
        FCollisionShape::MakeBox(FVector(120,608,127.5)),Query);
}

void APortWorkingCrane::Stop(const FString& Reason)
{
    Fault=Reason; SetOperationPaused(true);
    UE_LOG(LogTemp,Error,TEXT("WORKING_CRANE_%d: %s"),CraneID,*Reason);
}

void APortWorkingCrane::Advance(float Dt,bool bGlobalPaused)
{
    if (!bConfigured) return;
    SetOperationPaused(bGlobalPaused || !bEnabled || !Fault.IsEmpty());
    if (bPaused || !bJobActive) return;
    StageTime+=Dt;
    if (StageTime>180.f) { Stop(TEXT("Job stage timed out")); return; }
    const FVector Source=Local(Slots[SourceSlot]);
    const FVector Destination=Local(Slots[1-SourceSlot]);
    FVector Target=Head;
    switch(Stage)
    {
    case 0: Target.Z=SafeZ; break;
    case 1: Target=FVector(Source.X,Source.Y,SafeZ); break;
    case 2: Target=Source+FVector(0,0,WorkingCrane::LiftOffset); break;
    case 3: Target=FVector(Source.X,Source.Y,SafeZ); break;
    case 4: Target=FVector(Destination.X,Destination.Y,SafeZ); break;
    case 5: Target=Destination+FVector(0,0,WorkingCrane::LiftOffset); break;
    case 6: Target=FVector(Destination.X,Destination.Y,SafeZ); break;
    case 7:
        SettleTime+=Dt;
        if (SettleTime<1.5f) return;
        if (FVector::Dist(CargoActor->GetActorLocation(),Slots[1-SourceSlot])>10.f ||
            CargoActor->GetBody()->GetPhysicsLinearVelocity().Size()>5.f || CargoActor->GetActorUpVector().Z<.99f)
        { Stop(TEXT("Placed cargo failed physical settling check")); return; }
        ++CompletedJobs;
        if (bExternalJobs) { bJobActive=false; CargoActor=nullptr; Stage=0; StageTime=SettleTime=0; return; }
        SourceSlot=1-SourceSlot; Stage=0; StageTime=SettleTime=0; return;
    default: Stop(TEXT("Invalid job stage")); return;
    }
    if (!MoveHead(Target,Dt)) return;
    if (Stage==2)
    {
        if (FVector::Dist(CargoActor->GetActorLocation(),Slots[SourceSlot])>10.f ||
            CargoActor->GetActorUpVector().Z<.99f || CargoActor->GetBody()->GetPhysicsLinearVelocity().Size()>5.f)
        { Stop(TEXT("Pickup alignment/stop interlock")); return; }
        if (!DestinationClear()) { Stop(TEXT("Destination slot occupied")); return; }
        auto* Body=CargoActor->GetBody();
        Body->SetSimulatePhysics(false);
        CargoActor->SetActorLocationAndRotation(HeadPosition()-FVector(0,0,WorkingCrane::LiftOffset),Orientation.Rotator());
        CargoActor->AttachToComponent(Spreader,FAttachmentTransformRules::KeepWorldTransform);
        CargoActor->LocationOwner=bSTS?ECargoOwner::STS:ECargoOwner::RMG;
        bCarrying=true;
    }
    if (Stage==5)
    {
        if (!DestinationClear()) { Stop(TEXT("Destination became occupied")); return; }
        CargoActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        auto* Body=CargoActor->GetBody();
        Body->SetSimulatePhysics(true);
        Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
        Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        CargoActor->LocationOwner=bSTS && (1-SourceSlot)==0?ECargoOwner::Ship:ECargoOwner::Yard;
        bCarrying=false;
    }
    ++Stage; StageTime=0;
}

bool APortWorkingCrane::ValidateOperation(FString& Error) const
{
    if (bConfigured && bExternalJobs && !bJobActive && Fault.IsEmpty()) return true;
    if (!bConfigured || !IsValid(CargoActor) || (!bExternalJobs && CargoActor->GetOwner()!=this) || !Fault.IsEmpty())
    { Error=FString::Printf(TEXT("Crane %d: configuration/cargo/fault: %s"),CraneID,*Fault); return false; }
    const FVector A=Local(Slots[0]), B=Local(Slots[1]);
    if (Head.X<FMath::Min3(A.X,B.X,JobStartHead.X)-1 || Head.X>FMath::Max3(A.X,B.X,JobStartHead.X)+1 ||
        Head.Y<FMath::Min3(A.Y,B.Y,JobStartHead.Y)-1 || Head.Y>FMath::Max3(A.Y,B.Y,JobStartHead.Y)+1 || Head.Z>SafeZ+1)
    { Error=TEXT("Crane left its reserved envelope"); return false; }
    if (bCarrying && (CargoActor->GetAttachParentActor()!=this || CargoActor->GetBody()->IsSimulatingPhysics() ||
        !CargoActor->GetActorLocation().Equals(HeadPosition()-FVector(0,0,WorkingCrane::LiftOffset),1.f)))
    { Error=TEXT("Locked cargo is detached or out of alignment"); return false; }
    if (!bCarrying && CargoActor->GetAttachParentActor())
    { Error=TEXT("Released cargo remains attached"); return false; }
    return true;
}

void APortWorkingCrane::EndPlay(const EEndPlayReason::Type Reason)
{
    if (!bExternalJobs && IsValid(CargoActor)) CargoActor->Destroy();
    CargoActor=nullptr;
    Super::EndPlay(Reason);
}

bool APortWorkingCrane::AssignCargo(APortContainerActor* Cargo,FVector Source,FVector Destination,bool SourceSupport,bool DestinationSupport)
{
    if (!bExternalJobs || bJobActive || !IsValid(Cargo) || !Fault.IsEmpty()) return false;
    CargoActor=Cargo; Slots[0]=Source; Slots[1]=Destination; SourceSlot=Stage=0;
    bJobActive=true; bCarrying=false; Speed=StageTime=SettleTime=0; JobStartHead=Head;
    for (int32 I=0;I<2;++I)
    {
        const bool Support=I==0?SourceSupport:DestinationSupport;
        Pads[I]->SetWorldLocationAndRotation(Slots[I]-FVector(0,0,139.5f),Orientation);
        Pads[I]->SetCollisionEnabled(Support?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        Pads[I]->SetVisibility(Support);
    }
    return true;
}