#include "QuayCrane.h"
#include "PortSupportVehicle.h"
#include "PortSiteLogistics.h"
#include "PortWorkingCrane.h"
#include "PortAGVActor.h"
#include "Engine/World.h"

void AQuayCrane::BuildSupportFleet()
{
    FActorSpawnParameters Params; Params.Owner=this;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto Spawn=[&](EPortSupportType Type,int32 Number,FVector Metres)
    {
        auto* Vehicle=GetWorld()->SpawnActor<APortSupportVehicle>(Metres*100,FRotator::ZeroRotator,Params);
        Vehicle->Configure(Type,Number); SupportFleet.Add(Vehicle);
    };
    for (int32 I=0;I<74;++I) Spawn(EPortSupportType::YardChassis,I+1,FVector(390+(I%15)*15,435+(I/15)*10,0));
    for (int32 I=0;I<18;++I) Spawn(EPortSupportType::YardTractor,I+1,FVector(680+(I%3)*25,340+(I/3)*10,0));
    for (int32 I=0;I<4;++I) Spawn(EPortSupportType::ReachStacker,I+1,FVector(680+I*20,280,0));
    for (int32 I=0;I<2;++I) Spawn(EPortSupportType::EmptyHandler,I+1,FVector(600+I*25,380,0));
    for (int32 I=0;I<7;++I) Spawn(EPortSupportType::Forklift,I+1,FVector(670+I*12,310,0));
    UE_LOG(LogTemp,Display,TEXT("EQUIPMENT_INVENTORY: CC=9 (24 rows), TC=46, AGV=60 active, RS=4, YT=18, EH=2, FL=7, YC=74; total=220"));
}
