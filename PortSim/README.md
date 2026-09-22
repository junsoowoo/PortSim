# PortSim 폴더 안내

언리얼 엔진 5.6.1 프로젝트입니다. `PortSim.uproject`를 열거나 UnrealBuildTool로 빌드합니다. CMake 설정은 제거되었습니다.

## 현재 사용하는 폴더

- **Source/**: 언리얼이 컴파일하는 C++ 코드와 빌드 규칙입니다. `PortSim/` 아래에 컨테이너, AGV, RMG, 선박, STS 액터 및 자동화 로직이 있습니다. `PortSim.Build.cs`는 모듈 의존성, `*.Target.cs`는 게임/에디터 빌드 대상을 정의합니다.
- **Config/**: 입력, 기본 맵, 게임 모드, 렌더링 등 프로젝트 공통 설정입니다. `Default*.ini`를 Git에 포함합니다.
- **Content/**: 언리얼 에셋입니다. 모델과 재질, 원본 파일은 아래의 `PortSim/Assets/`에 모았습니다.
- **Scripts/**: 실행 및 검증 도구입니다. `LaunchCrane.ps1`은 실행, `TestCrane.ps1`과 `TestTerminal.ps1`은 동작 테스트, `import_models.py`는 컨테이너 원본 모델을 임포트합니다.

## 통합 에셋 폴더

`Content/PortSim/Assets/`가 에셋 보관 위치입니다. 언리얼 콘텐츠 브라우저에서는 `/Game/PortSim/Assets`로 표시됩니다.

- **Models/Container_Quaternius/**: 컨테이너 메시와 해당 재질입니다. 독립 컨테이너 액터의 외형으로 사용됩니다.
- **Materials/**: 크레인, 야드, 선박 등에 적용하는 프로젝트 공통 재질입니다.
- **SourceFiles/**: 원본 `.glb` 모델과 `LICENSES.md`입니다. 기존 `SourceAssets/FreeModels/`에서 이동했습니다. 에디터 재임포트 경로도 이 위치를 사용합니다.

현재 별도 선박·AGV·RMG 모델 파일은 없습니다. 선박은 `Source/PortSim/PortShipActor.cpp`, AGV는 `PortAGVActor.cpp`, RMG는 `PortRMGActor.cpp`, STS는 `QuayCrane.cpp`에서 기본 메시로 형상을 만듭니다. 향후 외부 선박 모델을 추가할 때는 원본을 `SourceFiles/`, 임포트한 에셋을 `Models/Ships/`에 배치하면 됩니다.

## 자동 생성 폴더

다음 폴더는 정리 과정에서 삭제했습니다. 빌드하거나 실행하면 다시 생성되며 Git에는 올리지 않습니다.

- **Binaries/**: 빌드된 실행 파일, DLL, 디버그 심볼.
- **Intermediate/**: UnrealHeaderTool 생성 코드, 컴파일 중간 파일, 생성된 IDE 프로젝트.
- **DerivedDataCache/**: 메시·텍스처·셰이더 등의 재생성 가능한 파생 데이터.
- **Saved/**: 로그, 자동 저장, 스크린샷, 로컬 설정, 테스트 결과와 임포트 보고서.

## 루트 파일과 GitHub 관리

- **PortSim.uproject**: 엔진 연결, 모듈, 플러그인 정보. `EngineAssociation: "5.6"`은 설치된 5.6.1 엔진을 사용합니다.
- **PortSim.sln**: 생성되는 Visual Studio 솔루션으로 정리 시 삭제했습니다. 필요하면 프로젝트 파일을 다시 생성합니다.
- **.vsconfig**: 공유할 Visual Studio 구성 요소 목록. Git에 포함합니다.
- **.gitignore**: 빌드 결과, 캐시, 개인 IDE 설정, 로그 등을 제외합니다.
- **.gitattributes**: 텍스트 줄바꿈과 에셋의 바이너리 처리를 지정합니다.

`Source/`, `Config/`, `Content/`, `Scripts/`, 원본 모델과 라이선스, `.uproject`는 Git에 포함합니다. `*.ini` 또는 `*.json` 전체를 무시하지 않습니다. 향후 `Build/`의 배포 리소스나 소스 플러그인이 필요할 수 있으므로 이 폴더 전체도 무시하지 않습니다. 플러그인의 `Binaries/`와 `Intermediate/`는 제외됩니다.

이미 추적 중이던 생성 파일은 Git 인덱스에서 제외했고, 사용하지 않는 로컬 생성 파일도 삭제했습니다. 이 변경을 커밋하면 이후 업로드에서 빠집니다. 기존 커밋 기록에 포함된 파일까지 제거하는 작업은 수행하지 않았습니다.


미사용 C++ 예제(app, automation, core, crane, ship), 참고용 크레인 모델과 원본, 일회성 에셋 이동 스크립트, 빈 폴더를 삭제했습니다. 실제 STS·RMG·선박·AGV 액터 코드는 Source에 유지합니다. Binaries를 삭제했으므로 다음 실행 전에 에디터 모듈을 다시 빌드해야 합니다.
