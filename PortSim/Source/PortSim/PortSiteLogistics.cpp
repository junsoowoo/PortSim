#include "PortSiteLogistics.h"
#include "PortWorkingCrane.h"
#include "PortAGVActor.h"
#include "PortContainerActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

namespace SiteLogistics
{
    constexpr float CraneY[]={-450,-350,-250,-100,0,100,250,350,450};
    constexpr float LegacyCraneY[]={-450,-350,-250,-100,100,250,350,450};
    FTransform Hidden(FTransform T) { T.SetScale3D(FVector::ZeroVector); return T; }
    FTransform YardTransform(FVector P) { return FTransform(FQuat::Identity,P,FVector(12.192,2.438,2.59)); }
}

APortSiteLogistics::APortSiteLogistics() { PrimaryActorTick.bCanEverTick=false; }

void APortSiteLogistics::AddShipCargo(FVector Position,int32 STS)
{
    check(STS>=0 && STS<9);
    FSiteShipCargo Record;
    Record.Transform=FTransform(FQuat::Identity,Position);
    Record.STS=STS; Record.ID=2000+Manifest.Num();
    FActorSpawnParameters Params; Params.Owner=this;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Cargo=GetWorld()->SpawnActor<APortContainerActor>(Position,FRotator::ZeroRotator,Params);
    check(Cargo);
    Cargo->InitializeContainer(Record.ID);
    // Secured aboard until pickup: real collidable actors, without 1,064 idle rigid bodies.
    Cargo->GetBody()->SetSimulatePhysics(false);
    Cargo->LocationOwner=ECargoOwner::Ship;
    Record.Actor=Cargo;
    ShipContainers.Add(Cargo);
    Manifest.Add(Record);
}
FVector APortSiteLogistics::QuayPark(int32 Lane) const
{ return FVector(4500,((LaneCount==9?SiteLogistics::CraneY[Lane]:SiteLogistics::LegacyCraneY[Lane])+8)*100,0); }
FVector APortSiteLogistics::YardHandover(const FSiteYardSlot& Slot) const
{ return FVector(Slot.Half?42000:18000,(-460+Slot.Block*44+13.2f)*100,0); }

void APortSiteLogistics::Initialize(const TArray<TObjectPtr<APortWorkingCrane>>& Cranes,TArray<FSiteYardSlot> Slots,
    const TArray<UHierarchicalInstancedStaticMeshComponent*>& Palette,int32 CentralCargo,int32 FixedYard)
{
    Equipment=Cranes; Yard=MoveTemp(Slots); CentralCount=CentralCargo; LaneCount=Equipment.Num()-36;
    BaselineYard=Yard.Num()+FixedYard;
    InitialYard=BaselineYard-InitialShipCount();
    check((LaneCount==8 || LaneCount==9) && Yard.Num()>InitialShipCount());
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
    CentralSlots.Reset();
    for (int32 Cargo=0;Cargo<CentralCount;++Cargo)
    {
        const int32 Slot=Yard.IndexOfByPredicate([Cargo](const FSiteYardSlot& S)
        { return S.Block==Cargo%18 && S.Reserved && !S.Central; });
        check(Slot!=INDEX_NONE);
        Yard[Slot].Central=true;
        CentralSlots.Add(Slot);
    }
    // Upper ship tiers leave first, so no boxes are lifted through an upper stack.
    Manifest.StableSort([](const FSiteShipCargo& A,const FSiteShipCargo& B) { return A.Transform.GetLocation().Z>B.Transform.GetLocation().Z; });
    Jobs.SetNum(LaneCount); BlocksBusy.Init(false,18); RMGBusy.Init(false,36);
    PreparedCargo.Init(INDEX_NONE,LaneCount); SlotAssigned.Init(false,Yard.Num());
    FActorSpawnParameters Params; Params.Owner=this;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (int32 I=0;I<LaneCount;++I)
    {
        auto* Vehicle=GetWorld()->SpawnActor<APortAGVActor>(QuayPark(I),FRotator::ZeroRotator,Params);
        Vehicle->InitializeVehicle(100+I); Vehicles.Add(Vehicle);
    }
    TrafficVehicles=Vehicles;
    bReady=true;
#if WITH_EDITOR
    SetActorLabel(TEXT("DGT_STS_AGV_RMG_Dispatch"));
    SetFolderPath(TEXT("PortSim/Logistics"));
#endif
    UE_LOG(LogTemp,Display,TEXT("SITE_INVENTORY: yard_before=%d, vessel_total=%d, yard_initial=%d, reserved=%d, site_ship=%d, central_ship=%d"),
        BaselineYard,InitialShipCount(),InitialYard,InitialShipCount(),Manifest.Num(),CentralCount);
}

