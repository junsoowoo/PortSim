#include "PortSiteLogistics.h"
#include "PortWorkingCrane.h"
#include "PortAGVActor.h"
#include "PortContainerActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

namespace SiteLogistics
{
    constexpr float CraneY[]={-450,-350,-250,-100,100,250,350,450};
    FTransform Hidden(FTransform T) { T.SetScale3D(FVector::ZeroVector); return T; }
    FTransform YardTransform(FVector P) { return FTransform(FQuat::Identity,P,FVector(12.192,2.438,2.59)); }
}

APortSiteLogistics::APortSiteLogistics() { PrimaryActorTick.bCanEverTick=false; }

void APortSiteLogistics::AddShipCargo(UHierarchicalInstancedStaticMeshComponent* Mesh,int32 Instance,const FTransform& Transform,int32 STS)
{
    FSiteShipCargo Cargo;
    Cargo.Mesh=Mesh; Cargo.Instance=Instance; Cargo.Transform=Transform; Cargo.STS=STS;
    Cargo.ID=2000+Manifest.Num(); Manifest.Add(Cargo);
}

FVector APortSiteLogistics::QuayPark(int32 Lane) const
{ return FVector(4500,(SiteLogistics::CraneY[Lane]+8)*100,0); }
FVector APortSiteLogistics::YardHandover(const FSiteYardSlot& Slot) const
{ return FVector(Slot.Half?42000:18000,(-460+Slot.Block*44+13.2f)*100,0); }

void APortSiteLogistics::Initialize(const TArray<TObjectPtr<APortWorkingCrane>>& Cranes,TArray<FSiteYardSlot> Slots,
    const TArray<UHierarchicalInstancedStaticMeshComponent*>& Palette,int32 CentralCargo,int32 FixedYard)
{
    Equipment=Cranes; Yard=MoveTemp(Slots); CentralCount=CentralCargo;
    BaselineYard=Yard.Num()+FixedYard;
    InitialYard=BaselineYard-InitialShipCount();
    check(Equipment.Num()==44 && Yard.Num()>InitialShipCount());
    // Remove TOP tiers only; filling the reservation bottom-up restores supported stacks.
    Yard.StableSort([](const FSiteYardSlot& A,const FSiteYardSlot& B) { return A.Position.Z==B.Position.Z ? A.Position.X<B.Position.X : A.Position.Z>B.Position.Z; });
    for (int32 I=0;I<Yard.Num();++I)
    {
        auto& Slot=Yard[I]; Slot.Reserved=I<InitialShipCount(); Slot.Occupied=!Slot.Reserved;
        Slot.Mesh=Palette[Slot.Color];
        const FTransform T=SiteLogistics::YardTransform(Slot.Position);
        Slot.Instance=Slot.Mesh->AddInstance(Slot.Reserved?SiteLogistics::Hidden(T):T);
    }
    Yard.StableSort([](const FSiteYardSlot& A,const FSiteYardSlot& B) { return A.Position.Z<B.Position.Z; });
    // Upper ship tiers leave first, so no boxes are lifted through an upper stack.
    Manifest.StableSort([](const FSiteShipCargo& A,const FSiteShipCargo& B) { return A.Transform.GetLocation().Z>B.Transform.GetLocation().Z; });
    Jobs.SetNum(8); BlocksBusy.Init(false,18); SlotAssigned.Init(false,Yard.Num());
    FActorSpawnParameters Params; Params.Owner=this;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (int32 I=0;I<8;++I)
    {
        auto* Vehicle=GetWorld()->SpawnActor<APortAGVActor>(QuayPark(I),FRotator::ZeroRotator,Params);
        Vehicle->InitializeVehicle(100+I); Vehicles.Add(Vehicle);
    }
    bReady=true;
#if WITH_EDITOR
    SetActorLabel(TEXT("DGT_STS_AGV_RMG_Dispatch"));
    SetFolderPath(TEXT("PortSim/Logistics"));
#endif
    UE_LOG(LogTemp,Display,TEXT("SITE_INVENTORY: yard_before=%d, vessel_total=%d, yard_initial=%d, reserved=%d, site_ship=%d, central_ship=%d"),
        BaselineYard,InitialShipCount(),InitialYard,InitialShipCount(),Manifest.Num(),CentralCount);
}

