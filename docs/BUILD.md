# 빌드 메모

`README.md` 의 빌드 절로 충분할 때는 이 문서를 볼 필요가 없습니다.

엔진(.vcxproj) 에 붙이거나 새 PC 에서 처음 구성할 때의 메모입니다.

## 의존성 (vcpkg 매니페스트)

`vcpkg.json` 이 전부입니다: directx-headers, directxtex, directx-dxc, assimp.

루트 `CMakeLists.txt` 가 `-DCMAKE_TOOLCHAIN_FILE` 이 없으면 `VCPKG_ROOT` 환경변수에서 vcpkg 툴체인을 잡습니다.

기본 트리플릿은 `x64-windows-static-md` (프리셋에 고정) 라 런타임에 따라가는 DLL 은 `dxcompiler.dll` 과 `dxil.dll` 뿐입니다.

## 새 PC 체크리스트

1. **vcpkg 클론**: `git clone https://github.com/microsoft/vcpkg <경로>` 후 `<경로>/bootstrap-vcpkg.bat`, 환경변수 `VCPKG_ROOT=<경로>`.
   얕은 클론(`--depth`) 금지: `vcpkg.json` 의 `builtin-baseline` 커밋이 히스토리에 있어야 합니다. 오래된 클론이면 `git pull` 뒤 `bootstrap-vcpkg.bat` 을 다시 (vcpkg.exe 가 옛 버전이면 "document schema version 2 is not supported" 로 구성 실패).
2. **Visual Studio 2022 또는 2026** (C++ 데스크톱 워크로드). CLI 로 빌드하려면 cmake 가 PATH 에 있어야 하고, 없으면 VS 번들 cmake 를 그대로 써도 됩니다. 한 빌드 폴더에 cmake 버전을 섞어 쓰지 마세요 (캐시가 깨집니다).
3. **경로.** 기본 프리셋 (Visual Studio 제너레이터 + 정적 트리플릿) 은 비ASCII 경로 (한글 등) 에서도 빌드됩니다. Ninja 같은 다른 제너레이터로 바꾸면 의존성 링크가 실패할 수 있으니 그때는 영문 경로를 쓰세요.
4. `cmake --preset vs2022-x64` 가 첫 구성 때 `build/<프리셋>/vcpkg_installed/` 에 의존성을 받아 빌드합니다 (첫 회 수 분, assimp / directxtex 소스 빌드). 이후 패키지 추가는 `vcpkg.json` 한 줄입니다.

VS Code 는 CMake Tools 가 `CMakePresets.json` 을 읽어 configure preset `vs2022-x64`, build preset `debug` / `release` 를 그대로 보여줍니다.

## 엔진 (Visual Studio .vcxproj) 으로 가져갈 때

1. VS 설치 구성 요소에 "vcpkg 패키지 관리자" 체크 (VS 번들 vcpkg 사용, `integrate install` 불필요).
2. 솔루션 옆에 `vcpkg.json` 을 두고 프로젝트 속성 > vcpkg > "Use Vcpkg Manifest = Yes", 트리플릿 `x64-windows-static-md`.
3. 컴파일 옵션 `/utf-8 /permissive-`, C++20. include 경로는 `Hakko/*/public` 만 (내부 구현은 `private`).
4. **디폴트 셰이더 폴더와 `dxil.dll` 은 빌드 후 이벤트로 직접 복사** (`<exe>/Render/D3D12/shader`, applocal 이 못 잡음). `dxil.dll` 이 빠지면 디버그 레이어가 셰이더를 전부 거부합니다. CMake 쪽에서는 `hakko_deploy(<target>)` 이 이 일을 합니다.

## 셰이더

DXC 런타임 컴파일 (SM 6.0 기본, 디바이스 지원 최고치 6.6 까지 자동). 변형 매크로:
- VS: `VERTEX_STATIC` / `VERTEX_SKINNED` / `VERTEX_SIMPLE`
- PS: `OUTPUT_DEPTHONLY` / `OUTPUT_COLOR` / `OUTPUT_COLORID` / `OUTPUT_GBUFFER` (컴파일 뒤 리플렉션으로 SV_TARGET 집합 검증)
