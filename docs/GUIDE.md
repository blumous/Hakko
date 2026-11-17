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

// 4. 물체 데이터박스 (월드 행렬, 색, 거칠기 / 금속 배율, 받을 조명칸, 그림자 프리셋 칸)
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

### 인터페이스 헬퍼

위 코드는 texProp 만 직접 만들고 요청을 손으로 채우는 기본 방법입니다.

\
요청 만들기는 헬퍼를 이용한 간편한 방식으로도 사용할 수 있습니다. (`Samples/Basic/GameService.cpp` 의 `BuildProps` 영역 참조)

| 길 | 쓰는 것 | 언제 |
|---|---|---|
| 한 번에 | `MakeDrawRequestAndProps` | 기본 제공 셰이더에 텍스처 몇 장이면 충분할 때 |
| 기본 제공 drawProp | `RESERVED_DRAWPROP_*` + `CreateTexProp` | 텍스처 조합만 직접 정할 때 (위 퀵스타트) |
| 전부 직접 | `CreateDrawProp` + `CreateTexProp` | 컬링 / 블렌드 / 셰이더까지 정할 때 |

```cpp
// 한 번에 drawProp / texProp 을 만들고 요청 구조체까지 채움 (보내는 건 직접, 업로드 RTB 도 그대로 필요)
DrawOption option;                                      // 기본값 = 기본 제공 셰이더 + 빈 텍스처
option.diffuseTexId = GetTexId(L"res/crate1_diffuse.png");

RTB_DrawOPQ req = {};
RTB_DrawSDW shadowReq = {};                             // 마지막 인자를 넘기면 그림자 요청도 같이
MakeDrawRequestAndProps(option, "CrateDraw", "CrateMat", req, GetMeshId(L"res/Box2.obj"), objId, 0, &shadowReq);
```

만든 프롭은 이름 (`"CrateDraw"`, `"CrateMat"`) 으로 등록되므로, 이후에는 기본 방법과 똑같이 이름으로 id 를 다시 얻어 씁니다.

### 계약 및 제한 사항

- **리소스는 만든 프레임에 업로드**, `Create*` / `Update*` / `Load*` 로 리소스를 생성한 프레임에 `RTB_UpdateResources` 를 보내야 합니다. (`RTBCMD_Upload` / `RTBCMD_Update`, 비트로 묶어 한 번에 보내도 OK) 대기 큐에 쌓아둔 채 그 사이에 리소스 해제가 실행될 경우 대기 중인 id 가 어긋납니다.
- **리소스 id 는 저장하지 않기**, 해제 (`Release*` 후 `RTBCMD_Release`) 는 몇 프레임 뒤에 실행되고, 해제 시에 기존 리소스의 id 가 바뀔 수 있습니다. id는 공개되어 있지만, id가 아닌 리소스의 이름, 뷰가 핸들 역할을 하는 것으로 생각해야 합니다. 따라서 리소스 사용 시 매번 이름으로 다시 조회하는 것을 권장합니다. (예약 리소스는 매크로 정의 그대로 사용해도 무방)
- **데이터박스 값 바꾸기** 데이터박스 한 칸을 통째로 바꿀 땐 `UpdateDataBox(id, data)`, 세부 필드나 여러 칸에 접근 필요 시 `DataBoxCustomizer::GetDataBoxList<T>()` 로 받아서 직접 수정할 수 있으나, 리스트 길이가 바뀌는 동작은 직접 조작하지 않고 `Add / RemoveDataBox()` 를 이용합니다.
- **데이터박스 0번은 빈 데이터**, 타입마다 0번은 전부 0 인 칸으로 예약되어 있고 서비스가 넣는 데이터는 1번부터입니다. 레이어의 조명칸도 0번은 비워 둡니다.
- **요청 목록은 서비스가 비우기**, 보낸 드로우 요청은 다음 프레임에도 남아 있습니다. 매 프레임 `Clear*DrawRequests()` 로 비우고 다시 보냅니다.

### 예약 리소스

허브 초기화 때 고정 id 로 등록되는 리소스들입니다.

