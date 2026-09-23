#include "PortWorkingCrane.h"
#include "PortContainerActor.h"
#include "PortAGVActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

// Site cranes retain their kinematic, level-spreader transport model. These
// observations do not claim independent rope reactions or a sway/skew solver.
void APortWorkingCrane::ClearSTSState()
{
    AxisVelocity=FVector::ZeroVector;
    Observation=FSTSObservation(); NextSample=0;
    JobSeconds=PausedSeconds=LastJobSeconds=LastPausedSeconds=0;
    HandoverAGV=nullptr;
    for(bool& Locked:CornerLocked) Locked=false;
}

bool APortWorkingCrane::AGVAligned() const
{
    if(!bExternalJobs) return true;
    return IsValid(HandoverAGV) && HandoverAGV->Speed<=0.1f &&
        HandoverAGV->CargoPosition().Equals(Slots[1-SourceSlot],STSProfile.AGVTolerance) &&
        FMath::Abs(FMath::FindDeltaAngleDegrees(HandoverAGV->GetActorRotation().Yaw,Orientation.Rotator().Yaw))<=STSProfile.AGVHeadingTolerance;
}

bool APortWorkingCrane::CargoSupported() const
{
    if(!IsValid(CargoActor)) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SiteSTSSupport),false);
    Query.AddIgnoredActor(this); Query.AddIgnoredActor(CargoActor);
    for(int32 I=0;I<4;++I)
    {
        const FVector P=CargoActor->GetActorLocation()+Orientation.RotateVector(FVector((I&1)?100:-100,(I&2)?520:-520,0));
        FHitResult Hit;
        if(!GetWorld()->LineTraceSingleByChannel(Hit,P,P-FVector(0,0,129.5f+STSProfile.SupportTolerance),ECC_Visibility,Query) || Hit.ImpactNormal.Z<.8f) return false;
        if(bExternalJobs && Stage==5 && Hit.GetActor()!=HandoverAGV) return false;
    }
    return true;
}

void APortWorkingCrane::SampleSTS(bool Force)
{
    if(!STSProfile.bReady || !IsValid(CargoActor) || (!Force && SimulationTime<NextSample)) return;
    NextSample=SimulationTime+STSProfile.SensorPeriod;
    auto& S=Observation; S.Timestamp=SimulationTime; S.bValid=!bSensorFault;
    S.DrivePosition=Head+FVector(STSProfile.PositionBias);
    S.DriveVelocity=AxisVelocity;
    S.SpreaderPosition=HeadPosition()+Orientation.RotateVector(FVector(STSProfile.PositionBias));
    S.SpreaderVelocity=Orientation.RotateVector(AxisVelocity);
    S.CargoPosition=CargoActor->GetActorLocation();
    S.CargoVelocity=bCarrying?S.SpreaderVelocity:CargoActor->GetBody()->GetPhysicsLinearVelocity();
    const FVector Gap=Orientation.UnrotateVector(S.SpreaderPosition-S.CargoPosition);
    S.bLanded=FMath::Abs(Gap.X)<=STSProfile.LandingTolerance && FMath::Abs(Gap.Y)<=STSProfile.LandingTolerance &&
        FMath::Abs(Gap.Z-154.5f)<=STSProfile.SeatingTolerance &&
        (S.SpreaderVelocity-S.CargoVelocity).Size()<=STSProfile.SettleSpeed &&
        FQuat::ErrorAutoNormalize(Orientation,CargoActor->GetActorQuat())<.02f;
    const FVector CoG=CargoActor->CoGOffsetCm;
    const float Weight=bCarrying?FMath::Max(0.f,CargoActor->MassKg+STSProfile.MassBiasKg)*9.80665f:0;
    for(int32 I=0;I<4;++I)
    {
        S.Locked[I]=CornerLocked[I];
        S.CornerLoadsN[I]=Weight*(.5f+((I&1)?1:-1)*CoG.X/200.f)*(.5f+((I&2)?1:-1)*CoG.Y/1040.f);
    }
    S.bAGVAligned=AGVAligned(); S.bCargoSupported=CargoSupported(); S.SwayDegrees=0;
}

bool APortWorkingCrane::MoveSTS(FVector Target,float Dt)
{
    if(!STSProfile.ContainsTarget(Target)) { Stop(TEXT("Automatic target outside STS envelope")); return false; }
    float Hoist=STSProfile.HoistLimit(bCarrying?CargoActor->MassKg:0,bCarrying);
    if(Stage==2 || Stage==5)
    {
        const float Distance=FMath::Abs(Target.Z-Observation.DrivePosition.Z);
        Hoist=FMath::Min(Hoist,FMath::Sqrt(FMath::Square(STSProfile.ApproachSpeed)+
            2.f*STSProfile.HoistAcceleration*FMath::Max(0.f,Distance-STSProfile.ApproachDistance)));
    }
    const FVector Limits(STSProfile.TrolleySpeed,STSProfile.GantrySpeed,Hoist);
    const FVector Accelerations(STSProfile.TrolleyAcceleration,STSProfile.GantryAcceleration,STSProfile.HoistAcceleration);
    for(int32 I=0;I<3;++I)
    {
        const double Error=Target[I]-Observation.DrivePosition[I];
        const double Desired=FMath::Clamp(.8*Error-.8*Observation.DriveVelocity[I],-Limits[I],Limits[I]);
        AxisVelocity[I]=FMath::FInterpConstantTo(AxisVelocity[I],Desired,double(Dt),Accelerations[I]);
        Head[I]+=AxisVelocity[I]*Dt;
    }
    Head.X=FMath::Clamp(Head.X,double(STSProfile.MinTrolley()),double(STSProfile.MaxTrolley()));
    Head.Z=FMath::Clamp(Head.Z,-double(STSProfile.LiftBelowRail),double(STSProfile.LiftAboveRail));
    UpdateParts();
    // Position is never snapped to a target to conceal a biased observation.
    const bool Ready=Observation.DrivePosition.Equals(Target,.1f) && Observation.DriveVelocity.Size()<.1f;
    if(Ready) AxisVelocity=FVector::ZeroVector;
    return Ready;
}