void APortSiteLogistics::PrepareNextCargo(int32 Lane)
{
    if (PreparedCargo[Lane]!=INDEX_NONE || Equipment[36+Lane]->IsBusy() || Dispatched>=DispatchLimit) return;
    const int32 Index=Manifest.IndexOfByPredicate([Lane](const FSiteShipCargo& C) { return C.STS==Lane && C.State==0; });
    if (Index==INDEX_NONE) return;
    auto& Record=Manifest[Index];
    if (!Equipment[36+Lane]->AssignCargo(Record.Actor.Get(),Record.Transform.GetLocation(),QuayPark(Lane)+FVector(0,0,349.5f),false,false))
    { Stop(TEXT("STS could not prepare its next ship container")); return; }
    Equipment[36+Lane]->SetDestinationReady(false);
    PreparedCargo[Lane]=Index; Record.State=1; ++Dispatched;
    if (Jobs[Lane].Stage) ++PrefetchedJobs;
}

void APortSiteLogistics::Dispatch(int32 Lane)
{
    PrepareNextCargo(Lane);
    const int32 Index=PreparedCargo[Lane];
    if (Index==INDEX_NONE) return;
    auto& Job=Jobs[Lane];
    Job.Cargo=Index; Job.Actor=Manifest[Index].Actor; Job.Stage=1; Job.Time=0;
    PreparedCargo[Lane]=INDEX_NONE;
    Equipment[36+Lane]->SetDestinationReady(true);
}

bool APortSiteLogistics::ReserveYard(int32 Lane)
{
    auto& Job=Jobs[Lane];
    float BestDistance=TNumericLimits<float>::Max();
    int32 BestSlot=INDEX_NONE;
    for (int32 I=0;I<Yard.Num();++I)
    {
        const auto& Slot=Yard[I]; const int32 RMG=Slot.Block*2+Slot.Half;
        if (!Slot.Reserved || Slot.Central || Slot.Occupied || SlotAssigned[I] || BlocksBusy[Slot.Block] ||
            RMGBusy[RMG] || Equipment[RMG]->IsBusy() || !Equipment[RMG]->Fault.IsEmpty() ||
            (CentralPending!=INDEX_NONE && Yard[CentralSlots[CentralPending]].Block==Slot.Block)) continue;
        const float Distance=FVector::Dist2D(Vehicles[Lane]->GetActorLocation(),YardHandover(Slot))+
            FVector::Dist2D(Equipment[RMG]->HeadPosition(),Slot.Position);
        if (Distance<BestDistance) { BestDistance=Distance; BestSlot=I; }
    }
    if (BestSlot==INDEX_NONE) return false;
    Job.Slot=BestSlot; Job.RMG=Yard[BestSlot].Block*2+Yard[BestSlot].Half;
    SlotAssigned[BestSlot]=true; RMGBusy[Job.RMG]=true;
    UE_LOG(LogTemp,Display,TEXT("SITE_JOB: C%d AGV%d selected available RMG%d -> slot%d"),Manifest[Job.Cargo].ID,100+Lane,Job.RMG+1,BestSlot);
    return true;
}

