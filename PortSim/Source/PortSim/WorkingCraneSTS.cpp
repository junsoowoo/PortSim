#include "PortWorkingCrane.h"
#include "PortContainerActor.h"
#include "PortAGVActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

// Site STS suspension integrates sway/yaw; sensor observations remain synthetic.
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
        (Stage<5 || STSSensorContains(TEXT("agv_position_lidar"),HandoverAGV->CargoPosition())) &&
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
    S.SpreaderVelocity=SpreaderVelocity();
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
    S.bAGVAligned=AGVAligned(); S.bCargoSupported=CargoSupported(); S.SwayDegrees=SuspensionState.SwayDegrees();
    for(const auto& Mount:STSProfile.Dynamics.Mounts)
    {
        double Value=0; bool Required=true;
        if(Mount.Key==TEXT("trolley_encoder"))
        {
            Value=Mount.ReadPosition(S.DrivePosition.X*.01);
            S.DrivePosition.X=(Value+Mount.MeasurementOrigin)*100;
            if(Mount.MaxTraversingSpeed>0 && FMath::Abs(S.DriveVelocity.X)*.01>Mount.MaxTraversingSpeed) S.bValid=false;
        }
        else if(Mount.Key==TEXT("hoist_encoder")) Value=(BeamZ-Head.Z)*.01;
        else if(Mount.Key==TEXT("twistlock_load")) for(double Load:S.CornerLoadsN) {if(Load<Mount.Minimum || Load>Mount.Maximum) S.bValid=false;}
        else Required=false;
        if(Required && Mount.Key!=TEXT("twistlock_load") && (Value<Mount.Minimum || Value>Mount.Maximum)) S.bValid=false;
    }
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
    const auto& C=STSProfile.Dynamics;
    Hoist=FMath::Min(Hoist,float(C.MotorMaxRPM*2*PI/60*C.DrumRadius/(C.Parts*C.GearRatio)*100));
    const FVector Limits(STSProfile.TrolleySpeed,STSProfile.GantrySpeed,Hoist);
    const FVector Accelerations(STSProfile.TrolleyAcceleration,STSProfile.GantryAcceleration,STSProfile.HoistAcceleration);
    const double Payload=bCarrying?CargoActor->MassKg:0, Mass=STSProfile.SpreaderMassKg+Payload;
    const FVector CoG=bCarrying?CargoActor->CoGOffsetCm*.01*(Payload/Mass):FVector::ZeroVector;
    for(double Remaining=Dt;Remaining>1.e-9;)
    {
        const double Step=FMath::Min(Remaining,1./120.);Remaining-=Step;
        const FVector Previous=AxisVelocity;
        const double Length=FMath::Max(.1,(BeamZ-Head.Z)*.01);
        for(int32 I=0;I<3;++I)
        {
            const double Error=Target[I]-(Head[I]+SuspensionState.Offset[I]*100+STSProfile.PositionBias);
            double Desired=.8*Error-.8*AxisVelocity[I];

            double Acceleration=Accelerations[I];
            if(I==2)
            {
                const double ShaftFactor=C.Parts*C.GearRatio/C.DrumRadius;
                const double BaseTorque=Mass*9.80665/(ShaftFactor*C.Efficiency*C.MotorCount);
                const double TorqueLimit=FMath::Min(C.MotorMaxTorque,STSProfile.HoistPowerW/(C.MotorCount*FMath::Max(1.,FMath::Abs(AxisVelocity.Z*.01*ShaftFactor))));
                if(BaseTorque>C.MotorMaxTorque) {Stop(TEXT("Hoist motor cannot hold suspended mass"));return false;}
                const double AccelLimit=FMath::Max(0.,(TorqueLimit-BaseTorque)/(Mass/(ShaftFactor*C.Efficiency*C.MotorCount)+C.MotorInertia*ShaftFactor));
                Acceleration=FMath::Min(Acceleration,AccelLimit*100.);
            }
            if(I<2)
            {
                // Acceleration feedback damps the pendulum; velocity feedback here adds phase lag.
                const double Command=C.HorizontalAcceleration(Error*.01,AxisVelocity[I]*.01,Length,SuspensionState.Rate[I])*100;
                AxisVelocity[I]=FMath::Clamp(AxisVelocity[I]+FMath::Clamp(Command,-Acceleration,Acceleration)*Step,-double(Limits[I]),double(Limits[I]));
            }
            else AxisVelocity[I]=FMath::FInterpConstantTo(AxisVelocity[I],FMath::Clamp(Desired,-double(Limits[I]),double(Limits[I])),Step,Acceleration);
            Head[I]+=AxisVelocity[I]*Step;
        }
        SuspensionState.Step(C,Step,Length,-AxisVelocity.Z*.01,(AxisVelocity-Previous)*(.01/Step),AxisVelocity*.01,Mass,CoG,STSProfile.HoistPowerW);
        if(!SuspensionState.Fault.IsEmpty()) {Stop(SuspensionState.Fault);return false;}
    }
    SuspendedOffset=SuspensionState.Offset*100;
    Head.X=FMath::Clamp(Head.X,double(STSProfile.MinTrolley()),double(STSProfile.MaxTrolley()));
    Head.Z=FMath::Clamp(Head.Z,-double(STSProfile.LiftBelowRail),double(STSProfile.LiftAboveRail));
    UpdateParts();
    const float PositionTolerance=(Stage==2 || Stage==5)?.5f:5.f;
    return (Head+SuspendedOffset+FVector(STSProfile.PositionBias)).Equals(Target,PositionTolerance) && SpreaderVelocity().Size()<PositionTolerance &&
        SuspensionState.SwayDegrees()<STSProfile.SwayLimitDegrees && FMath::Abs(FMath::RadiansToDegrees(SuspensionState.Yaw))<C.SkewLimit;
}

