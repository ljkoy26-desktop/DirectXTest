# DirectX8 빌드 사전 준비

## 원인

`DirectX8.cpp`는 `d3d8.h`를 포함하고 `d3d8.lib`를 링크합니다.
그러나 Direct3D8은 Windows 10/11 SDK(`WindowsTargetPlatformVersion 10.0`)에 포함되어 있지 않습니다.
현재 프로젝트(`DirectX8.vcxproj`)에도 `AdditionalIncludeDirectories`, `AdditionalLibraryDirectories`가 지정되어 있지 않아,
컴파일러가 `d3d8.h`를 찾지 못하거나(`fatal error C1083`) 링커가 `d3d8.lib`를 찾지 못하는(`LNK1104`) 오류가 발생합니다.

저장소 루트의 `include`, `lib64` 폴더는 비어 있어, 별도의 레거시 SDK 파일이 아직 배치되지 않은 상태입니다.

## 필요한 파일

- `d3d8.h`, `d3d8caps.h` (헤더)
- `d3d8.lib` (임포트 라이브러리)

위 파일은 **레거시 DirectX SDK (예: DirectX SDK - June 2010)** 에 포함되어 있습니다.
최신 Windows SDK에서는 제공되지 않으므로 별도로 구해야 합니다.

- 헤더 위치 (SDK 설치 기준): `Include\d3d8.h`, `Include\d3d8caps.h`
- 라이브러리 위치: `Lib\x86\d3d8.lib` (32비트), `Lib\x64\d3d8.lib` (64비트)

## 설정 방법

1. 확보한 `d3d8.h`, `d3d8caps.h`를 저장소의 `include\` 폴더에 복사합니다.
2. 확보한 `d3d8.lib`(플랫폼에 맞는 버전)를 `lib64\` 폴더(또는 Win32/x64를 구분할 폴더)에 복사합니다.
3. `DirectX8.vcxproj`를 열어 아래 항목을 설정합니다 (프로젝트 속성 > VC++ 디렉터리, 또는 `.vcxproj`의 `ItemDefinitionGroup`에 직접 추가).
   - **추가 포함 디렉터리(Additional Include Directories)**: `$(SolutionDir)..\..\..\include` (또는 헤더를 복사한 실제 경로)
   - **추가 라이브러리 디렉터리(Additional Library Directories)**: `$(SolutionDir)..\..\..\lib64` (또는 라이브러리를 복사한 실제 경로)
4. Win32/x64 플랫폼별로 `d3d8.lib` 경로가 다르면 `Debug|Win32`, `Debug|x64` 등 구성별로 각각 지정해야 합니다.

## 참고

- `ddraw.h`(DirectX7)와 `d3d11.h`(DirectX11)는 Windows 10/11 SDK에 기본 포함되어 있어 별도 설정 없이 빌드됩니다. DirectX8만 예외입니다.
- 레거시 SDK를 설치하지 않고 헤더/라이브러리 파일만 필요하다면, 다른 PC의 기존 DirectX SDK 설치 폴더에서 위 4개 파일만 복사해도 됩니다.