\
릴리즈되지 않으므로 id 매크로를 그대로 써도 되고, 이름으로 조회도 가능합니다. (`DataHub_Resource.h`)

\
번호는 버전마다 재배치될 수 있습니다. (v0.2.0 에서 셰이더 / drawProp 번호가 바뀜) 숫자 대신 매크로나 이름을 사용하세요.

| 종류 | id | 이름 | 용도 |
|---|---|---|---|
| 텍스처 | `RESERVED_TEX_ERROR_ID` (0) | - | 잘못된 id 가 닿는 에러 텍스처 (마젠타). `TexProp` 의 빈 슬롯도 여기로 |
| 텍스처 | `RESERVED_TEX_NONE_DIFFUSE_ID` (2) | `None_Diffuse` | 흰색 |
| 텍스처 | `RESERVED_TEX_NONE_NORMAL_ID` (3) | `None_Normal` | 평평한 노멀 |
| 텍스처 | `RESERVED_TEX_NONE_ORM_ID` (4) | `None_ORM` | AO 1 / 거칠기 0.5 / 금속 0 |
| 텍스처 | `RESERVED_TEX_NONE_EMISSION_ID` (5) | `None_Emission` | 검정 |
| 텍스처 | `RESERVED_TEX_NONE_SHADOW_ID` (6) | `None_Shadow` | 깊이 1 (그림자 없음), 레이어의 빈 섀도우맵 칸 |
| 셰이더 | `RESERVED_SHADER_WORLD_VS_ID` (5) | `Default_World_VS` | 기본 월드 VS (정점 타입별 변형) |
| 셰이더 | `RESERVED_SHADER_WORLD_OPQ_PS_ID` (6) | `Default_World_OPQ_PS` | PBR-lite 불투명 PS |
| 셰이더 | `RESERVED_SHADER_WORLD_TP_PS_ID` (7) | `Default_World_TP_PS` | PBR-lite 반투명 PS |
| 셰이더 | `RESERVED_SHADER_PARTICLE_VS_ID` / `_PS_ID` (8, 9) | `Default_Particle_VS` / `_PS` | 파티클 그리기 |
| 셰이더 | `RESERVED_SHADER_SHADOW_VS_ID` (10) | `Default_Shadow_VS` | 섀도우 (조명 시점 깊이) |
| 셰이더 | `RESERVED_SHADER_PP_VS_ID` (11) | `Default_PP_VS` | Edit 액션 공용 VS (v0.1 의 `Default_BG_VS`) |
| 셰이더 | `RESERVED_SHADER_SSAO_PS_ID` / `AOBLUR_PS_ID` (12, 13) | `Default_SSAO_PS` / `Default_AOBlur_PS` | SSAO, AO 블러 |
| 셰이더 | `RESERVED_SHADER_DEFERRED_LIGHTING_PS_ID` (14) | `Default_DeferredLighting_PS` | 디퍼드 라이팅 |
| 셰이더 | `RESERVED_SHADER_BLOOM_*_PS_ID` (15 ~ 18) | `Default_BloomPrefilter_PS` / `Down` / `Up` / `Combine` | 블룸 단계 |
| 셰이더 | `RESERVED_SHADER_TONEMAP_PS_ID` / `FXAA_PS_ID` (19, 20) | `Default_Tonemap_PS` / `Default_FXAA_PS` | 톤맵, FXAA |
| 셰이더 | `RESERVED_SHADER_SKINNING_CS_ID` / `PARTICLE_CS_ID` (21, 22) | `Default_Skinning_CS` / `Default_Particle_CS` | 컴퓨트 스키닝, 파티클 시뮬 |
| drawProp | `RESERVED_DRAWPROP_OPAQUE_ID` (3) | `Default_Opaque` | PBR-lite 불투명 |
| drawProp | `RESERVED_DRAWPROP_OPAQUE_SKINNED_ID` (4) | `Default_Opaque_Skinned` | + 스키닝 |
| drawProp | `RESERVED_DRAWPROP_OPAQUE_CLIP_ID` / `OPAQUE_SKINNED_CLIP_ID` (5, 6) | `Default_Opaque_Clip` / `Default_Opaque_Skinned_Clip` | + 알파 클립 (컷오프는 `DtB_3DGeneral.alphaCutoff`) |
| drawProp | `RESERVED_DRAWPROP_TRANSLUCENT_ID` (7) | `Default_Translucent` | PBR-lite 알파 블렌드 |
| drawProp | `RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_ID` (8) | `Default_Translucent_Skinned` | + 스키닝 |
| drawProp | `RESERVED_DRAWPROP_PARTICLE_ID` (9) | `Default_Particle` | 파티클 (가산 블렌드) |
| drawProp | `RESERVED_DRAWPROP_SHADOW_ID` ~ `SHADOW_SKINNED_CLIP_ID` (10 ~ 13) | `Default_Shadow` / `_Skinned` / `_Clip` / `_Skinned_Clip` | 섀도우 캐스터 (불투명 쪽과 같은 조합) |
| drawProp | `RESERVED_DRAWPROP_SSAO_ID` ~ `FXAA_ID` (14 ~ 22) | `Default_SSAO`, `Default_AOBlur`, `Default_DeferredLighting`, `Default_Bloom*`, `Default_Tonemap`, `Default_FXAA` | 고정 액션 / PP 칸용 (헬퍼가 꽂아 줌) |

