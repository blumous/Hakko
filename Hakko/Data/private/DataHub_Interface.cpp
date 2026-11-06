#include "DataHub_Interface.h"

#include <utility>


namespace Hakko::DataHub
{

    #pragma region Interface

    MeshID LoadMesh(const wchar_t* path, MeshFileInfo* info)
    {
        const MeshID existing = GetMeshId(path);
        if (existing != 0)
            return existing;

        Mesh data = {};
        if (!ReadMeshFile(path, data, info))
            return 0;

        return CreateMesh(path, std::move(data));
    }

    TexID LoadTexture(const wchar_t* path, TextureFileFormat format, TextureType type)
    {
        const TexID existing = GetTexId(path);
        if (existing != 0)
            return existing;

        Texture tex = {};
        if (!ReadTextureFile(path, format, type, tex))
            return 0;

        return CreateTexture(path, std::move(tex));
    }

    template <>
    DataHub::DataBoxID DataHub::AddDataBox<DataHub::DataBox_RenderLight>(const DataBox_RenderLight& dataBox)
    {
        DataBoxID idx;

        if (blankedLightIdList.size() > 0)
        {
            idx = blankedLightIdList.back();
            RenderLightList[idx] = dataBox;
            blankedLightIdList.pop_back();
        }
        else
        {
            idx = RenderLightList.size();
            RenderLightList.emplace_back(dataBox);
        }

        return idx;
    }

    template <>
    DataHub::DataBoxID DataHub::AddDataBox<DataHub::DataBox_RenderLightMatrix>(const DataBox_RenderLightMatrix& dataBox)
    {
        DataBoxID idx;

        if (blankedLightMatrixIdList.size() > 0)
        {
            idx = blankedLightMatrixIdList.back();
            RenderLightMatrixList[idx] = dataBox;
            blankedLightMatrixIdList.pop_back();
        }
        else
        {
            idx = RenderLightMatrixList.size();
            RenderLightMatrixList.emplace_back(dataBox);
        }

        return idx;
    }


    void DataHub::RemoveDataBox(DataBoxTID tid, DataBoxID index)
    {
        if (tid == DataBoxType_DataBox_RenderLight)
        {
            blankedLightIdList.push_back(index);
            return;
        }
        else if (tid == DataBoxType_DataBox_RenderLightMatrix)
        {
            blankedLightMatrixIdList.push_back(index);
            return;
        }

        DataBoxCustomizer::RemoveDataBox(tid, index);
    }


    template <>
    void DataHub::SendRTB<DataHub::RTB_UpdateResource>(const RTB_UpdateResource& RTB, uint8_t layer)
    {
        switch (RTB.command)
        {
        case RTBCMD_UpdateResource_Upload:
            flag_updateResources_Upload = true;   break;
        
        case RTBCMD_UpdateResource_Release:
            flag_updateResources_Release = true;  break;
        }
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_UpdateProp>(const RTB_UpdateProp& RTB, uint8_t layer)
    {
        switch (RTB.command)
        {
        case RTBCMD_UpdateProp_FindOrBuild:
            flag_updateProps_FindOrBuild = true;     break;
        
        case RTBCMD_UpdateProp_Release:
            flag_updateProps_Release = true;         break;
        }
    }


    template <>
    void DataHub::SendRTB<DataHub::RTB_SetRenderCondition>(const RTB_SetRenderCondition& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        LayerList[layer].condition = RTB.condition;
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetRenderLight>(const RTB_SetRenderLight& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.slot >= RENDER_LIGHT_MAX || RTB.light >= RenderLightList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetLight: 슬롯 / 조명 id 범위 밖");
            return;
        }

        LayerList[layer].lights[RTB.slot] = RTB.light;
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetRenderLightMatrix>(const RTB_SetRenderLightMatrix& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.slot >= RENDER_LIGHT_MAX || RTB.lightMatrix >= RenderLightMatrixList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetLightMatrix: 슬롯 / 행렬 id 범위 밖");
            return;
        }

        LayerList[layer].lightMatrixes[RTB.slot] = RTB.lightMatrix;
    }
    
    template <>
    void DataHub::SendRTB<DataHub::RTB_DrawRequestOpaque>(const RTB_DrawRequestOpaque& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.meshId >= meshIdLUT.size() || RTB.subMeshId >= meshSubMeshCountList[RTB.meshId])
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest: 메쉬 / 서브메쉬 id 범위 밖 (요청 버림)");
            return;
        }
        if (RTB.drawProp >= drawPropIdLUT.size() || RTB.texProp >= texPropIdLUT.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest: drawProp / texProp id 범위 밖 (요청 버림)");
            return;
        }

