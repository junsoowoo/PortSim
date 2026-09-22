#include "PortShipActor.h"
#include "TerminalLayout.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APortShipActor::APortShipActor()
{
    Box(TEXT("ShipHull"),RootComponent,FVector(0,0,-95),FVector(1700,8200,550),true);
    Box(TEXT("ShipDeck"),RootComponent,FVector(0,0,190),FVector(1600,8000,20),true);
    Box(TEXT("ShipBow1"),RootComponent,FVector(0,4350,-95),FVector(1300,500,550),true);
    Box(TEXT("ShipBow2"),RootComponent,FVector(0,4730,-95),FVector(750,260,550),true);
    Box(TEXT("ShipBow3"),RootComponent,FVector(0,4930,-95),FVector(250,180,550),true);
    Box(TEXT("ShipStern"),RootComponent,FVector(0,-4390,-95),FVector(1400,580,550),true);
    Box(TEXT("ShipBridge"),RootComponent,FVector(0,-4250,530),FVector(1050,600,700),true);
    Box(TEXT("ShipBridgeWindows"),RootComponent,FVector(0,-4250,800),FVector(1150,650,110),true);
    Box(TEXT("ShipFunnel"),RootComponent,FVector(-250,-4420,1080),FVector(200,220,400),true);
    Tags.Add(TEXT("PortSim.Ship"));
}