\
이름이 `__` 로 시작하는 나머지 예약 항목 (기본 메쉬 / 쿼드, 투명 텍스처 (1), 스카이박스 / 배경판 prop, 합성 / 배경용 셰이더 등) 은 렌더러 내부용입니다.

### 빌트인 파이프라인과 헬퍼

Hakko 에서는 렌더 패스 하나를 **액션 (Action)** 이라고 부릅니다. (레이어 `Layer` - 액션 `Action` - 그리기 요청 `DrawRequest` 순 포함 구조)

\
액션은 하는 일에 따라 Draw (요청 목록을 그림) / Edit (화면 전체를 셰이더 하나로 가공) / Compute (컴퓨트 디스패치) 로 나뉘며,

각자 입력 (앞 액션의 결과를 어떻게 받을지) 과 출력 (어디에 쓸지), 액션 데이터 (데이터박스 하나) 를 가집니다.

\
레이어 하나는 고정된 순서의 액션 묶음이며, 세팅하지 않은 칸은 건너뜁니다. (`DataHub_RTB.h` 의 레이어 구조체)

`컴퓨트 선계산 (4칸)` -> `뎁스 프리패스 (포워드) / GBuffer (디퍼드)` -> `SSAO, AO 블러` -> `불투명 (포워드) / 라이팅 (디퍼드)` -> `배경` -> `반투명` -> `포스트 프로세싱 (16칸)`

그림자 액션 (전역 캐스케이드 4칸 + 로컬 타일) 은 불투명 / 라이팅보다 먼저 그려집니다.

\
칸을 직접 채워도 되지만, 기본 제공 기능은 `DataHub_Interface.h` 의 헬퍼로 세팅하는 것을 권장합니다.

| 하려는 것 | 쓰는 것 |
|---|---|
| 포워드 / 디퍼드 전환, 뎁스 프리패스 켜고 끄기 | `SetLayerPipelineType`, `SetLayerEarlyZ` |
| 톤맵 / FXAA / SSAO / AO 블러 | `RTB_SetTonemap`, `RTB_SetFXAA`, `RTB_SetSSAO`, `RTB_SetAOBlur` |
| 블룸 | `CreateBloomChain`, `RTB_SetBloom` |
| 그림자 | `CreateShadowAtlas`, `RTB_SetShadowCascade`, `RTB_SetShadowLocal`, `MakeShadowRequest` |
| 파티클 | `CreateParticleBuffer`, `RTB_SetParticle`, `MakeParticleRequest` |
| 컴퓨트 스키닝 | `CreateSkinnedMesh`, `RTB_SetSkinning`, `UpdateSkinnedMeshVertices` |
| 원하는 액션에 요청 직접 넣기, 남는 데이터 칸 쓰기 | `MakeDrawRequest`, `SendRTB(요청, 액션)`, `SetActionDataType`, `SetActionOutputLayout` |
| 리소스 이름 바꾸기 | `RenameMesh` / `RenameTexture` / `RenameComputeBuffer` / `RenameDrawProp` / `RenameTexProp` |

