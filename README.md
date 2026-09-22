1. ui 추가
1-1 컨테이너 클릭 시, 무게/화물종류/날짜 등 컨테이너 정보 UI 추가
1-2 장비 클릭 시, 장비의 작동 시간/작업량 등 작업 정보 UI 추가 

2. 다양한 경로로 컨테이너 이동
2-1 다음 작업 스케줄 시스템 (RMG가 다음 작업 할 장소로 컨테이너 이동)

3. 컨테이너 랜덤
3-1 컨테이너 무게 다양화
3-2 컨테이너 무게중심 다양화
3-3 컨테이너 화물종류 다양화

4 항만 사이즈
4-1 실제 항만 사이즈 만큼 프로젝트 키우기
| 기준 항만           	|                    부산항 신항 7부두 DGT 	|
| 전체 안벽           	|                        **1,050m** 		|
| 전체 터미널 면적       |                      **837,000㎡** 		|
| 실제 CY 면적        	|                      **517,000㎡** 		|
| 전체 깊이 참고값       |                        약 **800m** 		|
| 선석              		|                             **3** 			|
| STS             		|                            **9대** 			|
| AGV             		|                           **60대** 		|
| ARMG            		|                           **46대** 		|
| 자동화 Yard Block  	|                         약 **23개** 		|
| Yard Row        		|                           **10열** 		|
| Yard Tier       		|                            **6단** 			|
| STS 인양 높이       	|                           **53m** 		|
| STS 전체 높이 참고    |                         약 **93m** 		|
| ARMG 높이 참고      	|                         약 **34m** 		|
| 컨테이너            	| **40ft, 12.192 × 2.438 × 2.591m** 	|
| AGV / STS 운영 기준 	|                      **약 5대/STS** 		|


발생할 수 있는 문제점(여기에 작성해 주세요)
크레인 작업시간의 대부분은 와이어 흔들림 제어
해결 방법 : 와이어 흔들림 제어 시스템을 크레인에 추가
## Unreal Engine build and actors

This project uses Unreal Engine 5.6.1 and UnrealBuildTool (`PortSim.Build.cs` and the `.Target.cs` files). CMakeLists.txt files have been removed.

Each container and vehicle is a separate runtime actor with its own components and transform:
- `PortContainerActor.h/.cpp`: APortContainerActor (Container_C01 through Container_C24).
- `PortAGVActor.h/.cpp`: APortAGVActor (AGV_01 through AGV_03).
- `PortRMGActor.h/.cpp`: APortRMGActor (RMG_01).
- `PortShipActor.h/.cpp`: APortShipActor (Ship_01).
- `QuayCrane.h/.cpp`: AQuayCrane, the existing STS crane pawn (STS_01).
- `PortEquipmentActor.h/.cpp`: shared equipment construction helpers.

`TerminalActors.h` remains a compatibility include. Terminal orchestration stays in the existing crane simulation. During Play, independent actors appear in the World Outliner under PortSim/Containers and PortSim/Equipment; they are spawned at runtime, not saved as placed level actors.

Build from PowerShell:
```powershell
& 'C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat' PortSimEditor Win64 Development "-Project=$PWD\PortSim\PortSim.uproject" -WaitMutex -NoHotReloadFromIDE
```
Run from the repository root. The `-PortSimActorTest` game flag checks actor counts, component ownership, independent transforms, and reset behavior.

폴더 구성, 통합 에셋 위치와 GitHub 관리 방법은 [PortSim 폴더 안내](PortSim/README.md)를 참고하세요.