void APortSiteLogistics::PrepareRoute(int32 Lane,bool Return)
{
    auto& Job=Jobs[Lane]; const FVector Quay=QuayPark(Lane), YardPoint=YardHandover(Yard[Job.Slot]);
    const float BlockY=(-460+Yard[Job.Slot].Block*44)*100.f;
    Job.Route.Reset(); Job.Waypoint=0;
    if (!Return)
    {
        const float RoadX=BlockY+2050>=Quay.Y?12500.f:16500.f;
        Job.Route.Add(FVector(RoadX,Quay.Y,0));
        Job.Route.Add(FVector(RoadX,BlockY+2050,0));
        Job.Route.Add(FVector(YardPoint.X,BlockY+2050,0));
        Job.Route.Add(YardPoint);
    }
    else
    {
        Job.Route.Add(FVector(YardPoint.X,BlockY+2500,0));
        const float RoadX=Quay.Y+2000>=BlockY+2500?12500.f:16500.f;
        Job.Route.Add(FVector(RoadX,BlockY+2500,0));
        Job.Route.Add(FVector(RoadX,Quay.Y+2000,0));
        Job.Route.Add(FVector(Quay.X,Quay.Y+2000,0));
        Job.Route.Add(Quay);
    }
}

void APortSiteLogistics::RegisterBerthVehicles(const TArray<TObjectPtr<APortAGVActor>>& BerthVehicles)
{ for (const auto& Vehicle:BerthVehicles) TrafficVehicles.AddUnique(Vehicle); }

void APortSiteLogistics::BeginTrafficFrame()
{
    for (int32 ID:FinishedRoadSegments) RoadReservations.Remove(ID);
    FinishedRoadSegments.Reset();
}

bool APortSiteLogistics::MoveVehicle(APortAGVActor* Vehicle,FVector Target,float Dt)
{
    const int32 ID=Vehicle->VehicleID;
    if (!RoadReservations.Contains(ID))
    {
        const FVector Along=Vehicle->GetActorForwardVector().GetAbs(), Across=Vehicle->GetActorRightVector().GetAbs();
        const FVector Extent=Along*210.f+Across*730.f+FVector(0,0,250);
        FBox Segment(Vehicle->GetActorLocation()-Extent,Vehicle->GetActorLocation()+Extent);
        Segment+=Target-Extent; Segment+=Target+Extent;
        for (const auto& Reservation:RoadReservations)
            if (Reservation.Key!=ID && Segment.Intersect(Reservation.Value)) { Vehicle->Speed=0; return false; }
        for (const auto& Other:TrafficVehicles)
        {
            if (Other==Vehicle) continue;
            const FVector E=Other->GetActorForwardVector().GetAbs()*210.f+Other->GetActorRightVector().GetAbs()*730.f+FVector(0,0,250);
            if (Segment.Intersect(FBox(Other->GetActorLocation()-E,Other->GetActorLocation()+E))) { Vehicle->Speed=0; return false; }
        }
        RoadReservations.Add(ID,Segment);
    }
    const bool Arrived=Vehicle->MoveToPosition(Target,Dt);
    if (Arrived) FinishedRoadSegments.Add(ID);
    return Arrived;
}

