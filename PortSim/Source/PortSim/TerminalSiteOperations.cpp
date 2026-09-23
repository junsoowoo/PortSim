#include "QuayCrane.h"
#include "PortWorkingCrane.h"
#include "PortSiteLogistics.h"
#include "PortAGVActor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/SpringArmComponent.h"

void AQuayCrane::ResetSiteOperations()
{
    if (SiteLogistics) SiteLogistics->ResetLogistics();
}
void AQuayCrane::TickSiteOperations(float Dt)
{
    if (!SiteLogistics || bTerminalTest) return;
    const bool Testing=FParse::Param(FCommandLine::Get(),TEXT("PortSimSiteTest"));
    if (Testing) SiteLogistics->DispatchLimit=8;
    SiteLogistics->Advance(Dt,bAutoPaused || bEmergencyStop);
    if (Testing) TickSiteTest(Dt);
}
void AQuayCrane::TickSiteTest(float Dt)
{
    SiteTestTime+=Dt;
    auto Finish=[](bool Pass,const FString& Why)
    {
        UE_LOG(LogTemp,Display,TEXT("PORTSIM_SITE_%s: %s"),Pass?TEXT("PASS"):TEXT("FAIL"),*Why);
        FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
    };
    FString Error;
    if (!SiteLogistics->Validate(Error)) { Finish(false,Error); return; }
    if (SiteTestTime>14000.f) { Finish(false,TEXT("Integrated logistics timeout")); return; }
    bool Moving=false;
    for (const auto& Vehicle:SiteLogistics->Vehicles) Moving|=Vehicle->Speed>1.f;
    auto Freeze=[&](bool Emergency)
    {
        bAutoPaused=!Emergency; bEmergencyStop=Emergency;
        SiteLogistics->Advance(0,true);
        SiteTestPositions=SiteLogistics->Snapshot(); SiteTestHold=SiteTestTime;
    };
    if (SiteTestStage==0 && Moving) { Freeze(false); SiteTestStage=1; }
    else if ((SiteTestStage==1 || SiteTestStage==3) && SiteTestTime-SiteTestHold>1.f)
    {
        const auto Current=SiteLogistics->Snapshot();
        if (Current.Num()!=SiteTestPositions.Num()) { Finish(false,TEXT("Pause changed actors")); return; }
        for (int32 I=0;I<Current.Num();++I)
            if (!Current[I].Equals(SiteTestPositions[I],.1f)) { Finish(false,TEXT("Pause/E-stop moved AGV, crane or cargo")); return; }
        bAutoPaused=bEmergencyStop=false; ++SiteTestStage;
    }
    else if (SiteTestStage==2 && Moving) { Freeze(true); SiteTestStage=3; }
    else if (SiteTestStage==4 && Moving)
    {
        SiteLogistics->ResetLogistics();
        if (SiteLogistics->Delivered || SiteLogistics->InTransit() || !SiteLogistics->Validate(Error))
        { Finish(false,TEXT("In-transit reset: ")+Error); return; }
        SiteTestStage=5;
    }
    else if (SiteTestStage==5 && SiteLogistics->Delivered==8 && SiteLogistics->IsIdle())
    {
        for (const auto& Vehicle:SiteLogistics->Vehicles)
            if (Vehicle->CompletedJobs!=1) { Finish(false,TEXT("Not all eight AGVs performed a handover")); return; }
        const FString Summary=FString::Printf(TEXT("yard %d -> %d (removed %d = all vessel cargo); 8 complete STS/AGV/RMG shipments with same IDs; road/slot reservations, physical placement, return, pause/E-stop and in-transit reset"),
            SiteLogistics->BaselineYard,SiteLogistics->InitialYard,SiteLogistics->InitialShipCount());
        SiteLogistics->ResetLogistics();
        if (!SiteLogistics->Validate(Error) || SiteLogistics->Delivered || SiteLogistics->InTransit())
        { Finish(false,TEXT("Completed reset: ")+Error); return; }
        SiteTestStage=6; Finish(true,Summary);
    }
}
void AQuayCrane::FocusNextSiteCrane()
{
    if (WorkingCranes.IsEmpty()) return;
    SiteCameraIndex=(SiteCameraIndex+1)%WorkingCranes.Num();
    const auto* Crane=WorkingCranes[SiteCameraIndex].Get();
    CameraArm->SetRelativeLocation(Crane->GetActorLocation()+FVector(0,0,Crane->bSTS?2300:800));
    CameraArm->TargetArmLength=Crane->bSTS?20000.f:10500.f;
    CameraArm->SetRelativeRotation(FRotator(-35,Crane->bSTS?38.f:128.f,0));
}
