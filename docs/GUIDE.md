# Hakko 가이드

허브 API 사용법, 엔진 연동 순서, 셰이더 계약 가이드 입니다.

\
빌드와 요구 사항은 [README](../README.md),

환경 세팅 메모는 [BUILD.md](BUILD.md) 를 확인하세요.

## 퀵스타트

```cpp
#include "DataHub_Interface.h"
#include "DataHub_FileLoader.h"
using namespace Hakko::DataHub;

// 1. 리소스 (파일 -> id), 이름은 경로 그대로. 색 텍스처는 sRGB 로 지정
MeshFileInfo info;
LoadMesh(L"res/Fox.gltf", &info);
LoadTexture(L"res/crate1_diffuse.png", TextureFileFormat_None, TextureType_2D, TextureColorSpace_SRGB);
LoadTexture(L"res/skybox.dds", TextureFileFormat_DDS, TextureType_Cube, TextureColorSpace_SRGB);

// 2. 프롭 (≒ 머터리얼): 기본 텍스처 슬롯 0 디퓨즈 / 1 노멀 / 2 ORM / 3 이미션
//    내 리소스는 이름으로, 예약 리소스는 매크로 id (이름 매크로 RESERVED_*_NAME 으로 조회해도 같음)
TexProp tp = {};
tp.texId[0] = GetTexId(L"res/crate1_diffuse.png");
tp.texId[1] = RESERVED_TEX_NONE_NORMAL_ID;
tp.texId[2] = RESERVED_TEX_NONE_ORM_ID;
tp.texId[3] = GetTexId(RESERVED_TEX_NONE_EMISSION_NAME);
CreateTexProp("crate", tp);

// 3. 만든 리소스들을 렌더러로 올리기: 만든 프레임에 한 번
RTB_UpdateResources up = {};
up.command = RTBCMD_Upload;
SendRTB(up);

// 4. 물체 데이터박스 (월드 행렬, 색, 거칠기 / 금속 배율)
//  스킨 메쉬는 본 행렬 데이터박스도 추가 (바인드 포즈 = 단위 행렬)
DtB_3DGeneral obj = {};
obj.worldMat = DirectX::XMMatrixTranspose(DirectX::XMMatrixScaling(0.01f, 0.01f, 0.01f));
obj.blendColor = { 1, 1, 1, 1 };  obj.roughness = 1.0f;  obj.metalic = 1.0f;
DataBoxID objId = AddDataBox(obj);

DtB_3DOptional bone = {};
for (size_t i = 0; i < SKIN_BONE_MAX; i++) bone.boneTransforms[i] = DirectX::XMMatrixIdentity();
DataBoxID boneId = AddDataBox(bone);

// 5. 레이어: 배경, 카메라, 전역 조명 (DirectXMath 행렬 사용, 명시적 전치를 요구함)
SetLayerBackground(GetTexId(L"res/skybox.dds"));

auto eye = DirectX::XMVectorSet(3.0f, 5.0f, -10.0f, 1.0f);
auto viewM = DirectX::XMMatrixLookAtLH(eye, DirectX::XMVectorZero(), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
auto projM = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), 16.0f / 9.0f, 0.1f, 1000.0f);

RTB_SetCamera cam = {};
cam.view = DirectX::XMMatrixTranspose(viewM);
cam.proj = DirectX::XMMatrixTranspose(projM);
cam.viewProj = DirectX::XMMatrixMultiplyTranspose(viewM, projM);
DirectX::XMStoreFloat3(&cam.eyePos, eye);
SendRTB(cam);

RTB_SetGlobalLight light = {};
light.ambient = { 0.2f, 0.2f, 0.2f, 1.0f };
light.color = { 1.0f, 1.0f, 1.0f, 1.0f };
light.direction = { -0.577f, -0.577f, 0.577f };     // 빛이 진행하는 방향
SendRTB(light);

// 6. 매 프레임: 지난 요청 비우고 다시 보내기 (id 는 해제로 바뀔 수 있으니 이름으로 매번 조회)
ClearOpaqueDrawRequests();
RTB_DrawOPQ req = {};
req.drawProp = RESERVED_DRAWPROP_OPAQUE_SKINNED_ID;
req.texProp  = GetTexPropId("crate");
req.meshId   = GetMeshId(L"res/Fox.gltf");
req.generalId = objId;
req.optionalId = boneId;
for (uint16_t s = 0; s < info.subMeshCount; s++) { req.subMeshId = s; SendRTB(req); }
```

드로우 단위는 서브메쉬 하나입니다.

\
리포트는 `HubReportList` (허브 입력 오류) 와 `RenderReportList` (렌더러) 에 쌓입니다.