bool APortSiteLogistics::Drive(int32 Lane,float Dt)
{
    auto& Job=Jobs[Lane]; auto* Vehicle=Vehicles[Lane].Get();
    if (!MoveVehicle(Vehicle,Job.Route[Job.Waypoint],Dt)) return false;
    if (Job.Waypoint==0 && Job.Stage==3) Vehicle->SetActorRotation(FRotator(0,90,0));
    ++Job.Waypoint;
    if (Job.Waypoint<Job.Route.Num()) return false;
    if (Job.Stage==5) Vehicle->SetActorRotation(FRotator::ZeroRotator);
    return true;
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
        // The central berth advances its reserved RMG in TickRMGTransfer.
        if (CentralReservation==INDEX_NONE || Crane!=CentralCrane(CentralReservation)) Crane->Advance(Dt,false);
        if (!Crane->Fault.IsEmpty()) { Stop(Crane->Fault); return; }
    }
    int32 Moving=0;
    for (const auto& Vehicle:Vehicles) Moving+=Vehicle->Speed>1.f;
    PeakMovingVehicles=FMath::Max(PeakMovingVehicles,Moving);
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
            Manifest[Job.Cargo].HandoverMask|=1;
            UE_LOG(LogTemp,Display,TEXT("SITE_HANDOVER: C%d STS -> AGV%d"),Manifest[Job.Cargo].ID,100+Lane);
            Job.Stage=2;
            PrepareNextCargo(Lane);
            break;
        case 2:
            if (!ReserveYard(Lane)) break;
            PrepareRoute(Lane,false); Job.Stage=3; break;
        case 3:
            if (!Drive(Lane,Dt)) break;
            // Keep the twist locks engaged until the RMG actually picks up the box.
            if (!Equipment[Job.RMG]->AssignCargo(Cargo,Vehicle->CargoPosition(),Yard[Job.Slot].Position,false,true))
            { Stop(TEXT("AGV / RMG reservation handover")); return; }
            Manifest[Job.Cargo].HandoverMask|=2;
            UE_LOG(LogTemp,Display,TEXT("SITE_HANDOVER: C%d AGV%d -> RMG%d"),Manifest[Job.Cargo].ID,100+Lane,Job.RMG+1);
            Job.Stage=4; break;
        case 4:
            if (Equipment[Job.RMG]->IsBusy()) break;
            if (!Cargo || Cargo->LocationOwner!=ECargoOwner::Yard || !Cargo->GetActorLocation().Equals(Yard[Job.Slot].Position,10.f))
            { Stop(TEXT("RMG yard placement mismatch")); return; }
            Yard[Job.Slot].Occupied=true;
            Cargo->GetBody()->SetSimulatePhysics(false);
            Cargo->SetActorLocation(Yard[Job.Slot].Position);
            PlacedContainers.Add(Cargo);
            Manifest[Job.Cargo].HandoverMask|=4;
            Manifest[Job.Cargo].State=2; ++Delivered; ++Vehicle->CompletedJobs;
            UE_LOG(LogTemp,Display,TEXT("SITE_DELIVERED: C%d via AGV%d -> RMG%d; total=%d"),Manifest[Job.Cargo].ID,100+Lane,Job.RMG+1,Delivered);
            Job.Actor=nullptr;
            PrepareRoute(Lane,true); Job.Stage=5; break;
        case 5:
            if (!Drive(Lane,Dt)) break;
            RMGBusy[Job.RMG]=false;
            Job=FSiteTransfer(); break;
        default: Stop(TEXT("Unknown dispatch stage")); return;
        }
    }
}

void APortSiteLogistics::ResetLogistics()
{
    if (!bReady) return;
    for (auto& Job:Jobs) if (Job.Actor.IsValid()) Job.Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    PlacedContainers.Reset();
    for (const auto& Crane:Equipment) Crane->ResetOperation();
    for (auto& Cargo:Manifest)
    {
        auto* Actor=Cargo.Actor.Get();
        check(Actor);
        Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        Actor->GetBody()->SetSimulatePhysics(false);
        Actor->SetActorLocationAndRotation(Cargo.Transform.GetLocation(),Cargo.Transform.GetRotation(),false,nullptr,ETeleportType::TeleportPhysics);
        Actor->LocationOwner=ECargoOwner::Ship;
        Cargo.State=0; Cargo.HandoverMask=0;
    }
    for (auto& Slot:Yard) if (Slot.Reserved)
    { Slot.Occupied=false; Slot.Mesh->UpdateInstanceTransform(Slot.Instance,SiteLogistics::Hidden(SiteLogistics::YardTransform(Slot.Position)),false,true,true); }
    for (int32 I=0;I<LaneCount;++I) { Jobs[I]=FSiteTransfer(); Vehicles[I]->ResetVehicle(QuayPark(I)); }
    BlocksBusy.Init(false,18); RMGBusy.Init(false,36); PreparedCargo.Init(INDEX_NONE,LaneCount); SlotAssigned.Init(false,Yard.Num());
    CentralReservation=CentralPending=INDEX_NONE;
    RoadReservations.Reset(); FinishedRoadSegments.Reset(); PeakMovingVehicles=PrefetchedJobs=Dispatched=Delivered=0; Fault.Empty(); bWasPaused=false;
}

