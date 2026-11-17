#include "GameService.h"

#include <cmath>

#include "DataHub_Interface.h"
#include "DataHub_FileLoader.h"


namespace Hakko
{

using namespace DataHub;


// 조작
//   W A S D : 카메라 이동        Q / E : 카메라 내리기 / 올리기        마우스 오른쪽 버튼 누른 채 움직이기 : 둘러보기
//   방향키 : 유리 상자 움직이기        Z / X : 유리 상자 투명도
//   1 톤맵   2 FXAA   3 SSAO   4 블룸   5 그림자   6 파티클   C 포워드 <-> 디퍼드


float boxX, boxZ;
bool boxR, boxL, boxU, boxD;


DirectX::XMFLOAT3 camPos;
float camYaw, camPitch;
bool camF, camB, camL, camR, camU, camD;

bool lookOn;
int mouseX, mouseY;

const float CAM_MOVE_SPEED = 0.1f;
const float CAM_LOOK_SPEED = 0.005f;

const auto upVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
auto projM = DirectX::XMMatrixIdentity();


RTB_DrawOPQ groundDrawReq;
RTB_DrawOPQ crateDrawReq;
RTB_DrawOPQ foxDrawReq;
RTB_DrawTP glassDrawReq;

RTB_DrawSDW crateShadowReq;
RTB_DrawSDW glassShadowReq;
RTB_DrawSDW foxShadowReq;

DataBoxID groundDataId;
DataBoxID crateDataId;
DataBoxID glassDataId;
DataBoxID foxDataId;
DataBoxID foxBoneId;
DtB_3DGeneral glassData;


// 스포트 조명 (조명칸 0 은 비워두는 약속이라 1번에)
const uint16_t SPOT_LIGHT_SLOT = 1;
DataBoxID spotLightId;


// 후처리 / 그림자 / 파티클 PP 노드
// PP 칸 번호가 곧 도는 순서: 블룸 (3 단계 x 2 칸) -> 톤맵 -> FXAA
const uint8_t PP_SLOT_BLOOM = 0;
const uint8_t PP_SLOT_TONEMAP = 6;
const uint8_t PP_SLOT_FXAA = 7;

RTB_SetSSAO ssaoSet;
RTB_SetAOBlur aoBlurSet;
RTB_SetTonemap tonemapSet;
RTB_SetFXAA fxaaSet;

BloomChain bloomChain;
RTB_SetBloom bloomSet;

ShadowAtlas sunAtlas;
ShadowAtlas spotAtlas;
RTB_SetShadowCascade sunShadow;
RTB_SetShadowLocal spotShadow;

ParticleBuffer particleBuffer;
RTB_SetParticle particleSet;

bool deferredOn = false;



static void UpdateCamera()
{
    const float cosPitch = cosf(camPitch);
    const auto forward = DirectX::XMVectorSet(cosPitch * sinf(camYaw), sinf(camPitch), cosPitch * cosf(camYaw), 0.0f);
    const auto right = DirectX::XMVectorSet(cosf(camYaw), 0.0f, -sinf(camYaw), 0.0f);

    auto eyePos = DirectX::XMLoadFloat3(&camPos);

    if (camF)
        eyePos = DirectX::XMVectorAdd(eyePos, DirectX::XMVectorScale(forward, CAM_MOVE_SPEED));
    if (camB)
        eyePos = DirectX::XMVectorSubtract(eyePos, DirectX::XMVectorScale(forward, CAM_MOVE_SPEED));
    if (camR)
        eyePos = DirectX::XMVectorAdd(eyePos, DirectX::XMVectorScale(right, CAM_MOVE_SPEED));
    if (camL)
        eyePos = DirectX::XMVectorSubtract(eyePos, DirectX::XMVectorScale(right, CAM_MOVE_SPEED));
    if (camU)
        eyePos = DirectX::XMVectorAdd(eyePos, DirectX::XMVectorScale(upVec, CAM_MOVE_SPEED));
    if (camD)
        eyePos = DirectX::XMVectorSubtract(eyePos, DirectX::XMVectorScale(upVec, CAM_MOVE_SPEED));

    DirectX::XMStoreFloat3(&camPos, eyePos);


    const auto viewM = DirectX::XMMatrixLookToLH(eyePos, forward, upVec);

    RTB_SetCamera rtbCam = {};
    rtbCam.view = DirectX::XMMatrixTranspose(viewM);
    rtbCam.proj = DirectX::XMMatrixTranspose(projM);
    rtbCam.viewProj = DirectX::XMMatrixMultiplyTranspose(viewM, projM);
    rtbCam.eyePos = camPos;

    SendRTB(rtbCam);
}

static DirectX::XMMATRIX MakeProjection(int width, int height)
{
    const float fov = DirectX::XMConvertToRadians(60.0f);
    const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    return DirectX::XMMatrixPerspectiveFovLH(fov, aspectRatio, 0.1f, 1000.0f);
}



GameService::GameService()
{
}


bool GameService::Init(int width, int height)
{
    boxX = boxZ = 0.0f;
    boxR = boxL = boxU = boxD = false;

    
    camPos = DirectX::XMFLOAT3{3.0f, 5.0f, -10.0f};
    camYaw = DirectX::XMConvertToRadians(-17.0f);
    camPitch = DirectX::XMConvertToRadians(-26.0f);
    camF = camB = camL = camR = camU = camD = false;

    lookOn = false;
    mouseX = mouseY = 0;



    // 리소스 이름이 곧 나중에 사용할 키, 잘 보관해 두세요
    #pragma region UploadResources

    const MeshID boxMesh = LoadMesh(L"res/Box2.obj");
    if (boxMesh == 0)
        return false;

    const MeshID foxMesh = LoadMesh(L"res/Fox.gltf");
    if (foxMesh == 0)
        return false;

    if (!LoadTexture(L"res/skybox.dds", TextureFileFormat_DDS, TextureType_Cube, TextureColorSpace_SRGB))
        return false;

    if (!LoadTexture(L"res/crate1_diffuse.png", TextureFileFormat_None, TextureType_2D, TextureColorSpace_SRGB))
        return false;

    if (!LoadTexture(L"res/crate1_normal.png", TextureFileFormat_None, TextureType_2D, TextureColorSpace_Linear))
        return false;

    if (!LoadTexture(L"res/Texture.png", TextureFileFormat_None, TextureType_2D, TextureColorSpace_SRGB))
        return false;


    // 헬퍼로 만드는 리소스 (간편 사용)
    bloomChain = CreateBloomChain("Bloom", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    sunAtlas = CreateShadowAtlas(L"SunShadowAtlas", 2048);
    spotAtlas = CreateShadowAtlas(L"SpotShadowAtlas", 2048);
    particleBuffer = CreateParticleBuffer(L"ParticleBuffer", 1024);

    if (bloomChain.levels == 0 || sunAtlas.size == 0 || spotAtlas.size == 0 || particleBuffer.count == 0)
        return false;

    #pragma endregion



    // 렌더러에 넘겨줄 수 있는 데이터를 보관해요 - 데이터는 서비스 자율 접근, 고정 아이디를 씁니다
    #pragma region StoreData

    DtB_3DGeneral groundData = DtB_3DGeneral();
    groundData.worldMat = DirectX::XMMatrixTranspose(
        DirectX::XMMatrixMultiply(DirectX::XMMatrixScaling(10.0f, 0.1f, 10.0f), DirectX::XMMatrixTranslation(0.0f, -1.1f, 0.0f)));
    groundData.blendColor = DirectX::XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f};
    groundData.roughness = 1.0f;
    groundData.metalic = 0.0f;
    groundData.affectLights[0] = SPOT_LIGHT_SLOT;       // 포워드에서 받을 조명칸 (최대 8개)

    groundDataId = AddDataBox(groundData);



    DtB_3DGeneral crateData = groundData;
    crateData.worldMat = DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(-3.5f, 0.0f, 0.0f));