void APortSiteLogistics::Dispatch(int32 Lane)
{
    if (Dispatched>=DispatchLimit) return;
    int32 CargoIndex=INDEX_NONE;
    for (int32 I=0;I<Manifest.Num();++I)
        if (Manifest[I].STS==Lane && Manifest[I].State==0) { CargoIndex=I; break; }
    if (CargoIndex==INDEX_NONE) return;
    int32 SlotIndex=INDEX_NONE, CraneIndex=INDEX_NONE;
    for (int32 Try=0;Try<36 && SlotIndex==INDEX_NONE;++Try)
    {
        const int32 RMG=(NextRMG+Try)%36;
        if (BlocksBusy[RMG/2]) continue;
        for (int32 I=0;I<Yard.Num();++I)
            if (Yard[I].Reserved && !Yard[I].Occupied && !SlotAssigned[I] && Yard[I].Block*2+Yard[I].Half==RMG)
            { SlotIndex=I; CraneIndex=RMG; break; }
    }
    if (SlotIndex==INDEX_NONE) return;
    auto& Record=Manifest[CargoIndex]; auto& Job=Jobs[Lane];
    FActorSpawnParameters Params; Params.Owner=this;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Cargo=GetWorld()->SpawnActor<APortContainerActor>(Record.Transform.GetLocation(),FRotator::ZeroRotator,Params);
    Cargo->InitializeContainer(Record.ID); Cargo->LocationOwner=ECargoOwner::Ship;
    Record.Mesh->UpdateInstanceTransform(Record.Instance,SiteLogistics::Hidden(Record.Transform),false,true,true);
    Record.State=1;
    Job.Cargo=CargoIndex; Job.Slot=SlotIndex; Job.RMG=CraneIndex; Job.Actor=Cargo; Job.Stage=1; Job.Time=0;
    SlotAssigned[SlotIndex]=true; BlocksBusy[Yard[SlotIndex].Block]=true; NextRMG=(CraneIndex+1)%36; ++Dispatched;
    if (!Equipment[36+Lane]->AssignCargo(Cargo,Record.Transform.GetLocation(),Vehicles[Lane]->CargoPosition(),true,false))
        Stop(TEXT("STS could not accept reserved cargo"));
    UE_LOG(LogTemp,Display,TEXT("SITE_JOB: C%d STS%d -> AGV%d -> RMG%d -> slot%d"),Record.ID,Lane+1,100+Lane,CraneIndex+1,SlotIndex);
}

void APortSiteLogistics::PrepareRoute(int32 Lane,bool Return)
{
    auto& Job=Jobs[Lane]; const FVector Quay=QuayPark(Lane), YardPoint=YardHandover(Yard[Job.Slot]);
    Job.Route.Reset(); Job.Waypoint=0;
    Job.Route.Add(FVector(14500,Return?YardPoint.Y:Quay.Y,0));
    Job.Route.Add(FVector(14500,Return?Quay.Y:YardPoint.Y,0));
    Job.Route.Add(Return?Quay:YardPoint);
}

bool APortSiteLogistics::Drive(int32 Lane,float Dt)
{
    // Only one vehicle occupies the shared road. Waiting vehicles remain at the quay
    // or inside an exclusively reserved yard block, never at a crossing.
    if (CorridorOwner!=INDEX_NONE && CorridorOwner!=Lane) return false;
    CorridorOwner=Lane;
    auto& Job=Jobs[Lane]; auto* Vehicle=Vehicles[Lane].Get();
    if (!Vehicle->MoveToPosition(Job.Route[Job.Waypoint],Dt)) return false;
    if (Job.Waypoint==0 && Job.Stage==2) Vehicle->SetActorRotation(FRotator(0,90,0));
    if (Job.Waypoint==1 && Job.Stage==5) Vehicle->SetActorRotation(FRotator::ZeroRotator);
    ++Job.Waypoint;
    if (Job.Waypoint<Job.Route.Num()) return false;
    CorridorOwner=INDEX_NONE; return true;
}

void APortSiteLogistics::Freeze(bool Paused)
{
    for (const auto& Crane:Equipment) Crane->SetOperationPaused(Paused);
    if (Paused==bWasPaused) return;
    bWasPaused=Paused;
    // Cargo on the AGV is attached; crane-managed cargo is frozen by its crane.
    for (auto& Job:Jobs)
        if (Job.Actor.IsValid() && Job.Actor->LocationOwner==ECargoOwner::AGV && !Job.Actor->GetAttachParentActor())
            Job.Actor->GetBody()->SetSimulatePhysics(!Paused);
}