int32 APortSiteLogistics::ShipRemaining() const
{
    int32 Count=0;
    for (const auto& Cargo:Manifest) Count+=Cargo.State==0;
    for (const auto& Job:Jobs) if (Job.Actor.IsValid() && Job.Actor->LocationOwner==ECargoOwner::Ship) ++Count;
    for (int32 Index:PreparedCargo) if (Index!=INDEX_NONE && Manifest[Index].Actor->LocationOwner==ECargoOwner::Ship) ++Count;
    return Count;
}
int32 APortSiteLogistics::InTransit() const { return Manifest.Num()-ShipRemaining()-Delivered; }

bool APortSiteLogistics::Validate(FString& Error) const
{
    if (!Fault.IsEmpty()) { Error=Fault; return false; }
    if (BaselineYard-InitialYard!=InitialShipCount()) { Error=TEXT("Yard reduction does not equal vessel inventory"); return false; }
    if (LaneCount==9)
    {
        int32 VesselCounts[3]={0,0,0};
        for (const auto& Cargo:Manifest)
        {
            if (Cargo.STS<0 || Cargo.STS>=9) { Error=TEXT("Invalid berth STS assignment"); return false; }
            ++VesselCounts[Cargo.STS/3];
        }
        if (CentralCount!=0 || VesselCounts[0]!=528 || VesselCounts[1]!=528 || VesselCounts[2]!=528 || Vehicles.Num()!=9)
        { Error=TEXT("Three equal vessels / unified fleet inventory mismatch"); return false; }
    }
    int32 Ship=0,Active=0,Placed=0,Reserved=0,OccupiedReservations=0,CentralOccupied=0;
    TSet<int32> IDs,JobCargo,JobSlots,ActiveRMGs;
    TSet<const APortContainerActor*> Actors;
    if (ShipContainers.Num()!=Manifest.Num()) { Error=TEXT("Ship actor/manifest count mismatch" ); return false; }
    for (const auto& Cargo:Manifest)
    {
        const auto* Actor=Cargo.Actor.Get();
        if (!IsValid(Actor) || Actor->GetOwner()!=this || Actors.Contains(Actor) ||
            Actor->ContainerID!=FName(*FString::Printf(TEXT("C%02d"),Cargo.ID)) || Cargo.STS<0 || Cargo.STS>=LaneCount)
        { Error=TEXT("Missing, replaced, duplicate or unassigned ship container actor"); return false; }
        Actors.Add(Actor);
        if (Cargo.State==0 && (Actor->LocationOwner!=ECargoOwner::Ship || Actor->GetAttachParentActor() ||
            Actor->GetBody()->IsSimulatingPhysics() || !Actor->GetActorLocation().Equals(Cargo.Transform.GetLocation(),.1f)))
        { Error=TEXT("Secured ship cargo moved or lost its ship state"); return false; }
        if (IDs.Contains(Cargo.ID)) { Error=TEXT("Duplicate manifest cargo ID"); return false; }
        if (Cargo.State==2 && Cargo.HandoverMask!=7) { Error=TEXT("Delivered cargo bypassed STS/AGV/RMG handover" ); return false; }
        IDs.Add(Cargo.ID); Ship+=Cargo.State==0; Active+=Cargo.State==1; Placed+=Cargo.State==2;
    }
    for (const auto& Slot:Yard) { Reserved+=Slot.Reserved; OccupiedReservations+=Slot.Reserved && Slot.Occupied; CentralOccupied+=Slot.Central && Slot.Occupied; }
    if (Ship+Active+Placed!=Manifest.Num() || Placed!=Delivered || Reserved!=InitialShipCount() || OccupiedReservations!=Delivered+CentralOccupied)
    { Error=TEXT("Inventory conservation / reservation mismatch"); return false; }
    TSet<int32> CentralIDs,CentralBlocks;
    if (CentralSlots.Num()!=CentralCount) { Error=TEXT("Central yard mapping count mismatch"); return false; }
    for (int32 Slot:CentralSlots)
    {
        if (!Yard.IsValidIndex(Slot) || !Yard[Slot].Central || !Yard[Slot].Reserved || CentralIDs.Contains(Slot))
        { Error=TEXT("Duplicate or unreserved central yard destination"); return false; }
        CentralIDs.Add(Slot); CentralBlocks.Add(Yard[Slot].Block);
    }
    if (CentralCount>0 && CentralBlocks.Num()!=18) { Error=TEXT("Berth cargo must be distributed across all 18 yards"); return false; }
    if (CentralReservation!=INDEX_NONE)
    {
        if (!BlocksBusy[Yard[CentralSlots[CentralReservation]].Block])
        { Error=TEXT("Central transfer lost its road or yard reservation"); return false; }
        ActiveRMGs.Add(Yard[CentralSlots[CentralReservation]].Block*2);
        ActiveRMGs.Add(Yard[CentralSlots[CentralReservation]].Block*2+1);
    }
    int32 Physical=0;
    for (int32 Lane=0;Lane<Jobs.Num();++Lane)
    {
        const auto& Job=Jobs[Lane];
        if (!Job.Stage) continue;
        if (JobCargo.Contains(Job.Cargo)) { Error=TEXT("Duplicate job cargo"); return false; }
        JobCargo.Add(Job.Cargo);
        if (Job.Slot!=INDEX_NONE)
        {
            if (Yard[Job.Slot].Central || JobSlots.Contains(Job.Slot) || ActiveRMGs.Contains(Job.RMG) || !RMGBusy[Job.RMG])
            { Error=TEXT("Duplicate or missing slot/RMG reservation"); return false; }
            JobSlots.Add(Job.Slot); ActiveRMGs.Add(Job.RMG);
        }
        if (Job.Actor.IsValid())
        {
            ++Physical;
            if (Job.Actor!=Manifest[Job.Cargo].Actor || Job.Actor->ContainerID!=FName(*FString::Printf(TEXT("C%02d"),Manifest[Job.Cargo].ID)))
            { Error=TEXT("Cargo identity changed during handover"); return false; }
            if ((Job.Stage==2 || Job.Stage==3) && (Job.Actor->LocationOwner!=ECargoOwner::AGV || Job.Actor->GetAttachParentActor()!=Vehicles[Lane] ||
                !Job.Actor->GetActorLocation().Equals(Vehicles[Lane]->CargoPosition(),1.f)))
            { Error=TEXT("AGV cargo alignment/ownership mismatch"); return false; }
        }
        if (Vehicles[Lane]->Speed>0 && !RoadReservations.Contains(Vehicles[Lane]->VehicleID))
        { Error=TEXT("AGV moved without a path reservation"); return false; }
    }
    if (PlacedContainers.Num()!=Delivered) { Error=TEXT("Delivered cargo actor count mismatch" ); return false; }
    TSet<FName> PlacedIDs;
    for (const auto& Cargo:PlacedContainers)
    {
        if (!IsValid(Cargo) || Cargo->GetOwner()!=this || Cargo->LocationOwner!=ECargoOwner::Yard || Cargo->GetAttachParentActor() || PlacedIDs.Contains(Cargo->ContainerID))
        { Error=TEXT("Lost/duplicate/attached yard cargo actor" ); return false; }
        PlacedIDs.Add(Cargo->ContainerID);
    }
    for (int32 Index:PreparedCargo) if (Index!=INDEX_NONE)
    {
        if (JobCargo.Contains(Index) || Manifest[Index].State!=1) { Error=TEXT("Invalid prefetched STS cargo"); return false; }
        JobCargo.Add(Index); ++Physical;
    }
    if (Physical!=Active) { Error=TEXT("Physical/manifest cargo mismatch"); return false; }
    for (int32 I=0;I<TrafficVehicles.Num();++I)
        for (int32 J=I+1;J<TrafficVehicles.Num();++J)
        {
            const auto* A=TrafficVehicles[I].Get(); const auto* B=TrafficVehicles[J].Get();
            const FVector EA=A->GetActorForwardVector().GetAbs()*169.f+A->GetActorRightVector().GetAbs()*689.f+FVector(0,0,100);
            const FVector EB=B->GetActorForwardVector().GetAbs()*169.f+B->GetActorRightVector().GetAbs()*689.f+FVector(0,0,100);
            if (FBox(A->GetActorLocation()-EA,A->GetActorLocation()+EA).Intersect(FBox(B->GetActorLocation()-EB,B->GetActorLocation()+EB)))
            { Error=FString::Printf(TEXT("AGV%d and AGV%d traffic envelopes overlap"),A->VehicleID,B->VehicleID); return false; }
        }
    for (const auto& Crane:Equipment) if (!Crane->ValidateOperation(Error)) return false;
    return true;
}

