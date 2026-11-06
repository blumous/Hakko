#include "GameService.h"

#include "DataHub_Interface.h"

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

    
    RTB_UpdateResource res = {};
    res.command = RTBCMD_UpdateResource_Upload;

    SendRTB(res);

    #pragma endregion


    #pragma region BuildProps

    TexProp tp = {};
    tp.texId[0] = GetTexId(L"res/crate1_diffuse.png");
    tp.texId[1] = GetTexId(L"res/crate1_normal.png");
    tp.texId[2] = GetTexId(L"None_ORM");
    tp.texId[3] = GetTexId(L"None_Emission");

    AddTextureProp("boxMat", tp);

    propName = "boxMat";



    TexProp tp2 = {};
    tp2.texId[0] = GetTexId(L"res/Texture.png");
    tp2.texId[1] = GetTexId(L"None_Normal");
    tp2.texId[2] = GetTexId(L"None_ORM");
    tp2.texId[3] = GetTexId(L"None_Emission");

    AddTextureProp("FoxMat", tp2);


    RTB_UpdateProp rtbProp;
    rtbProp.command = RTBCMD_UpdateProp_FindOrBuild;

    SendRTB(rtbProp);

    #pragma endregion


    #pragma region SetLayerOption

    LayerList[0].backGround.bgTexId = GetTexId(L"res/skybox.dds");

    // LayerList[0].viewport = {...};

    #pragma endregion


    #pragma region SetCondition
    
    auto eyePos = DirectX::XMVectorSet(3.0f, 5.0f, -10.0f, 1.0f);
    auto upVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    float fov = DirectX::XMConvertToRadians(60.0f);
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    float nearZ = 0.1f;
    float farZ = 1000.0f;

    auto viewM = DirectX::XMMatrixLookAtLH(eyePos, objPos, upVec);
    projM = DirectX::XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);


    DtB_Condition cond = {};
    DirectX::XMStoreFloat3(&cond.camera_EyePos, eyePos);
    cond.camera_View = DirectX::XMMatrixTranspose(viewM);
    cond.camera_Proj = DirectX::XMMatrixTranspose(projM);
    cond.camera_ViewProj = DirectX::XMMatrixMultiplyTranspose(viewM, projM);
    cond.global_LightAmbient = DirectX::XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
    cond.global_LightColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    DirectX::XMStoreFloat3(&cond.global_LightDirection, DirectX::XMVector3Normalize(DirectX::XMVectorSet(-1.0f, -1.0f, 1.0f, 0.0f)));

    RTB_SetCondition rtbCond;
    rtbCond.condition = cond;

    SendRTB(rtbCond);

    #pragma endregion


    #pragma region StoreData

    auto worldM = DirectX::XMMatrixTranslationFromVector(objPos);


    DtB_3DGeneral data = {};
    data.worldMat = DirectX::XMMatrixTranspose(worldM);
    data.blendColor = DirectX::XMFLOAT4{1,1,1,1};
    data.roughness = 1;
    data.metalic = 1;

    boxDataId = AddDataBox(data);


    foxGnrDataId = AddDataBox(data);

    DtB_3DOptional bone = {};
    for (size_t i = 0; i < SKIN_BONE_MAX; i++)
    {
        bone.boneTransforms[i] = DirectX::XMMatrixIdentity();
    }

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


void GameService::OnResize(int width, int height)
{
    Layer_Viewport viewport = {0,0,static_cast<float>(width),static_cast<float>(height)};

    auto& pb = LayerList[0];

    pb.viewport = viewport;
    
    
    auto eyePos = DirectX::XMVectorSet(3.0f, 5.0f, -10.0f, 1.0f);
    auto upVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    float fov = DirectX::XMConvertToRadians(60.0f);
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    float nearZ = 0.1f;
    float farZ = 1000.0f;

    auto viewM = DirectX::XMMatrixLookAtLH(eyePos, objPos, upVec);
    auto projM = DirectX::XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);



    DtB_Condition cond = {};
    DirectX::XMStoreFloat3(&cond.camera_EyePos, eyePos);
    cond.camera_View = DirectX::XMMatrixTranspose(viewM);
    cond.camera_Proj = DirectX::XMMatrixTranspose(projM);
    cond.camera_ViewProj = DirectX::XMMatrixMultiplyTranspose(viewM, projM);
    cond.global_LightAmbient = DirectX::XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
    cond.global_LightColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    DirectX::XMStoreFloat3(&cond.global_LightDirection, DirectX::XMVector3Normalize(DirectX::XMVectorSet(-1.0f, -1.0f, 1.0f, 0.0f)));

    RTB_SetCondition rtbCond;
    rtbCond.condition = cond;

    SendRTB(rtbCond);
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


    #pragma region ClearPrevRequest

    LayerList[0].actionOPQ.drawReqList.clear();
    LayerList[0].actionTP.drawReqList.clear();
    LayerList[0].actionSC.drawReqList.clear();

    #pragma endregion


    #pragma region SetData

    objPos = {x, 0.0f, y, 1.0f};
    auto worldM = DirectX::XMMatrixTranslationFromVector(objPos);

    DataBoxCustomizer::GetDataBoxList<DtB_3DGeneral>()[boxDataId].worldMat = DirectX::XMMatrixTranspose(worldM);

    
    auto eyePos = DirectX::XMVectorSet(3.0f, 5.0f, -10.0f, 1.0f);
    auto upVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    
    auto viewM = DirectX::XMMatrixLookAtLH(eyePos, objPos, upVec);

    LayerList[0].condition.camera_View = viewM;
    LayerList[0].condition.camera_View = DirectX::XMMatrixMultiplyTranspose(viewM, projM);

    #pragma endregion


    #pragma region DrawRequest

    // 리소스 아이디는 바뀔수 있기 때문에 매 프레임 혹은 갱신 시마다 이름을 통해 얻어와야 해요

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
        DataBoxCustomizer::GetDataBoxList<DtB_3DGeneral>()[boxDataId].blendColor.w != 0.0f ? 
            DataBoxCustomizer::GetDataBoxList<DtB_3DGeneral>()[boxDataId].blendColor.w -= 0.1f :
            0;
    if (keyCode == (int)KeyCode::E)
        DataBoxCustomizer::GetDataBoxList<DtB_3DGeneral>()[boxDataId].blendColor.w != 1.0f ? 
            DataBoxCustomizer::GetDataBoxList<DtB_3DGeneral>()[boxDataId].blendColor.w += 0.1f :
            0;
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