FVector APortWorkingCrane::SpreaderVelocity() const
{ return Orientation.RotateVector(AxisVelocity+SuspensionState.OffsetVelocity*100); }

void APortWorkingCrane::BuildSTSSensors()
{
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    for(const auto& Mount:STSProfile.Dynamics.Mounts)
    {
        auto* Parent=Mount.Frame==TEXT("spreader")?Spreader.Get():(Mount.Frame==TEXT("trolley")?Trolley.Get():RootComponent.Get());
        auto* Marker=NewObject<UStaticMeshComponent>(this,*FString::Printf(TEXT("Sensor_%s"),*Mount.Key));
        Marker->SetStaticMesh(Cube); Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Marker->SetupAttachment(Parent); Marker->SetAbsolute(false,false,true);
        Marker->SetRelativeLocation(Mount.Position*100/Parent->GetComponentScale()); Marker->SetRelativeRotation(Mount.Rotation);
        Marker->SetWorldScale3D(FVector(.14)); Marker->RegisterComponent(); AddInstanceComponent(Marker); SensorMarkers.Add(Marker);
        for(int32 ExtraIndex=0;ExtraIndex<Mount.AdditionalPositions.Num();++ExtraIndex)
        {
            auto* Extra=NewObject<UStaticMeshComponent>(this,*FString::Printf(TEXT("Sensor_%s_extra_%d"),*Mount.Key,ExtraIndex));
            Extra->SetStaticMesh(Cube); Extra->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Extra->SetupAttachment(Parent); Extra->SetAbsolute(false,false,true);
            Extra->SetRelativeLocation(Mount.AdditionalPositions[ExtraIndex]*100/Parent->GetComponentScale()); Extra->SetRelativeRotation(Mount.Rotation);
            Extra->SetWorldScale3D(FVector(.14)); Extra->RegisterComponent(); AddInstanceComponent(Extra);
        }
        if(Mount.Key==TEXT("twistlock_load") || Mount.Key==TEXT("twistlock_state") || Mount.Key==TEXT("landed"))
            for(int32 Corner=0;Corner<3;++Corner)
            {
                auto* Extra=NewObject<UStaticMeshComponent>(this,*FString::Printf(TEXT("Sensor_%s_%d"),*Mount.Key,Corner));
                Extra->SetStaticMesh(Cube); Extra->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Extra->SetupAttachment(Parent); Extra->SetAbsolute(false,false,true);
                const FVector P((Corner&1)?Mount.Position.X:-Mount.Position.X,(Corner&2)?Mount.Position.Y:-Mount.Position.Y,Mount.Position.Z);
                Extra->SetRelativeLocation(P*100/Parent->GetComponentScale()); Extra->SetRelativeRotation(Mount.Rotation);
                Extra->SetWorldScale3D(FVector(.14)); Extra->RegisterComponent(); AddInstanceComponent(Extra);
            }
    }
}

bool APortWorkingCrane::STSSensorContains(const FString& Key,FVector Point) const
{
    const int32 Index=STSProfile.Dynamics.Mounts.IndexOfByPredicate([&](const FSTSSensorMount& M){return M.Key==Key;});
    if(!SensorMarkers.IsValidIndex(Index)) return false;
    const auto& M=STSProfile.Dynamics.Mounts[Index]; const auto* Marker=SensorMarkers[Index].Get();
    TArray<FVector> Origins;Origins.Add(Marker->GetComponentLocation());
    for(const FVector& Local:M.AdditionalPositions) Origins.Add(Marker->GetAttachParent()->GetComponentLocation()+Marker->GetAttachParent()->GetComponentQuat().RotateVector(Local*100));
    for(const FVector& Origin:Origins)
    {
        const FVector Delta=Point-Origin;const double Distance=Delta.Size()*.01;
        if(Distance>=M.Minimum && Distance<=M.Maximum && FVector::DotProduct(Delta.GetSafeNormal(),Marker->GetForwardVector())>=FMath::Cos(FMath::DegreesToRadians(M.Fov*.5)))return true;
    }
    return false;
}