```cpp
// 리소스를 만드는 헬퍼 (Create~) 는 업로드 RTB 전에
BloomChain bloomChain = CreateBloomChain("Bloom", width, height);
ShadowAtlas sunAtlas = CreateShadowAtlas(L"SunShadowAtlas", 2048);

// 후처리: PP 칸 번호가 곧 도는 순서 (블룸은 단계 수 x 2 칸을 차지, 톤맵보다 앞 칸에)
RTB_SetBloom bloom = {};
bloom.ppSlot = 0;
bloom.chain = bloomChain;
SendRTB(bloom);

RTB_SetTonemap tonemap = {};
tonemap.ppSlot = 6;
SendRTB(tonemap);

// 그림자: 전역 조명 캐스케이드 한 장 (그리는 쪽과 읽는 쪽을 한번에 세팅)
RTB_SetShadowCascade sun = {};
sun.cascade = 0;
sun.atlas = sunAtlas;
sun.lightViewProj = DirectX::XMMatrixMultiplyTranspose(lightViewM, lightProjM);
sun.splitEnd = 1000.0f;                 // 이 캐스케이드가 덮는 뷰 깊이의 끝
SendRTB(sun);

// 매 프레임: 캐스터 요청은 그 캐스케이드 / 타일의 섀도우 액션에 직접
ClearShadowDrawRequests();
SendRTB(MakeShadowRequest(req), LayerList[0].actionGlobalSDWs[0]);
```

- 세팅 구조체는 `= {}` 로 만들면 초깃값이 곧 기본 모습이며, 끌 때는 `enable = false` 로 다시 보냅니다.
- 파이프라인 종류 / 프리패스 토글은 그 프레임의 드로우 요청을 보내기 전에 호출합니다. 불투명 요청 (`RTB_DrawOPQ`) 이 종류에 따라 불투명 + 프리패스, 또는 GBuffer 액션으로 자동으로 나뉘어 들어가기 때문입니다.
- 로컬 조명은 레이어 조명칸 (64칸, `RTB_SetLight`) 에 꽂습니다. 포워드는 물체의 `affectLights` 8칸에 적힌 조명칸만, 디퍼드는 조명칸 전부를 계산합니다.
- 그림자 아틀라스는 정사각 격자이며 타일 번호 = 섀도우 액션 칸 번호입니다. 로컬 조명 그림자 (`RTB_SetShadowLocal`) 는 레이어의 그림자 프리셋 칸에 모이고, 물체는 `DtB_3DGeneral.shadowPreset` 에 적은 칸 (0 = 레이어 기본) 의 프리셋으로 그림자를 받습니다.
- 헬퍼가 액션에 만들어 꽂은 데이터박스 (액션 데이터, 그림자 프리셋) 를 묶어서 풀어주는 기능은 아직 없습니다.

사용 예는 `Samples/Basic/GameService.cpp`, 칸별 설명은 `DataHub_Interface.h` 의 주석을 참고하세요.

### 렌더러 옵션

