#pragma once

#include "DataHub.h"
#include "DataHub_FileLoader.h"

// 데이터허브 사용에 도움을 주는 조작 헬퍼들입니다
// 선택 기능 헤더


namespace Hakko::DataHub
{
    
    #pragma region Interface

    template <std::derived_from<DataBox> T>
    DataBoxID AddDataBox(const T& dataBox)
    {
        static_assert(std::derived_from<T, DataBox_Custom>, "Condition / Light 는 전용 경로, 그 외는 DataBox_Custom 상속 필요");
        return DataBoxCustomizer::AddDataBox<T>(T(dataBox));
    }
    
    void RemoveDataBox(DataBoxTID tid, DataBoxID id);


    MeshID LoadMesh(const wchar_t* path, MeshFileInfo* info = nullptr);
    TexID LoadTexture(const wchar_t* path, TextureFileFormat format, TextureType type);


    template <std::derived_from<RTB> T>
    void SendRTB(const T& RTB, uint8_t layer = 0U) = delete;
    
    template <std::derived_from<RTB> T>
    void SendRTB(const T& RTB, ActionID action, uint8_t layer = 0U) = delete;


    // 미구현 (커스텀 파이프라인)
    template <std::derived_from<RTB_Action> T>
    bool AddActionNode(const T& Action, ActionID frontNode, uint8_t layer = 0U)
    {
        Internal::PushHubReport(HubReport_Contract, L"[Hub] AddActionNode: 커스텀 파이프라인 미구현");
        return false;
    }



    struct DrawOption
    {
        DrawProp_CullType cullType;
        DrawProp_BlendType blendType;
        bool alphaClipOrTranslucent;
        
        VertexType vertexType = VertexType_Static;
        ShaderID VSId;
        ShaderID PSId;

        TexID diffuseTexId = 1U;        uint16_t diffuseMipLevels = 0;
        TexID normalTexId = 2U;         uint16_t normalMipLevels = 0;
        TexID ORMTexId = 3U;            uint16_t ORMMipLevels = 0;
        TexID emissionTexId = 4U;       uint16_t emissionMipLevels = 0;
        TexID flexibleTexId[TEXPROP_SLOT_COUNT - 4] = {0};
        uint16_t flexibleMipLevels[TEXPROP_SLOT_COUNT - 4] = {0};
    };

    bool MakeDrawRequestAndProps(
        DrawOption option, 
        const char* drawPropName, const char* texPropName, 
        RTB_DrawRequestOpaque& rtb,
        MeshID meshId = 0U, DataBoxID generalId = 0U, DataBoxID optionalId = 0U
    );

    bool MakeDrawRequestAndProps(
        DrawOption option, 
        const char* drawPropName, const char* texPropName, 
        RTB_DrawRequestTranslucent& rtb,
        MeshID meshId = 0U, DataBoxID generalId = 0U, DataBoxID optionalId = 0U
    );


    bool MakeDrawRequestAndProps(
        DrawOption option, 
        const char* drawPropName, 
        RTB_DrawRequestScreen& rtb,
        TexID texId, DataBoxID screenId = 0U
    );

    #pragma endregion
    


#pragma region Template

template <>
DataHub::DataBoxID DataHub::AddDataBox<DataHub::DtB_Light>(const DtB_Light& dataBox);


template <>
void DataHub::SendRTB<DataHub::RTB_UpdateResource>(const RTB_UpdateResource& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_UpdateProp>(const RTB_UpdateProp& RTB, uint8_t layer);


template <>
void DataHub::SendRTB<DataHub::RTB_SetCondition>(const RTB_SetCondition& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetLight>(const RTB_SetLight& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetLightMatrix>(const RTB_SetLightMatrix& RTB, uint8_t layer);


template <>
void DataHub::SendRTB<DataHub::RTB_DrawOPQ>(const RTB_DrawOPQ& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_DrawTP>(const RTB_DrawTP& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_DrawSC>(const RTB_DrawSC& RTB, uint8_t layer);

#pragma endregion

}
