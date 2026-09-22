#include "PortSimTimeStep.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"

bool UPortSimTimeStep::Initialize(UEngine* InEngine)
{
    bInitialized=true;
    LastFrameWall=SampleStart=FPlatformTime::Seconds();
    SampleSimulation=0;
    return true;
}

void UPortSimTimeStep::Shutdown(UEngine* InEngine)
{
    bInitialized=false;
}

void UPortSimTimeStep::SetPlaybackRate(float Rate)
{
    PlaybackRate=FMath::Clamp(Rate,1.f,16.f);
}

ECustomTimeStepSynchronizationState UPortSimTimeStep::GetSynchronizationState() const
{
    return bInitialized?ECustomTimeStepSynchronizationState::Synchronized:ECustomTimeStepSynchronizationState::Closed;
}

bool UPortSimTimeStep::UpdateTimeStep(UEngine* InEngine)
{
    if (!bInitialized) return true;
    // Keep presentation pacing independent of playback. Accelerate simulation time,
    // not the required render/engine frame rate (16x must not require 960 FPS).
    const double Deadline=LastFrameWall+StepSeconds;
    double Now=FPlatformTime::Seconds();
    if (Now<Deadline)
    {
        FPlatformProcess::SleepNoStats(static_cast<float>(Deadline-Now));
        Now=FPlatformTime::Seconds();
    }
    const double SimulationDelta=FMath::Clamp((Now-LastFrameWall)*PlaybackRate,0.0,MaxSimulationDelta);
    LastFrameWall=Now;
    UpdateApplicationLastTime();
    FApp::SetDeltaTime(SimulationDelta);
    FApp::SetCurrentTime(FApp::GetLastTime()+SimulationDelta);
    SampleSimulation+=SimulationDelta;
    if (Now-SampleStart>=0.5)
    {
        AchievedRate=static_cast<float>(SampleSimulation/(Now-SampleStart));
        SampleStart=Now; SampleSimulation=0;
    }
    return false;
}
