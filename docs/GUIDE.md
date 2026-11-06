# Hakko 가이드

허브 API 사용법, 엔진 연동 순서, 셰이더 계약. 빌드와 요구 사항은 [README](../README.md), 환경 세팅 메모는 [BUILD.md](BUILD.md) 를 보세요.

## 퀵스타트

```cpp
#include "DataHub_Interface.h"
using namespace Hakko::DataHub;

// 1. 리소스 (파일 -> id), 이름은 경로 그대로
MeshFileInfo info;
LoadMesh(L"res/Fox.gltf", &info);
LoadTexture(L"res/crate1_diffuse.png", TextureFileFormat_None, TextureType_Diffuse);
RTB_UpdateResource up = {}; up.command = RTBCMD_UpdateResource_Upload; SendRTB(up);

// 2. 프롭 (≒ 머터리얼): 기본 텍스처 슬롯 0 디퓨즈 / 1 노멀 / 2 ORM / 3 이미션
//    내 리소스는 이름으로, 예약 리소스는 매크로 id (이름 매크로 RESERVED_*_NAME 으로 조회해도 같음)
TexProp tp = {};
tp.texId[0] = GetTexId(L"res/crate1_diffuse.png");
tp.texId[1] = RESERVED_TEX_NONE_NORMAL_ID;
tp.texId[2] = RESERVED_TEX_NONE_ORM_ID;
tp.texId[3] = GetTexId(RESERVED_TEX_NONE_EMISSION_NAME);
AddTextureProp("crate", tp);
RTB_UpdateProp fb = {}; fb.command = RTBCMD_UpdateProp_FindOrBuild; SendRTB(fb);

// 3. 물체 데이터박스 (월드 행렬, 색, 거칠기 / 금속 배율, 조명 칸)
DtB_3DGeneral obj = {};
obj.worldMat = DirectX::XMMatrixTranspose(DirectX::XMMatrixScaling(0.01f, 0.01f, 0.01f));
obj.blendColor = { 1, 1, 1, 1 };  obj.roughness = 1.0f;  obj.metalic = 1.0f;
DataBoxID objId = AddDataBox<DtB_3DGeneral>(obj);

// 4. 레이어 / 카메라 (RTB_SetCondition 으로 뷰 / 투영 행렬, 전역 조명)
LayerList[0].backGround.bgTexId = GetTexId(L"res/skybox.dds");

// 5. 매 프레임: 지난 요청 비우고 다시 보내기 (id 는 릴리즈로 바뀔 수 있으니 이름으로 매번 조회)
LayerList[0].actionOPQ.drawReqList.clear();
RTB_DrawOPQ req = {};
req.drawProp = RESERVED_DRAWPROP_OPAQUE_SKINNED_ID;
req.texProp  = GetTexPropId("crate");
req.meshId   = GetMeshId(L"res/Fox.gltf");
req.generalId = objId;
for (uint16_t s = 0; s < info.subMeshCount; s++) { req.subMeshId = s; SendRTB(req); }
```

드로우 단위는 서브메쉬 하나입니다.
리포트는 `HubReportList` (허브 입력 오류) 와 `RenderReportList` (렌더러) 에 쌓이며 서비스가 읽고 비웁니다.

### 예약 리소스

허브 초기화 때 고정 id 로 등록되는 것들입니다.
릴리즈되지 않으므로 id 매크로를 그대로 써도 되고, 이름으로 조회해도 됩니다 (`DataHub_Resource.h`).

| 종류 | id | 이름 | 용도 |
|---|---|---|---|
| 텍스처 | `RESERVED_TEX_ERROR_ID` (0) | - | 잘못된 id 가 닿는 에러 텍스처 (마젠타). `TexProp` 의 빈 슬롯도 여기로 |
| 텍스처 | `RESERVED_TEX_NONE_DIFFUSE_ID` (1) | `None_Diffuse` | 흰색 |
| 텍스처 | `RESERVED_TEX_NONE_NORMAL_ID` (2) | `None_Normal` | 평평한 노멀 |
| 텍스처 | `RESERVED_TEX_NONE_ORM_ID` (3) | `None_ORM` | AO 1 / 거칠기 0.5 / 금속 0 |
| 텍스처 | `RESERVED_TEX_NONE_EMISSION_ID` (4) | `None_Emission` | 검정 |
| drawProp | `RESERVED_DRAWPROP_OPAQUE_ID` (3) | `Default_Opaque` | PBR-lite 불투명 |
| drawProp | `RESERVED_DRAWPROP_TRANSLUCENT_ID` (4) | `Default_Translucent` | PBR-lite 알파 블렌드 |
| drawProp | `RESERVED_DRAWPROP_OPAQUE_SKINNED_ID` (5) | `Default_Opaque_Skinned` | + 스키닝 |
| drawProp | `RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_ID` (6) | `Default_Translucent_Skinned` | + 스키닝 |