    crateDataId = AddDataBox(crateData);



    glassData = groundData;
    glassData.worldMat = DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(boxX, 0.0f, boxZ));
    glassData.blendColor = DirectX::XMFLOAT4{0.6f, 0.8f, 1.0f, 0.5f};

    glassDataId = AddDataBox(glassData);



    DtB_3DGeneral foxData = groundData;
    foxData.worldMat = DirectX::XMMatrixTranspose(
        DirectX::XMMatrixMultiply(DirectX::XMMatrixScaling(0.02f, 0.02f, 0.02f), DirectX::XMMatrixTranslation(3.5f, -1.0f, 0.0f)));

    foxDataId = AddDataBox(foxData);

    DtB_3DOptional foxBone = DtB_3DOptional();
    for (size_t i = 0; i < SKIN_BONE_MAX; i++)
        foxBone.boneTransforms[i] = DirectX::XMMatrixIdentity();

    foxBoneId = AddDataBox(foxBone);

    #pragma endregion



    // 익히 머터리얼이라고 부르는 그겁니다
    // 그리기 속성 (DrawProp) 및 텍스처 조합 (TexProp), 리소스처럼 관리하고 (이름 핸들로 관리)
    // 그리기 요청에 아이디로 끼워넣으세요
    #pragma region BuildProps

    // 간편 사용법 한번에 뽑기, 요청까지 한번에
    // 옵션을 기본값으로 두면 기본 셰이더 + 기본 텍스처, 마지막 인자를 넘기면 그림자 요청도 같이
    DrawOption groundOption;

    if (!MakeDrawRequestAndProps(groundOption, "GroundDraw", "GroundMat", groundDrawReq, boxMesh, groundDataId))
        return false;


    DrawOption crateOption;
    crateOption.diffuseTexId = GetTexId(L"res/crate1_diffuse.png");
    crateOption.normalTexId = GetTexId(L"res/crate1_normal.png");

    if (!MakeDrawRequestAndProps(crateOption, "CrateDraw", "CrateMat", crateDrawReq, boxMesh, crateDataId, 0, &crateShadowReq))
        return false;


    // TexProp 직접 만들기
    // 기본 제공 셰이더에서 TexProp은 0 ~ 3 번 칸이 디퓨즈 / 노멀 / ORM / 이미션으로 읽습니다 (PBR)
    TexProp foxTex = {};
    foxTex.texId[0] = GetTexId(L"res/Texture.png");
    foxTex.texId[1] = GetTexId(RESERVED_TEX_NONE_NORMAL_NAME);      // 안 쓰는 칸은 기본 제공 빈 텍스처로
    foxTex.texId[2] = GetTexId(RESERVED_TEX_NONE_ORM_NAME);
    foxTex.texId[3] = GetTexId(RESERVED_TEX_NONE_EMISSION_NAME);

    if (CreateTexProp("FoxMat", foxTex) == 0)
        return false;

    foxDrawReq.drawProp = GetDrawPropId(RESERVED_DRAWPROP_OPAQUE_SKINNED_NAME);
    foxDrawReq.texProp = GetTexPropId("FoxMat");
    foxDrawReq.meshId = foxMesh;
    foxDrawReq.generalId = foxDataId;
    foxDrawReq.optionalId = foxBoneId;      // 스킨드 메쉬의 본 데이터

    // 그림자 그리기용 요청 같이 뽑기
    // 기본 제공 DrawProp인 요청으로 그림자용 속성을 자동으로 뽑을수도 있음 (간편 사용)
    foxShadowReq = MakeShadowRequest(foxDrawReq);


    // DrawProp인 직접 만들기
    // 셰이더는 기본 제공 그대로, 직접 등록한 셰이더가 있으면 그 이름으로
    DrawProp glassDraw = {};
    glassDraw.depthUsage = DrawProp_DepthUsage_Translucent;
    glassDraw.cullType = DrawProp_CullType_None;
    glassDraw.blendType = DrawProp_BlendType_Alpha;
    glassDraw.fillType = DrawProp_FillType_Solid;
    glassDraw.depthBias = DrawProp_DepthBias_None;
    glassDraw.topology = DrawProp_Topology_TriangleList;
    glassDraw.vertexType = VertexType_Static;
    glassDraw.VSId = GetShaderId(RESERVED_SHADER_WORLD_VS_NAME);
    glassDraw.PSId = GetShaderId(RESERVED_SHADER_WORLD_TP_PS_NAME);

    TexProp glassTex = {};
    glassTex.texId[0] = GetTexId(RESERVED_TEX_NONE_DIFFUSE_NAME);
    glassTex.texId[1] = GetTexId(RESERVED_TEX_NONE_NORMAL_NAME);
    glassTex.texId[2] = GetTexId(RESERVED_TEX_NONE_ORM_NAME);
    glassTex.texId[3] = GetTexId(RESERVED_TEX_NONE_EMISSION_NAME);

    if (CreateDrawProp("GlassDraw", glassDraw) == 0 || CreateTexProp("GlassMat", glassTex) == 0)
        return false;

    glassDrawReq.drawProp = GetDrawPropId("GlassDraw");
    glassDrawReq.texProp = GetTexPropId("GlassMat");
    glassDrawReq.meshId = boxMesh;
    glassDrawReq.generalId = glassDataId;

    // 직접 만든 DrawProp은 그림자용 DrawProp도 직접 지정해야 함
    glassShadowReq = MakeShadowRequest(glassDrawReq, GetDrawPropId(RESERVED_DRAWPROP_SHADOW_NAME));


    // 지금까지 만든 리소스와 속성을 올려달라고 요청 (만든 프레임에 보내기)
    RTB_UpdateResources rtbUpload = {};
    rtbUpload.command = RTBCMD_Upload;

    SendRTB(rtbUpload);

    #pragma endregion



    // 화면 층을 나누고 (Layer) 층별로 배경이나 기타 설정을 해줘요
    // 모든 요청은 층 별로 관리됩니다
    #pragma region SetLayerOption

    SetLayerBackground(GetTexId(L"res/skybox.dds"));

    #pragma endregion



    // 전역 데이터, 카메라나 조명 값을 설정해요
    #pragma region SetCondition

    projM = MakeProjection(width, height);


    const auto sunDir = DirectX::XMVector3Normalize(DirectX::XMVectorSet(-1.0f, -1.0f, 1.0f, 0.0f));

    RTB_SetGlobalLight rtbLight = {};
    rtbLight.ambient = DirectX::XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
    rtbLight.color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    DirectX::XMStoreFloat3(&rtbLight.direction, sunDir);

    SendRTB(rtbLight);


    // 스포트 라이트, 조명 데이터는 레이어의 조명 칸에 아이디로 넣기
    const auto spotPos = DirectX::XMVectorSet(-5.0f, 6.0f, -3.0f, 1.0f);
    const auto spotTarget = DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 1.0f);
    const float spotAngle = DirectX::XMConvertToRadians(50.0f);

    DtB_Light spotLight = DtB_Light();
    spotLight.type = 3;
    DirectX::XMStoreFloat3(&spotLight.position, spotPos);
    spotLight.range = 30.0f;
    spotLight.color = DirectX::XMFLOAT3{1.0f, 0.8f, 0.5f};
    DirectX::XMStoreFloat3(&spotLight.direction, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(spotTarget, spotPos)));
    spotLight.spotAngle = spotAngle;

    spotLightId = AddDataBox(spotLight);

    RTB_SetLight rtbSpotLight = {};
    rtbSpotLight.light = spotLightId;
    rtbSpotLight.slot = SPOT_LIGHT_SLOT;

    SendRTB(rtbSpotLight);

    #pragma endregion



    // 그림자 그리기용 세팅, 조명 시점 행렬을 그리는 쪽과 읽는 쪽에 한번에 넘겨줘요 (간편 사용)
    #pragma region SetShadow

    // 전역 조명, CSM 캐스케이드에 그리기
    const auto sunEye = DirectX::XMVectorScale(sunDir, -15.0f);
    const auto sunViewM = DirectX::XMMatrixLookAtLH(sunEye, DirectX::XMVectorZero(), upVec);
    const auto sunProjM = DirectX::XMMatrixOrthographicLH(12.0f, 12.0f, 0.1f, 40.0f);

    sunShadow.cascade = 0;
    sunShadow.atlas = sunAtlas;
    sunShadow.lightViewProj = DirectX::XMMatrixMultiplyTranspose(sunViewM, sunProjM);
    sunShadow.splitEnd = 1000.0f;

    SendRTB(sunShadow);


    // 로컬 조명, 섀도우 아틀라스 타일에 그리기
    const auto spotViewM = DirectX::XMMatrixLookAtLH(spotPos, spotTarget, upVec);
    const auto spotProjM = DirectX::XMMatrixPerspectiveFovLH(spotAngle, 1.0f, 0.5f, 30.0f);

    spotShadow.tile = 0;
    spotShadow.atlas = spotAtlas;
    spotShadow.lightViewProj = DirectX::XMMatrixMultiplyTranspose(spotViewM, spotProjM);
    spotShadow.lightSlot = SPOT_LIGHT_SLOT;

    SendRTB(spotShadow);

    #pragma endregion



    // 기본 제공 후처리 설정, 헬퍼 기본값 그대로 넘기면 기본 세팅 (간편 사용)
    // SSAO 이외 PP는 순서(칸 번호)도 설정
    #pragma region SetPostProcess

    SendRTB(ssaoSet);
    SendRTB(aoBlurSet);


    bloomSet.ppSlot = PP_SLOT_BLOOM;
    bloomSet.chain = bloomChain;
    SendRTB(bloomSet);

    tonemapSet.ppSlot = PP_SLOT_TONEMAP;
    SendRTB(tonemapSet);

    fxaaSet.ppSlot = PP_SLOT_FXAA;
    SendRTB(fxaaSet);

    #pragma endregion



    // 기본 제공 파티클, 값 세팅
    #pragma region SetParticle

    particleSet.computeSlot = 0;
    particleSet.buffer = particleBuffer;

    #pragma endregion


    return true;
}