void APortSiteLogistics::EndPlay(const EEndPlayReason::Type Reason)
{
    for (const auto& Cargo:ShipContainers) if (IsValid(Cargo)) Cargo->Destroy();
    PlacedContainers.Reset(); ShipContainers.Reset();
    for (const auto& Vehicle:Vehicles) if (IsValid(Vehicle)) Vehicle->Destroy();
    Super::EndPlay(Reason);
}

bool APortSiteLogistics::IsIdle() const
{
    for (const auto& Job:Jobs) if (Job.Stage) return false;
    for (int32 Index:PreparedCargo) if (Index!=INDEX_NONE) return false;
    return true;
}
TArray<FVector> APortSiteLogistics::Snapshot() const
{
    TArray<FVector> Positions;
    for (const auto& Vehicle:Vehicles) Positions.Add(Vehicle->GetActorLocation());
    for (const auto& Cargo:ShipContainers) Positions.Add(Cargo->GetActorLocation());
    for (const auto& Crane:Equipment) { Positions.Add(Crane->GetActorLocation()); Positions.Add(Crane->HeadPosition()); }
    return Positions;
}
FVector APortSiteLogistics::CentralSlot(int32 Index) const
{ return Yard[CentralSlots[Index]].Position; }
FVector APortSiteLogistics::CentralHandover(int32 Index) const
{ return YardHandover(Yard[CentralSlots[Index]]); }
APortWorkingCrane* APortSiteLogistics::CentralCrane(int32 Index) const
{
    const auto& Slot=Yard[CentralSlots[Index]];
    return Equipment[Slot.Block*2+Slot.Half];
}
bool APortSiteLogistics::ReserveCentral(int32 Index)
{
    if (CentralReservation==Index) return true;
    CentralPending=Index;
    const auto& Slot=Yard[CentralSlots[Index]];
    if (CentralReservation!=INDEX_NONE || BlocksBusy[Slot.Block] || RMGBusy[Slot.Block*2] || RMGBusy[Slot.Block*2+1]) return false;
    // Reserve only the yard block; traffic paths are reserved independently.
    CentralReservation=Index; CentralPending=INDEX_NONE;
    BlocksBusy[Slot.Block]=true;
    return true;
}
void APortSiteLogistics::CompleteCentral(int32 Index,bool Occupied)
{
    check(CentralReservation==Index);
    auto& Slot=Yard[CentralSlots[Index]];
    Slot.Occupied=Occupied; BlocksBusy[Slot.Block]=false;
    CentralReservation=CentralPending=INDEX_NONE;
}