void APortSiteLogistics::Stop(const FString& Reason)
{ Fault=Reason; Freeze(true); UE_LOG(LogTemp,Error,TEXT("SITE_LOGISTICS_FAIL: %s"),*Reason); }

void APortSiteLogistics::Advance(float Dt,bool Paused)
{
    if (!bReady) return;
    Freeze(Paused || !Fault.IsEmpty());
    if (Paused || !Fault.IsEmpty()) return;
    for (const auto& Crane:Equipment)
    {
        Crane->Advance(Dt,false);
        if (!Crane->Fault.IsEmpty()) { Stop(Crane->Fault); return; }
    }
    for (int32 Lane=0;Lane<Jobs.Num();++Lane)
    {
        auto& Job=Jobs[Lane]; auto* Vehicle=Vehicles[Lane].Get();
        if (Job.Stage==0) { Dispatch(Lane); continue; }
        Job.Time+=Dt;
        if (Job.Time>12000.f) { Stop(TEXT("Shipment timeout")); return; }
        auto* Cargo=Job.Actor.Get();
        switch(Job.Stage)
        {
        case 1:
            if (Equipment[36+Lane]->IsBusy()) break;
            if (!Cargo || !Cargo->GetActorLocation().Equals(Vehicle->CargoPosition(),10.f) || Vehicle->Speed>0)
            { Stop(TEXT("STS / AGV handover alignment")); return; }
            Cargo->GetBody()->SetSimulatePhysics(false);
            Cargo->AttachToComponent(Vehicle->GetRootComponent(),FAttachmentTransformRules::KeepWorldTransform);
            Cargo->LocationOwner=ECargoOwner::AGV;
            PrepareRoute(Lane,false); Job.Stage=2; break;
        case 2:
            if (!Drive(Lane,Dt)) break;
            Cargo->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            Cargo->GetBody()->SetSimulatePhysics(true);
            Cargo->GetBody()->SetPhysicsLinearVelocity(FVector::ZeroVector);
            Cargo->GetBody()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            if (!Equipment[Job.RMG]->AssignCargo(Cargo,Vehicle->CargoPosition(),Yard[Job.Slot].Position,false,true))
            { Stop(TEXT("AGV / RMG reservation handover")); return; }
            Job.Stage=3; break;
        case 3:
            if (Equipment[Job.RMG]->IsBusy()) break;
            if (!Cargo || Cargo->LocationOwner!=ECargoOwner::Yard || !Cargo->GetActorLocation().Equals(Yard[Job.Slot].Position,10.f))
            { Stop(TEXT("RMG yard placement mismatch")); return; }
            Yard[Job.Slot].Occupied=true;
            Yard[Job.Slot].Mesh->UpdateInstanceTransform(Yard[Job.Slot].Instance,SiteLogistics::YardTransform(Yard[Job.Slot].Position),false,true,true);
            Manifest[Job.Cargo].State=2; ++Delivered; ++Vehicle->CompletedJobs;
            UE_LOG(LogTemp,Display,TEXT("SITE_DELIVERED: C%d via AGV%d -> RMG%d; total=%d"),Manifest[Job.Cargo].ID,100+Lane,Job.RMG+1,Delivered);
            Cargo->Destroy(); Job.Actor=nullptr;
            PrepareRoute(Lane,true); Job.Stage=5; break;
        case 5:
            if (!Drive(Lane,Dt)) break;
            BlocksBusy[Yard[Job.Slot].Block]=false;
            Job=FSiteTransfer(); break;
        default: Stop(TEXT("Unknown dispatch stage")); return;
        }
    }
}

void APortSiteLogistics::ResetLogistics()
{
    if (!bReady) return;
    for (auto& Job:Jobs) if (Job.Actor.IsValid()) { Job.Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); Job.Actor->Destroy(); }
    for (const auto& Crane:Equipment) Crane->ResetOperation();
    for (auto& Cargo:Manifest)
    { Cargo.State=0; Cargo.Mesh->UpdateInstanceTransform(Cargo.Instance,Cargo.Transform,false,true,true); }
    for (auto& Slot:Yard) if (Slot.Reserved)
    { Slot.Occupied=false; Slot.Mesh->UpdateInstanceTransform(Slot.Instance,SiteLogistics::Hidden(SiteLogistics::YardTransform(Slot.Position)),false,true,true); }
    for (int32 I=0;I<8;++I) { Jobs[I]=FSiteTransfer(); Vehicles[I]->ResetVehicle(QuayPark(I)); }
    BlocksBusy.Init(false,18); SlotAssigned.Init(false,Yard.Num());
    CorridorOwner=INDEX_NONE; NextRMG=Dispatched=Delivered=0; Fault.Empty(); bWasPaused=false;
}