void GameService::Finalize()
{
    RemoveDataBox(DataBoxType_DataBox_RenderLight, spotLightId);
    RemoveDataBox(DataBoxType_DataBox_Render3DOptional, foxBoneId);
    RemoveDataBox(DataBoxType_DataBox_Render3DGeneral, foxDataId);
    RemoveDataBox(DataBoxType_DataBox_Render3DGeneral, glassDataId);
    RemoveDataBox(DataBoxType_DataBox_Render3DGeneral, crateDataId);
    RemoveDataBox(DataBoxType_DataBox_Render3DGeneral, groundDataId);
}


void GameService::Update()
{
    if (boxR)
        boxX += 0.05f;
    if (boxL)
        boxX -= 0.05f;
    if (boxU)
        boxZ += 0.05f;
    if (boxD)
        boxZ -= 0.05f;



    // 이전 프레임 요청은 서비스에서 직접 비워주세요
    #pragma region ClearPrevRequest

    ClearOpaqueDrawRequests();
    ClearTranslucentDrawRequests();
    ClearShadowDrawRequests();

    #pragma endregion



    // 데이터는 마음대로 접근 가능, 저장해놓은 위치에 값 업데이트 (아이디 그대로 유지)
    #pragma region SetData

    glassData.worldMat = DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(boxX, 0.0f, boxZ));
    UpdateDataBox(glassDataId, glassData);

    #pragma endregion



    // 프레임별 바뀌는 전역 데이터 세팅
    #pragma region SetCondition

    UpdateCamera();

    #pragma endregion



    // 렌더러에게 이거 가지고 그려줘 요청해요
    // 리소스는 해제 시 아이디가 바뀔 수 있음, 매 프레임 이름(핸들)로 다시 얻어서 세팅
    #pragma region DrawRequest

    groundDrawReq.drawProp = GetDrawPropId("GroundDraw");
    groundDrawReq.texProp = GetTexPropId("GroundMat");
    groundDrawReq.meshId = GetMeshId(L"res/Box2.obj");

    crateDrawReq.drawProp = GetDrawPropId("CrateDraw");
    crateDrawReq.texProp = GetTexPropId("CrateMat");
    crateDrawReq.meshId = GetMeshId(L"res/Box2.obj");

    foxDrawReq.drawProp = GetDrawPropId(RESERVED_DRAWPROP_OPAQUE_SKINNED_NAME);
    foxDrawReq.texProp = GetTexPropId("FoxMat");
    foxDrawReq.meshId = GetMeshId(L"res/Fox.gltf");

    glassDrawReq.drawProp = GetDrawPropId("GlassDraw");
    glassDrawReq.texProp = GetTexPropId("GlassMat");
    glassDrawReq.meshId = GetMeshId(L"res/Box2.obj");


    SendRTB(groundDrawReq);
    SendRTB(crateDrawReq);
    SendRTB(foxDrawReq);
    SendRTB(glassDrawReq);


    // 그림자 요청도 같은 텍스처 조합 / 메쉬로 세팅
    // - 기본 제공 리소스는 아이디 변동 없음, 그대로 사용
    crateShadowReq.texProp = crateDrawReq.texProp;
    crateShadowReq.meshId = crateDrawReq.meshId;

    foxShadowReq.texProp = foxDrawReq.texProp;
    foxShadowReq.meshId = foxDrawReq.meshId;

    glassShadowReq.texProp = glassDrawReq.texProp;
    glassShadowReq.meshId = glassDrawReq.meshId;


    // 그림자는 타일을 세팅해놓은 액션으로 직접 지정 후 요청 전송
    if (sunShadow.enable)
    {
        RTB_ActionDraw& sunShadowAction = LayerList[0].actionGlobalSDWs[sunShadow.cascade];

        SendRTB(crateShadowReq, sunShadowAction);
        SendRTB(glassShadowReq, sunShadowAction);
        SendRTB(foxShadowReq, sunShadowAction);
    }

    if (spotShadow.enable)
    {
        RTB_ActionDraw& spotShadowAction = LayerList[0].actionLocalSDWs[spotShadow.tile];

        SendRTB(crateShadowReq, spotShadowAction);
        SendRTB(glassShadowReq, spotShadowAction);
        SendRTB(foxShadowReq, spotShadowAction);
    }


    // 파티클 계산용 데이터 세팅 및 전송 (기본 제공 파티클, 간편 사용)
    particleSet.dt = 1.0f / 60.0f;
    particleSet.time += particleSet.dt;
    particleSet.emitPos = DirectX::XMFLOAT3{boxX, 1.2f, boxZ};

    SendRTB(particleSet);

    if (particleSet.enable)
        SendRTB(MakeParticleRequest(particleBuffer));

    #pragma endregion

}