```cpp
#include "RenderFactory.h"

Hakko::RendererDesc desc;                 // 전부 기본값이 있어 비워도 됨
desc.vsync = true;
desc.limits.rvDescriptors = 262144;       // 셰이더 가시 SRV 힙 (이닛 때 통째로 확보, 약 8~16MB)
desc.limits.rtvDescriptors = 1024;
desc.limits.dsvDescriptors = 1024;
desc.limits.cbAreaInitNum = 1024;         // 데이터박스 상수버퍼 영역 초기 칸 수
desc.limits.cbAreaShrinkFreq = 180;       // 영역에 여유가 이만큼 (프레임) 연속되면 줄임, 0 이면 줄이지 않음
desc.limits.cbAreaShrinkThreshold = 4;    // 쓰는 칸의 몇 배 이상 남아야 여유로 칠지
desc.tuning.resCollectFreq = 4096;        // 미사용 리소스 참조 정리 주기 (프레임), SetTuning 으로 런타임 변경 가능
desc.tuning.texCollectThreshold = 4096;   // 이만큼 (프레임) 안 쓰인 텍스처 참조를 정리 대상으로
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
service->OnKeyDown(key);    // 입력은 호스트가 그대로 전달 (OnKeyUp / OnMouseMove)

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
추가로, `RenderOption.h` 의 매크로로 렌더러의 디버그 출력 / 리포트 전송을 끄거나 백버퍼 수 (2 / 3) 를 바꿀 수 있습니다.

### ImGui 모듈 (선택)

`RENDER_IMGUI_MODE` 가 켜진 빌드 (CMake `-DHAKKO_USE_IMGUI=ON`, [BUILD.md](BUILD.md)) 에서는 렌더러가 ImGui 드로우 데이터를 직접 읽어 합성이 끝난 백버퍼 위에 그립니다.

\
컨텍스트와 플랫폼 백엔드 (창 / 입력) 는 호스트 몫이며, 렌더러 백엔드는 따로 붙이지 않습니다.

```cpp
#if RENDER_IMGUI_MODE == RENDER_IMGUI_ENABLE    // RenderOption.h 를 먼저 포함할 것
// 시작: renderer->Init 전에
ImGui::CreateContext();
ImGui_ImplWin32_Init(hwnd);

// 매 프레임
ImGui_ImplWin32_NewFrame();
ImGui::NewFrame();
service->Update();          // ImGui:: 호출은 이 사이에
ImGui::Render();            // Sketch 전에
renderer->Sketch();

// 종료: renderer->Finalize 뒤에
ImGui_ImplWin32_Shutdown();
ImGui::DestroyContext();
#endif
```

창 메시지는 `ImGui_ImplWin32_WndProcHandler` 에 먼저 넘기고, ImGui 가 입력을 잡은 동안 (`WantCaptureMouse` / `WantCaptureKeyboard`) 은 서비스로 넘기지 않습니다. (`Samples/Basic/main.cpp`, `WinAdapter.cpp`)

\
허브 텍스처는 `ImGui::Image((ImTextureID)GetTexId(L"..."), size)` 처럼 id 를 그대로 넘겨 그릴 수 있습니다. (2D 텍스처만, 잘못된 id 는 에러 텍스처)

## 셰이더

디폴트 셰이더는 렌더러 소유이며 런타임에 DXC 로 컴파일됩니다. (SM 6.0 이상, 변형 매크로 `VERTEX_*` / `OUTPUT_*`)

\
커스텀 셰이더는 `RegisterShader` 로 등록하고 (VS / PS / CS, 동일하게 리소스 아이디 발급, 해제는 불가),

`common/registers.hlsli` 를 포함하면 루트 시그니처 계약 (레이어 / 조명 / 물체 상수, 텍스처 슬롯, 섀도우맵 / 그림자 프리셋) 을 그대로 사용합니다.

\
컴퓨트 셰이더는 대신 `common/registers_compute.hlsli` 를 포함합니다. (칸 배치가 달라 둘을 같이 포함할 수 없음)

액션의 `reads[k]` / `writes[k]` 가 `CS_READ_BUFFER(k)` / `CS_WRITE_BUFFER(k)` (텍스처는 `CS_READ_TEX` / `CS_WRITE_TEX`) 자리로 들어옵니다.

\
액션 데이터 (`actionDataBoxId`) 는 두 헤더 모두 `ACTION_DATA_CB` 자리로 들어옵니다.

Edit 액션의 기본 타입은 `DtB_EditCustom` (float 64칸) 이며, 칸의 의미는 셰이더가 선언한 순서 그대로입니다.

\
`common/` 의 나머지 헤더 (`lighting.hlsli`, `depth.hlsli`, `bloom.hlsli`, `particle.hlsli`) 는 기본 제공 셰이더가 나눠 쓰는 함수 모음입니다.

\
기본 제공 셰이더는 `GetShaderId(RESERVED_SHADER_WORLD_VS_NAME)` 처럼 이름으로 얻어 커스텀 drawProp 에 그대로 쓸 수 있습니다.

(VS -> PS 입력 구조체는 디폴트 셰이더 소스를 참고하세요, 추후 지원 예정)