읽는 시점은 아래 "엔진에 붙이기" 를 확인하세요.

### 계약 및 제한 사항

- **리소스는 만든 프레임에 업로드**, `Create*` / `Update*` / `Load*` 로 리소스를 생성한 프레임에 `RTB_UpdateResources` 를 보내야 합니다. (`RTBCMD_Upload` / `RTBCMD_Update`, 비트로 묶어 한 번에 보내도 OK) 대기 큐에 쌓아둔 채 그 사이에 리소스 해제가 실행될 경우 대기 중인 id 가 어긋납니다.
- **리소스 id 는 저장하지 않기**, 해제 (`Release*` 후 `RTBCMD_Release`) 는 몇 프레임 뒤에 실행되고, 해제 시에 기존 리소스의 id 가 바뀔 수 있습니다. 리소스는 이름으로 다시 조회하는 것을 권장합니다. (예약 리소스는 매크로 정의 그대로 사용하면 무방)
- **해제 요청한 리소스로 요청하지 않기**
- **데이터박스 값 바꾸기** 데이터박스 한 칸을 통째로 바꿀 땐 `UpdateDataBox(id, data)`, 세부 필드나 여러 칸에 접근 필요 시 `DataBoxCustomizer::GetDataBoxList<T>()` 로 받아서 직접 수정할 수 있으나, 리스트 길이가 바뀌는 동작은 직접 조작하지 않고 `Add / RemoveDataBox()` 를 이용합니다.

### 예약 리소스

허브 초기화 때 고정 id 로 등록되는 리소스들입니다.

\
릴리즈되지 않으므로 id 매크로를 그대로 써도 되고, 이름으로 조회도 가능합니다. (`DataHub_Resource.h`)

| 종류 | id | 이름 | 용도 |
|---|---|---|---|
| 텍스처 | `RESERVED_TEX_ERROR_ID` (0) | - | 잘못된 id 가 닿는 에러 텍스처 (마젠타). `TexProp` 의 빈 슬롯도 여기로 |
| 텍스처 | `RESERVED_TEX_NONE_DIFFUSE_ID` (2) | `None_Diffuse` | 흰색 |
| 텍스처 | `RESERVED_TEX_NONE_NORMAL_ID` (3) | `None_Normal` | 평평한 노멀 |
| 텍스처 | `RESERVED_TEX_NONE_ORM_ID` (4) | `None_ORM` | AO 1 / 거칠기 0.5 / 금속 0 |
| 텍스처 | `RESERVED_TEX_NONE_EMISSION_ID` (5) | `None_Emission` | 검정 |
| 셰이더 | `RESERVED_SHADER_WORLD_VS_ID` (6) | `Default_World_VS` | 기본 월드 VS (정점 타입별 변형) |
| 셰이더 | `RESERVED_SHADER_WORLD_OPQ_PS_ID` (7) | `Default_World_OPQ_PS` | PBR-lite 불투명 PS |
| 셰이더 | `RESERVED_SHADER_WORLD_TP_PS_ID` (8) | `Default_World_TP_PS` | PBR-lite 반투명 PS |
| drawProp | `RESERVED_DRAWPROP_OPAQUE_ID` (3) | `Default_Opaque` | PBR-lite 불투명 |
| drawProp | `RESERVED_DRAWPROP_TRANSLUCENT_ID` (4) | `Default_Translucent` | PBR-lite 알파 블렌드 |
| drawProp | `RESERVED_DRAWPROP_OPAQUE_SKINNED_ID` (5) | `Default_Opaque_Skinned` | + 스키닝 |
| drawProp | `RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_ID` (6) | `Default_Translucent_Skinned` | + 스키닝 |

\
이름이 `__` 로 시작하는 나머지 예약 항목 (기본 메쉬 / 쿼드, 투명 텍스처 (1), 스카이박스 / 배경판 prop, 합성 / 배경용 셰이더 등) 은 렌더러 내부용입니다.

### 렌더러 옵션

```cpp
#include "RenderFactory.h"

Hakko::RendererDesc desc;                 // 전부 기본값이 있어 비워도 됨
desc.vsync = true;
desc.limits.rvDescriptors = 262144;       // 셰이더 가시 SRV 힙 (이닛 때 통째로 확보, 약 8~16MB)
desc.limits.rtvDescriptors = 1024;
desc.limits.dsvDescriptors = 1024;
desc.limits.cbAreaInitNum = 1024;         // 데이터박스 상수버퍼 영역 초기 칸 수
desc.tuning.resCollectFreq = 4096;        // 미사용 리소스 참조 정리 주기 (프레임), SetTuning 으로 런타임 변경 가능
Hakko::RenderAdapter* renderer = Hakko::CreateRenderer(desc);
```