void GameService::OnRender()
{
}


void GameService::OnResize(int width, int height)
{
    Layer_Viewport viewport = {0, 0, static_cast<float>(width), static_cast<float>(height)};

    SetLayerViewport(viewport);

    projM = MakeProjection(width, height);
}


void GameService::OnMouseMove(int x, int y)
{
    if (lookOn)
    {
        camYaw += (float)(x - mouseX) * CAM_LOOK_SPEED;
        camPitch -= (float)(y - mouseY) * CAM_LOOK_SPEED;

        const float pitchLimit = DirectX::XMConvertToRadians(89.0f);

        if (camPitch > pitchLimit)
            camPitch = pitchLimit;
        if (camPitch < -pitchLimit)
            camPitch = -pitchLimit;
    }

    mouseX = x;
    mouseY = y;
}

void GameService::OnKeyDown(int keyCode)
{
    if (keyCode == (int)KeyCode::W)
        camF = true;
    if (keyCode == (int)KeyCode::S)
        camB = true;
    if (keyCode == (int)KeyCode::D)
        camR = true;
    if (keyCode == (int)KeyCode::A)
        camL = true;
    if (keyCode == (int)KeyCode::E)
        camU = true;
    if (keyCode == (int)KeyCode::Q)
        camD = true;
    if (keyCode == (int)KeyCode::Mouse1)
        lookOn = true;


    if (keyCode == (int)KeyCode::RightArrow)
        boxR = true;
    if (keyCode == (int)KeyCode::LeftArrow)
        boxL = true;
    if (keyCode == (int)KeyCode::UpArrow)
        boxU = true;
    if (keyCode == (int)KeyCode::DownArrow)
        boxD = true;

    if (keyCode == (int)KeyCode::Z)
        glassData.blendColor.w = glassData.blendColor.w > 0.1f ? glassData.blendColor.w - 0.1f : 0.0f;
    if (keyCode == (int)KeyCode::X)
        glassData.blendColor.w = glassData.blendColor.w < 0.9f ? glassData.blendColor.w + 0.1f : 1.0f;


    if (keyCode == (int)KeyCode::Num1)
    {
        tonemapSet.enable = !tonemapSet.enable;
        SendRTB(tonemapSet);
    }
    if (keyCode == (int)KeyCode::Num2)
    {
        fxaaSet.enable = !fxaaSet.enable;
        SendRTB(fxaaSet);
    }
    if (keyCode == (int)KeyCode::Num3)
    {
        ssaoSet.enable = !ssaoSet.enable;
        aoBlurSet.enable = ssaoSet.enable;
        SendRTB(ssaoSet);
        SendRTB(aoBlurSet);
    }
    if (keyCode == (int)KeyCode::Num4)
    {
        bloomSet.enable = !bloomSet.enable;
        SendRTB(bloomSet);
    }
    if (keyCode == (int)KeyCode::Num5)
    {
        sunShadow.enable = !sunShadow.enable;
        spotShadow.enable = sunShadow.enable;
        SendRTB(sunShadow);
        SendRTB(spotShadow);
    }
    if (keyCode == (int)KeyCode::Num6)
        particleSet.enable = !particleSet.enable;
    if (keyCode == (int)KeyCode::C)
    {
        deferredOn = !deferredOn;
        SetLayerPipelineType(deferredOn ? Layer_PipelineType_Deferred : Layer_PipelineType_Forward);
    }
}

void GameService::OnKeyUp(int keyCode)
{
    if (keyCode == (int)KeyCode::W)
        camF = false;
    if (keyCode == (int)KeyCode::S)
        camB = false;
    if (keyCode == (int)KeyCode::D)
        camR = false;
    if (keyCode == (int)KeyCode::A)
        camL = false;
    if (keyCode == (int)KeyCode::E)
        camU = false;
    if (keyCode == (int)KeyCode::Q)
        camD = false;
    if (keyCode == (int)KeyCode::Mouse1)
        lookOn = false;


    if (keyCode == (int)KeyCode::RightArrow)
        boxR = false;
    if (keyCode == (int)KeyCode::LeftArrow)
        boxL = false;
    if (keyCode == (int)KeyCode::UpArrow)
        boxU = false;
    if (keyCode == (int)KeyCode::DownArrow)
        boxD = false;
}

}