        RenderIdOpaque idp;
        idp.alphaClip = RTB.alphaClip;  idp.layer = RTB.layer;  idp.drawProp = RTB.drawProp;
        idp.texProp = RTB.texProp;      idp.meshId = RTB.meshId;  idp.depth = RTB.depth;

        RTB_DrawRequest req = {};
        req.renderId = idp;
        req.meshId = RTB.meshId;
        req.subMeshId = RTB.subMeshId;
        req.dataBoxIds[0] = RTB.generalId;
        req.dataBoxIds[1] = RTB.optionalId;
        LayerList[layer].actionOPQ.drawReqList.push_back(req);
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_DrawRequestTranslucent>(const RTB_DrawRequestTranslucent& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.meshId >= meshIdLUT.size() || RTB.subMeshId >= meshSubMeshCountList[RTB.meshId])
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest: 메쉬 / 서브메쉬 id 범위 밖 (요청 버림)");
            return;
        }
        if (RTB.drawProp >= drawPropIdLUT.size() || RTB.texProp >= texPropIdLUT.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest: drawProp / texProp id 범위 밖 (요청 버림)");
            return;
        }

        RenderIdTranslucent idp;
        idp.depth = RTB.depth;  idp.drawProp = RTB.drawProp;  idp.texProp = RTB.texProp;  idp.iid = RTB.iid;

        RTB_DrawRequest req = {};
        req.renderId = idp;
        req.meshId = RTB.meshId;
        req.subMeshId = RTB.subMeshId;
        req.dataBoxIds[0] = RTB.generalId;
        req.dataBoxIds[1] = RTB.optionalId;
        LayerList[layer].actionTP.drawReqList.push_back(req);
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_DrawRequestScreen>(const RTB_DrawRequestScreen& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.drawProp >= drawPropIdLUT.size() || RTB.texture >= textureIdLUT.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest(SC): drawProp / 텍스처 id 범위 밖 (요청 버림)");
            return;
        }

        RenderIdScreen idp;
        idp.layer = RTB.layer;  idp.translucent = RTB.translucent;  idp.texture = RTB.texture;
        idp.drawProp = RTB.drawProp;  idp.iid = RTB.iid;

        RTB_DrawRequest req = {};
        req.renderId = idp;
        req.meshId = 1;
        req.subMeshId = 0;
        req.dataBoxIds[0] = RTB.screenId;
        LayerList[layer].actionSC.drawReqList.push_back(req);
    }


    bool MakeDrawRequestAndProps(
        DrawOption option, 
        const char* drawPropName, const char* texPropName, 
        RTB_DrawRequestOpaque& rtb,
        MeshID meshId, DataBoxID generalId, DataBoxID optionalId)
    {
        if (option.blendType != DrawProp_BlendType_None)
            return false;


        DrawProp dp = {};
        dp.depthUsage = DrawProp_DepthUsage_Opaque;
        dp.cullType = option.cullType;
        dp.blendType = DrawProp_BlendType_None;
        dp.stecilOpt.useStencil = false;
        dp.vertexType = option.vertexType;
        dp.VSId = option.VSId;
        dp.PSId = option.PSId;

        TextureProp tp = {};
        tp.texId[0] = option.diffuseTexId;      tp.texMipLevels[0] = option.diffuseMipLevels;
        tp.texId[1] = option.diffuseTexId;      tp.texMipLevels[1] = option.diffuseMipLevels;
        tp.texId[2] = option.diffuseTexId;      tp.texMipLevels[2] = option.diffuseMipLevels;
        tp.texId[3] = option.diffuseTexId;      tp.texMipLevels[3] = option.diffuseMipLevels;
        for (size_t i = 4; i < TEXPROP_SLOT_COUNT; i++)
        {
            tp.texId[i] = option.flexibleTexId[i - 4];  tp.texMipLevels[i] = option.flexibleMipLevels[i - 4];
        }
        

        std::string_view dpSName(drawPropName);
        auto dpKey = HashString(dpSName);

        if (drawPropIdLUT.contains(dpKey))
            return false;

        std::string_view tpSName(texPropName);
        auto tpKey = HashString(tpSName);

        if (texPropIdLUT.contains(tpKey))
            return false;

        
        DrawPropID dpId = static_cast<DrawPropID>(drawPropIdLUT.size());
        drawPropIdLUT.emplace(dpKey, dpId);
        drawPropList.emplace_back(dp);

        TexPropID tpId = static_cast<TexPropID>(texPropIdLUT.size());
        texPropIdLUT.emplace(tpKey, tpId);
        texPropList.emplace_back(tp);
        

        rtb.alphaClip = option.alphaClipOrTranslucent;
        rtb.layer = 0U;
        rtb.drawProp = dpId;
        rtb.texProp = tpId;
        rtb.meshId = meshId;
        rtb.depth = 0U;
        rtb.generalId = generalId;
        rtb.optionalId = optionalId;


        return true;
    }

    bool MakeDrawRequestAndProps(
        DrawOption option, 
        const char *drawPropName, const char *texPropName, 
        RTB_DrawRequestTranslucent &rtb, 
        MeshID meshId, DataBoxID generalId, DataBoxID optionalId)
    {

        DrawProp dp = {};
        dp.depthUsage = DrawProp_DepthUsage_Translucent;
        dp.cullType = option.cullType;
        dp.blendType = option.blendType;
        dp.stecilOpt.useStencil = false;
        dp.vertexType = option.vertexType;
        dp.VSId = option.VSId;
        dp.PSId = option.PSId;

        TextureProp tp = {};
        tp.texId[0] = option.diffuseTexId;      tp.texMipLevels[0] = option.diffuseMipLevels;
        tp.texId[1] = option.diffuseTexId;      tp.texMipLevels[1] = option.diffuseMipLevels;
        tp.texId[2] = option.diffuseTexId;      tp.texMipLevels[2] = option.diffuseMipLevels;
        tp.texId[3] = option.diffuseTexId;      tp.texMipLevels[3] = option.diffuseMipLevels;
        for (size_t i = 4; i < TEXPROP_SLOT_COUNT; i++)
        {
            tp.texId[i] = option.flexibleTexId[i - 4];  tp.texMipLevels[i] = option.flexibleMipLevels[i - 4];
        }
        

        std::string_view dpSName(drawPropName);
        auto dpKey = HashString(dpSName);

        if (drawPropIdLUT.contains(dpKey))
            return false;

        std::string_view tpSName(texPropName);
        auto tpKey = HashString(tpSName);

        if (texPropIdLUT.contains(tpKey))
            return false;

        
        DrawPropID dpId = static_cast<DrawPropID>(drawPropIdLUT.size());
        drawPropIdLUT.emplace(dpKey, dpId);
        drawPropList.emplace_back(dp);

        TexPropID tpId = static_cast<TexPropID>(texPropIdLUT.size());
        texPropIdLUT.emplace(tpKey, tpId);
        texPropList.emplace_back(tp);
        

        rtb.depth = 0U;
        rtb.drawProp = dpId;
        rtb.texProp = tpId;
        rtb.iid = 0U;
        rtb.meshId = meshId;
        rtb.generalId = generalId;
        rtb.optionalId = optionalId;


        return true;
    }
    
    bool MakeDrawRequestAndProps(
        DrawOption option, 
        const char* drawPropName,
        RTB_DrawRequestScreen& rtb,
        TexID texId, DataBoxID screenId)
    {

        DrawProp dp = {};
        dp.cullType = option.cullType;
        dp.blendType = option.blendType;
        dp.stecilOpt.useStencil = false;
        dp.vertexType = VertexType_Simple;
        dp.VSId = option.VSId;
        dp.PSId = option.PSId;
        

        std::string_view dpSName(drawPropName);
        auto dpKey = HashString(dpSName);

        if (drawPropIdLUT.contains(dpKey))
            return false;

        
        DrawPropID dpId = static_cast<DrawPropID>(drawPropIdLUT.size());
        drawPropIdLUT.emplace(dpKey, dpId);
        drawPropList.emplace_back(dp);
        

        rtb.layer = 0U;
        rtb.translucent = option.alphaClipOrTranslucent;
        rtb.texture = texId;
        rtb.drawProp = dpId;
        rtb.iid = 0U;
        rtb.screenId = screenId;


        return true;
    }

    #pragma endregion

    

}