`limits` 는 생성 시 한 번 (0 이면 기본값), `tuning` 은 `renderer->SetTuning()` 으로 언제든 바꿀 수 있습니다.

## 엔진에 붙이기

라이브러리는 현재 세 덩어리입니다:

`Hakko::DataHub` (네임스페이스, 전역 상태),

`Hakko::RenderAdapter` (팩토리가 만드는 렌더러),

`Hakko::ServiceAdapter` (애플리케이션이 상속하는 틀)

\
창 / 입력 및 루프 호출은 호스트 몫이며 `Samples/Basic/WinAdapter.cpp` 를 예시로 참고하세요.

\
렌더러의 단위 함수 호출은 싱글 스레드 환경 상정입니다. (최소 Sketch() 실행 중 허브 조작 금지, 임계 영역 취급)

```cpp
#include "DataHub.h"
#include "RenderFactory.h"
#include "ServiceAdapter.h"

// --- 시작: 허브 -> 렌더러 생성 -> 서비스 Init -> 렌더러 Init (순서 고정) ---
Hakko::DataHub::Init();                                   // 예약 리소스 / 빌트인 drawProp 등록
Hakko::RenderAdapter* renderer = Hakko::CreateRenderer(Hakko::RendererDesc{});
Hakko::ServiceAdapter* service = new MyService();         // 사용자 클래스
service->Init(width, height);                             // 여기서 리소스 로드 / 프롭 / 데이터박스 등록 (데이터박스 커스터마이즈 타입 등록은 이 시점에 필수)
renderer->Init(hwnd, width, height);                      // 디바이스 / 스왑체인, 서비스가 쌓아 둔 리소스 / 프롭을 올림

// --- 매 프레임 ---
service->Update();          // 허브에 이번 프레임 요청 쌓기 (RTB / 데이터박스 수정)
renderer->Sketch();         // 허브를 읽어 프레임 구성 (함수 실행 중 허브는 고정, 끝나면 다시 자유)
service->OnRender();        // 다음 프레임 준비 등, GPU 제출 전에 할 일
renderer->Render();         // 커맨드 제출 + Present

// --- 창 이벤트 ---
service->OnResize(w, h);    // 카메라 / 뷰포트 갱신 (서비스에서 직접 데이터박스 / 레이어에 반영)
renderer->OnResize(w, h);   // 스왑체인 재생성
renderer->SetVSync(false);  // 티어링 지원 시 즉시 Present

// --- 종료: 역순 ---
renderer->Finalize();
service->Finalize();
Hakko::DataHub::Finalize();
delete service;
Hakko::DestroyRenderer(renderer);
```

`ServiceAdapter` 는 순수 가상 함수만 있는 인터페이스이기 때문에,

어댑터를 상속해 엔진의 기존 루프에 끼워 넣을 수도 있습니다.

\
루프 호출 순서는 `OnResize` -> `Sketch` -> `Render` 순서를 권장합니다. (`OnResize`가 중간에 낄 경우 꼬일 수 있음)

\
에러/경고/디버그 등 리포트는 현재 허브 함수 호출 후 `Hakko::DataHub::HubReportList` (허브 규약 검증) 와

렌더러 단위 함수 실행 후 `RenderReportList` (렌더러, `Sketch` 시작 때 비움) 에 쌓입니다.

리포트는 서비스 레이어가 `Update` 끝에서 읽고 비우는 것을 권장합니다.

\
추가로, `RenderOption.h` 의 매크로로 렌더러의 디버그 출력 / 리포트 전송을 끌 수 있습니다.

## 셰이더

디폴트 셰이더는 렌더러 소유이며 런타임에 DXC 로 컴파일됩니다. (SM 6.0 이상, 변형 매크로 `VERTEX_*` / `OUTPUT_*`)

\
커스텀 셰이더는 `RegisterShader` 로 등록하고 (동일하게 리소스 아이디 발급, 해제는 불가),

`common/registers.hlsli` 를 포함하면 루트 시그니처 계약 (레이어 / 조명 / 물체 상수, 텍스처 슬롯) 을 그대로 사용합니다.

\
기본 제공 셰이더는 `GetShaderId(RESERVED_SHADER_WORLD_VS_NAME)` 처럼 이름으로 얻어 커스텀 drawProp 에 그대로 쓸 수 있습니다.

(VS -> PS 입력 구조체는 디폴트 셰이더 소스를 참고하세요, 추후 지원 예정)
