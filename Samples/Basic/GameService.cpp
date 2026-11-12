#include "GameService.h"

#include "DataHub_Interface.h"
#include "DataHub_FileLoader.h"

#include <string_view>


namespace Hakko
{

using namespace DataHub;


float x, y;
bool flagR, flagL, flagU, flagD;

auto objPos = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
auto projM = DirectX::XMMatrixIdentity();


MeshFileInfo info;

std::string_view propName;


RTB_DrawTP boxDrawReq;
DataBoxID boxDataId;
DtB_3DGeneral boxData;

RTB_DrawOPQ foxDrawReq;
DataBoxID foxGnrDataId;
DataBoxID foxOptDataId;



GameService::GameService()
{
}


bool GameService::Init(int width, int height)
{
    x = y = 0.0f;
    flagR = flagL = flagU = flagD = false;



    // 리소스 이름이 곧 나중에 사용할 키, 잘 보관해 두세요
    #pragma region UploadResources

    if (!LoadMesh(L"res/Box.obj"))
        return false;

    if (!LoadMesh(L"res/Box2.obj"))
        return false;
    
    if (!LoadMesh(L"res/Sphere.obj"))
        return false;
    
    if (LoadMesh(L"res/Fox.gltf", &info) == 0)
        return false;


    if (!LoadTexture(L"res/skybox.dds", 
                        TextureFileFormat_DDS, 
                        TextureType_CubeMap
                    ))
        return false;

    if (!LoadTexture(L"res/crate1_diffuse.png", 
                        TextureFileFormat_None, 
                        TextureType_Diffuse
                    ))
        return false;

    if (!LoadTexture(L"res/crate1_normal.png", 
                        TextureFileFormat_None,
                        TextureType_Default
                    ))
        return false;

    if (!LoadTexture(L"res/Texture.png", 
                        TextureFileFormat_None,
                        TextureType_Diffuse
                    ))
        return false;

    

    RTB_UpdateResources res = {};
    res.command = RTBCMD_Upload_Resource;

    SendRTB(res);

    #pragma endregion


    
    // 익히 머터리얼이라고 부르는 그겁니다
    #pragma region BuildProps

    TexProp tp = {};
    tp.texId[0] = GetTexId(L"res/crate1_diffuse.png");
    tp.texId[1] = GetTexId(L"res/crate1_normal.png");
    tp.texId[2] = GetTexId(L"None_ORM");
    tp.texId[3] = GetTexId(L"None_Emission");

    CreateTexProp("boxMat", tp);

    propName = "boxMat";



    TexProp tp2 = {};
    tp2.texId[0] = GetTexId(L"res/Texture.png");
    tp2.texId[1] = GetTexId(L"None_Normal");
    tp2.texId[2] = GetTexId(L"None_ORM");
    tp2.texId[3] = GetTexId(L"None_Emission");

    CreateTexProp("FoxMat", tp2);



    RTB_UpdateResources rtbProp;
    rtbProp.command = RTBCMD_Upload_Prop;

    SendRTB(rtbProp);

    #pragma endregion

    

    // 화면 층을 나누고(Layer) 층별로 배경이나 기타 설정을 해줘요
    #pragma region SetLayerOption

    SetLayerBackground(GetTexId(L"res/skybox.dds"));

    #pragma endregion


    
    // 전역 데이터, 카메라나 조명 값을 설정해요
    #pragma region SetCondition
    
    auto eyePos = DirectX::XMVectorSet(3.0f, 5.0f, -10.0f, 1.0f);
    auto upVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    float fov = DirectX::XMConvertToRadians(60.0f);
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    float nearZ = 0.1f;
    float farZ = 1000.0f;

    auto viewM = DirectX::XMMatrixLookAtLH(eyePos, objPos, upVec);
    projM = DirectX::XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);



    RTB_SetCamera rtbCam = {};
    rtbCam.view = DirectX::XMMatrixTranspose(viewM);
    rtbCam.proj = DirectX::XMMatrixTranspose(projM);
    rtbCam.viewProj = DirectX::XMMatrixMultiplyTranspose(viewM, projM);
    DirectX::XMStoreFloat3(&rtbCam.eyePos, eyePos);

    SendRTB(rtbCam);


    RTB_SetGlobalLight rtbLight = {};
    rtbLight.ambient = DirectX::XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
    rtbLight.color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    DirectX::XMStoreFloat3(&rtbLight.direction, DirectX::XMVector3Normalize(DirectX::XMVectorSet(-1.0f, -1.0f, 1.0f, 0.0f)));

    SendRTB(rtbLight);

    #pragma endregion


    
    // 렌더러에 넘겨줄수 있는 데이터를 보관해요 - 데이터는 서비스 자율 접근, 고정 아이디를 씁니다
    #pragma region StoreData

    auto worldM = DirectX::XMMatrixTranslationFromVector(objPos);


    boxData = DtB_3DGeneral();
    boxData.worldMat = DirectX::XMMatrixTranspose(worldM);
    boxData.blendColor = DirectX::XMFLOAT4{1,1,1,1};
    boxData.roughness = 1;
    boxData.metalic = 1;

    boxDataId = AddDataBox(boxData);


    foxGnrDataId = AddDataBox(boxData);


    DtB_3DOptional bone = {};
    for (size_t i = 0; i < SKIN_BONE_MAX; i++)
        bone.boneTransforms[i] = DirectX::XMMatrixIdentity();

    foxOptDataId = AddDataBox(bone);

    #pragma endregion


    return true;
}

void GameService::Finalize()
{
    RemoveDataBox(DataBoxType_DataBox_Render3DOptional, foxOptDataId);
    RemoveDataBox(DataBoxType_DataBox_Render3DGeneral, foxGnrDataId);
    RemoveDataBox(DataBoxType_DataBox_Render3DGeneral, boxDataId);
}