int32 APortSiteLogistics::ShipRemaining() const
{
    int32 Count=0;
    for (const auto& Cargo:Manifest) Count+=Cargo.State==0;
    for (const auto& Job:Jobs) if (Job.Actor.IsValid() && Job.Actor->LocationOwner==ECargoOwner::Ship) ++Count;
    return Count;
}
int32 APortSiteLogistics::InTransit() const { return Manifest.Num()-ShipRemaining()-Delivered; }

bool APortSiteLogistics::Validate(FString& Error) const
{
    if (!Fault.IsEmpty()) { Error=Fault; return false; }
    if (BaselineYard-InitialYard!=InitialShipCount()) { Error=TEXT("Yard reduction does not equal vessel inventory"); return false; }
    int32 Ship=0,Active=0,Placed=0,Reserved=0,OccupiedReservations=0;
    TSet<int32> IDs,JobCargo,JobSlots,ActiveBlocks;
    for (const auto& Cargo:Manifest)
    {
        if (IDs.Contains(Cargo.ID)) { Error=TEXT("Duplicate manifest cargo ID"); return false; }
        IDs.Add(Cargo.ID); Ship+=Cargo.State==0; Active+=Cargo.State==1; Placed+=Cargo.State==2;
    }
    for (const auto& Slot:Yard) { Reserved+=Slot.Reserved; OccupiedReservations+=Slot.Reserved && Slot.Occupied; }
    if (Ship+Active+Placed!=Manifest.Num() || Placed!=Delivered || Reserved!=InitialShipCount() || OccupiedReservations!=Delivered)
    { Error=TEXT("Inventory conservation / reservation mismatch"); return false; }
    int32 Physical=0;
    for (int32 Lane=0;Lane<Jobs.Num();++Lane)
    {
        const auto& Job=Jobs[Lane];
        if (!Job.Stage) continue;
        if (JobSlots.Contains(Job.Slot) || JobCargo.Contains(Job.Cargo) || ActiveBlocks.Contains(Yard[Job.Slot].Block))
        { Error=TEXT("Duplicate job cargo, slot or yard block reservation"); return false; }
        JobSlots.Add(Job.Slot); JobCargo.Add(Job.Cargo); ActiveBlocks.Add(Yard[Job.Slot].Block);
        if (Job.Actor.IsValid())
        {
            ++Physical;
            if (Job.Actor->ContainerID!=FName(*FString::Printf(TEXT("C%02d"),Manifest[Job.Cargo].ID)))
            { Error=TEXT("Cargo identity changed during handover"); return false; }
            if (Job.Stage==2 && (Job.Actor->LocationOwner!=ECargoOwner::AGV || Job.Actor->GetAttachParentActor()!=Vehicles[Lane] ||
                !Job.Actor->GetActorLocation().Equals(Vehicles[Lane]->CargoPosition(),1.f)))
            { Error=TEXT("AGV cargo alignment/ownership mismatch"); return false; }
        }
        if (Vehicles[Lane]->Speed>0 && CorridorOwner!=Lane) { Error=TEXT("AGV moved without road reservation"); return false; }
    }
    if (Physical!=Active) { Error=TEXT("Physical/manifest cargo mismatch"); return false; }
    for (const auto& Crane:Equipment) if (!Crane->ValidateOperation(Error)) return false;
    return true;
}

void APortSiteLogistics::EndPlay(const EEndPlayReason::Type Reason)
{
    for (auto& Job:Jobs) if (Job.Actor.IsValid()) Job.Actor->Destroy();
    for (const auto& Vehicle:Vehicles) if (IsValid(Vehicle)) Vehicle->Destroy();
    Super::EndPlay(Reason);
}

bool APortSiteLogistics::IsIdle() const
{
    for (const auto& Job:Jobs) if (Job.Stage) return false;
    return true;
}
TArray<FVector> APortSiteLogistics::Snapshot() const
{
    TArray<FVector> Positions;
    for (const auto& Vehicle:Vehicles) Positions.Add(Vehicle->GetActorLocation());
    for (const auto& Job:Jobs) Positions.Add(Job.Actor.IsValid()?Job.Actor->GetActorLocation():FVector::ZeroVector);
    for (const auto& Crane:Equipment) { Positions.Add(Crane->GetActorLocation()); Positions.Add(Crane->HeadPosition()); }
    return Positions;
}