이름이 `__` 로 시작하는 나머지 예약 항목 (기본 메쉬 / 쿼드, 스카이박스 / 배경판 prop 등) 은 렌더러 내부용입니다.

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
창 / 입력 루프는 호스트 몫이고, `Samples/Basic/WinAdapter.cpp` 가 그 예입니다.
어댑터 단위 함수 호출은 싱글 스레드 환경 상정입니다.

```cpp
#include "DataHub.h"
#include "RenderFactory.h"
#include "ServiceAdapter.h"

// --- 시작: 허브 -> 렌더러 생성 -> 서비스 Init -> 렌더러 Init (순서 고정) ---
Hakko::DataHub::Init();                                   // 예약 리소스 / 빌트인 drawProp 등록
Hakko::RenderAdapter* renderer = Hakko::CreateRenderer(Hakko::RendererDesc{});
Hakko::ServiceAdapter* service = new MyService();          // 사용자 클래스
service->Init(width, height);                             // 여기서 리소스 로드 / 프롭 / 데이터박스 등록
renderer->Init(hwnd, width, height);                      // 디바이스 / 스왑체인, 허브의 빌트인 prop 을 읽음

// --- 매 프레임 ---
service->Update();          // 허브에 이번 프레임 요청 쌓기 (RTB / 데이터박스 수정)
renderer->Sketch();         // 허브를 읽어 프레임 구성 (이 동안 허브는 고정, 끝나면 다시 자유)
service->OnRender();        // 다음 프레임 준비 등, GPU 제출 전에 할 일
renderer->Render();         // 커맨드 제출 + Present

// --- 창 이벤트 ---
service->OnResize(w, h);    // 카메라 / 뷰포트 갱신 (서비스가 조건 데이터박스에 반영)
renderer->OnResize(w, h);   // 스왑체인 재생성
renderer->SetVSync(false);  // 티어링 지원 시 즉시 Present

// --- 종료: 역순 ---
renderer->Finalize();
service->Finalize();
Hakko::DataHub::Finalize();
delete service;
Hakko::DestroyRenderer(renderer);
```

`ServiceAdapter` 는 `Init / Finalize / OnResize / Update / OnRender / OnMouseMove / OnKeyDown / OnKeyUp` 순수 가상 함수만 있는 인터페이스라 엔진의 기존 루프에 그대로 끼워 넣을 수 있습니다.
허브 상태는 전역이라 서비스 여러 개가 같은 허브를 나눠 써도 되지만, 요청 리스트 (`LayerList[n].action*.drawReqList`) 를 비우는 주체는 하나로 정하세요.

리포트는 `Hakko::DataHub::HubReportList` (허브 입력 검증) 와 `RenderReportList` (렌더러, `Sketch` 시작 때 비움) 에 쌓입니다. 서비스 레이어가 `Update` 끝에서 읽고 비우는 것을 권장합니다.
`RenderOption.h` 의 매크로로 렌더러의 디버그 출력 / 리포트 전송을 끌 수 있습니다.

## 셰이더

디폴트 셰이더는 렌더러 소유이며 런타임에 DXC 로 컴파일됩니다 (SM 6.0 이상, 변형 매크로 `VERTEX_*` / `OUTPUT_*`). 커스텀 셰이더는 `RegisterShader` 로 등록하고,
`common/registers.hlsli` 를 포함하면 루트 시그니처 계약 (레이어 / 조명 / 물체 상수, 텍스처 슬롯) 을 그대로 사용합니다.