void GameService::Update()
{
    if (flagR)
        x += 0.05f;
    if (flagL)
        x -= 0.05f;
    if (flagU)
        y += 0.05f;
    if (flagD)
        y -= 0.05f;



    // 이전 프레임 데이터는 서비스에서 직접 비워요 (선택)
    #pragma region ClearPrevRequest

    ClearOpaqueDrawRequests();
    ClearTranslucentDrawRequests();
    ClearScreenDrawRequests();

    #pragma endregion


    
    // 데이터는 마음대로 접근 가능, 저장해놓은 위치에 값 업데이트 (아이디 그대로 유지)
    #pragma region SetData

    objPos = {x, 0.0f, y, 1.0f};

    auto worldM = DirectX::XMMatrixTranslationFromVector(objPos);


    // 값 통째로 올리기
    boxData.worldMat = DirectX::XMMatrixTranspose(worldM);
    UpdateDataBox(boxDataId, boxData);

    // 일부 칸만 바로 고치고 싶으면 리스트를 받아서 써도 돼요 (길이는 건드리지 않기)
    // auto& generalList = DataBoxCustomizer::GetDataBoxList<DtB_3DGeneral>();
    // generalList[boxDataId].worldMat = DirectX::XMMatrixTranspose(worldM);

    

    auto eyePos = DirectX::XMVectorSet(3.0f, 5.0f, -10.0f, 1.0f);
    auto upVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    
    auto viewM = DirectX::XMMatrixLookAtLH(eyePos, objPos, upVec);


    RTB_SetCamera rtbCam = {};
    rtbCam.view = DirectX::XMMatrixTranspose(viewM);
    rtbCam.proj = DirectX::XMMatrixTranspose(projM);
    rtbCam.viewProj = DirectX::XMMatrixMultiplyTranspose(viewM, projM);
    DirectX::XMStoreFloat3(&rtbCam.eyePos, eyePos);

    SendRTB(rtbCam);

    #pragma endregion



    // 렌더러에게 이거 가지고 그려줘 요청해요
    #pragma region DrawRequest

    // 리소스 아이디는 항상 바뀔수 있기 때문에, 매 프레임 이름을 통해 얻어오는걸 권장
    boxDrawReq.depth = 0;
    boxDrawReq.drawProp = GetDrawPropId("Default_Translucent");
    boxDrawReq.texProp = GetTexPropId(propName);
    boxDrawReq.meshId = GetMeshId(L"res/Box2.obj");

    boxDrawReq.generalId = boxDataId;


    foxDrawReq.alphaClip = false;
    foxDrawReq.drawProp = GetDrawPropId("Default_Opaque_Skinned");
    foxDrawReq.texProp = GetTexPropId("FoxMat");

    foxDrawReq.meshId = GetMeshId(L"res/Fox.gltf");

    foxDrawReq.generalId = foxGnrDataId;
    foxDrawReq.optionalId = foxOptDataId;


    SendRTB(boxDrawReq);
    // for (uint16_t s = 0; s < info.subMeshCount; s++) { foxDrawReq.subMeshId = s; SendRTB(foxDrawReq); }

    #pragma endregion

}

void GameService::OnRender()
{
}


void GameService::OnResize(int width, int height)
{
    Layer_Viewport viewport = {0,0,static_cast<float>(width),static_cast<float>(height)};

    SetLayerViewport(viewport);
    
    
    
    auto eyePos = DirectX::XMVectorSet(3.0f, 5.0f, -10.0f, 1.0f);
    auto upVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    float fov = DirectX::XMConvertToRadians(60.0f);
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    float nearZ = 0.1f;
    float farZ = 1000.0f;

    auto viewM = DirectX::XMMatrixLookAtLH(eyePos, objPos, upVec);
    projM = DirectX::XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);


    RTB_SetCamera rtbCam = {};
    rtbCam.view = DirectX::XMMatrixTranspose(viewM);
    rtbCam.proj = DirectX::XMMatrixTranspose(projM);
    rtbCam.viewProj = DirectX::XMMatrixMultiplyTranspose(viewM, projM);
    DirectX::XMStoreFloat3(&rtbCam.eyePos, eyePos);

    SendRTB(rtbCam);
}


void GameService::OnMouseMove(int x, int y)
{
}

void GameService::OnKeyDown(int keyCode)
{
    if (keyCode == (int)KeyCode::D || keyCode == (int)KeyCode::RightArrow)
        flagR = true;
    if (keyCode == (int)KeyCode::A || keyCode == (int)KeyCode::LeftArrow)
        flagL = true;
    if (keyCode == (int)KeyCode::W || keyCode == (int)KeyCode::UpArrow)
        flagU = true;
    if (keyCode == (int)KeyCode::S || keyCode == (int)KeyCode::DownArrow)
        flagD = true;
    if (keyCode == (int)KeyCode::Q)
        boxData.blendColor.w = boxData.blendColor.w > 0.1f ? boxData.blendColor.w - 0.1f : 0.0f;
    if (keyCode == (int)KeyCode::E)
        boxData.blendColor.w = boxData.blendColor.w < 0.9f ? boxData.blendColor.w + 0.1f : 1.0f;
}

void GameService::OnKeyUp(int keyCode)
{
    if (keyCode == (int)KeyCode::D || keyCode == (int)KeyCode::RightArrow)
        flagR = false;
    if (keyCode == (int)KeyCode::A || keyCode == (int)KeyCode::LeftArrow)
        flagL = false;
    if (keyCode == (int)KeyCode::W || keyCode == (int)KeyCode::UpArrow)
        flagU = false;
    if (keyCode == (int)KeyCode::S || keyCode == (int)KeyCode::DownArrow)
        flagD = false;
}

}
