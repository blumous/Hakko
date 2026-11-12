#include "DataHub_FileLoader.h"

#include "DirectXTex.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <unordered_map>
#include <string>
#include <utility>

using namespace DirectX;
using namespace std;


namespace Hakko::DataHub
{

    TextureReadFormat FromDXGI(DXGI_FORMAT f);
    
    
    #pragma region FileLoader

    #pragma region Helper

    static void CollectMeshInstances(const aiNode* node, const aiMatrix4x4& parent, std::vector<std::pair<unsigned int, aiMatrix4x4>>& out)
    {
        const aiMatrix4x4 global = parent * node->mTransformation;

        for (unsigned int i = 0; i < node->mNumMeshes; i++)
            out.push_back({ node->mMeshes[i], global });

        for (unsigned int c = 0; c < node->mNumChildren; c++)
            CollectMeshInstances(node->mChildren[c], global, out);
    }

    TextureReadFormat FromDXGI(DXGI_FORMAT f)
    {
        switch (f) {
        case DXGI_FORMAT_R8_UNORM:              return TextureReadFormat::R8_UNorm;
        case DXGI_FORMAT_R8G8_UNORM:            return TextureReadFormat::RG8_UNorm;
        case DXGI_FORMAT_R8G8B8A8_UNORM:        return TextureReadFormat::RGBA8_UNorm;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:   return TextureReadFormat::RGBA8_SRGB;
        case DXGI_FORMAT_B8G8R8A8_UNORM:        return TextureReadFormat::BGRA8_UNorm;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:   return TextureReadFormat::BGRA8_SRGB;
        case DXGI_FORMAT_R10G10B10A2_UNORM:     return TextureReadFormat::R10G10B10A2_UNorm;
        case DXGI_FORMAT_R11G11B10_FLOAT:       return TextureReadFormat::R11G11B10_Float;
        case DXGI_FORMAT_R16_FLOAT:             return TextureReadFormat::R16_Float;
        case DXGI_FORMAT_R16G16_FLOAT:          return TextureReadFormat::RG16_Float;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:    return TextureReadFormat::RGBA16_Float;
        case DXGI_FORMAT_R32_FLOAT:             return TextureReadFormat::R32_Float;
        case DXGI_FORMAT_R32G32_FLOAT:          return TextureReadFormat::RG32_Float;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:    return TextureReadFormat::RGBA32_Float;
        case DXGI_FORMAT_R16_UNORM:             return TextureReadFormat::R16_UNorm;
        case DXGI_FORMAT_R16G16_UNORM:          return TextureReadFormat::RG16_UNorm;
        case DXGI_FORMAT_R16G16B16A16_UNORM:    return TextureReadFormat::RGBA16_UNorm;
        case DXGI_FORMAT_R8G8_SNORM:            return TextureReadFormat::RG8_SNorm;
        case DXGI_FORMAT_R8G8B8A8_SNORM:        return TextureReadFormat::RGBA8_SNorm;
        case DXGI_FORMAT_R16G16_SNORM:          return TextureReadFormat::RG16_SNorm;
        case DXGI_FORMAT_R8_UINT:               return TextureReadFormat::R8_UInt;
        case DXGI_FORMAT_R16_UINT:              return TextureReadFormat::R16_UInt;
        case DXGI_FORMAT_R32_UINT:              return TextureReadFormat::R32_UInt;
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:    return TextureReadFormat::RGB9E5_SharedExp;

        case DXGI_FORMAT_BC1_UNORM:             return TextureReadFormat::BC1_UNorm;
        case DXGI_FORMAT_BC1_UNORM_SRGB:        return TextureReadFormat::BC1_SRGB;
        case DXGI_FORMAT_BC2_UNORM:             return TextureReadFormat::BC2_UNorm;
        case DXGI_FORMAT_BC2_UNORM_SRGB:        return TextureReadFormat::BC2_SRGB;
        case DXGI_FORMAT_BC3_UNORM:             return TextureReadFormat::BC3_UNorm;
        case DXGI_FORMAT_BC3_UNORM_SRGB:        return TextureReadFormat::BC3_SRGB;
        case DXGI_FORMAT_BC4_UNORM:             return TextureReadFormat::BC4_UNorm;
        case DXGI_FORMAT_BC4_SNORM:             return TextureReadFormat::BC4_SNorm;
        case DXGI_FORMAT_BC5_UNORM:             return TextureReadFormat::BC5_UNorm;
        case DXGI_FORMAT_BC5_SNORM:             return TextureReadFormat::BC5_SNorm;
        case DXGI_FORMAT_BC6H_UF16:             return TextureReadFormat::BC6H_UF16;
        case DXGI_FORMAT_BC6H_SF16:             return TextureReadFormat::BC6H_SF16;
        case DXGI_FORMAT_BC7_UNORM:             return TextureReadFormat::BC7_UNorm;
        case DXGI_FORMAT_BC7_UNORM_SRGB:        return TextureReadFormat::BC7_SRGB;

        case DXGI_FORMAT_D16_UNORM:             return TextureReadFormat::D16_UNorm;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:     return TextureReadFormat::D24_UNorm_S8_UInt;
        case DXGI_FORMAT_D32_FLOAT:             return TextureReadFormat::D32_Float;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:  return TextureReadFormat::D32_Float_S8X24_UInt;

        default:                                return TextureReadFormat::Unknown;
        }
    }

    #pragma endregion


    bool ReadMeshFile(const wchar_t* filename, Mesh& out, MeshFileInfo* info)
    {
        if (info != nullptr)
            *info = {};

        std::string path;
        {
            const int n = WideCharToMultiByte(CP_UTF8, 0, filename, -1, nullptr, 0, nullptr, nullptr);
            if (n <= 1)
                return false;
            path.resize((size_t)n - 1);
            WideCharToMultiByte(CP_UTF8, 0, filename, -1, path.data(), n, nullptr, nullptr);
        }


        Assimp::Importer importer;
        importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

        const unsigned int flags =
            aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_JoinIdenticalVertices |
            aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded |
            aiProcess_LimitBoneWeights;

        const aiScene* scene = importer.ReadFile(path, flags);
        if (scene == nullptr || scene->mNumMeshes == 0)
        {
            Internal::PushHubReport(HubReport_Load, L"[Load] 어심프 파싱 실패");
            OutputDebugStringA("[Load] 어심프 파싱 실패: "); OutputDebugStringA(importer.GetErrorString()); OutputDebugStringA("\n");
            return false;
        }


        bool skinned = false;
        size_t totalVerts = 0;
        size_t totalIdx = 0;

        for (unsigned int m = 0; m < scene->mNumMeshes; m++)
        {
            const aiMesh* mesh = scene->mMeshes[m];
            if (mesh->HasBones())
                skinned = true;
            totalVerts += mesh->mNumVertices;
            totalIdx += (size_t)mesh->mNumFaces * 3;
        }

        if (totalIdx == 0)
            return false;

        out.vertexType = skinned ? VertexType_Skinned : VertexType_Static;
        out.indices.reserve(totalIdx);
        out.subMeshes.reserve(scene->mNumMeshes);

        std::vector<VertexStatic>& vs = out.verticesStatic;
        std::vector<VertexSkinned>& vk = out.verticesSkinned;
        if (skinned) vk.reserve(totalVerts); else vs.reserve(totalVerts);

        std::unordered_map<std::string, uint32_t> boneLUT;
        std::vector<uint8_t> boneSlot;

        std::vector<std::pair<unsigned int, aiMatrix4x4>> instances;
        CollectMeshInstances(scene->mRootNode, aiMatrix4x4(), instances);
        if (instances.empty())
            for (unsigned int m = 0; m < scene->mNumMeshes; m++)
                instances.push_back({ m, aiMatrix4x4() });


        for (size_t inst = 0; inst < instances.size(); inst++)
        {
            const aiMesh* mesh = scene->mMeshes[instances[inst].first];
            const aiMatrix4x4& xf = instances[inst].second;
            const bool bake = !mesh->HasBones();
            aiMatrix3x3 nxf(xf);
            if (bake)
            {
                nxf.Inverse();
                nxf.Transpose();
            }

            if ((mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) == 0 || mesh->mNumFaces == 0 || mesh->mNumVertices == 0)
                continue;

            SubMesh sub = {};
            sub.baseVertex = (uint32_t)(skinned ? vk.size() : vs.size());
            sub.indexStart = (uint32_t)out.indices.size();

            const bool hasUV = mesh->HasTextureCoords(0);
            const bool hasNormal = mesh->HasNormals();
            const bool hasTangent = mesh->HasTangentsAndBitangents();
            const bool hasColor = mesh->HasVertexColors(0);

            for (unsigned int v = 0; v < mesh->mNumVertices; v++)
            {
                VertexSkinned x = {};

                const aiVector3D pv = bake ? (xf * mesh->mVertices[v]) : mesh->mVertices[v];
                x.Pos = { pv.x, pv.y, pv.z };
                if (hasUV)      x.UV = { mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y };
                if (hasNormal)
                {
                    aiVector3D nv = bake ? (nxf * mesh->mNormals[v]) : mesh->mNormals[v];
                    nv.NormalizeSafe();
                    x.Normal = { nv.x, nv.y, nv.z };
                }
                if (hasTangent)
                {
                    aiVector3D tv = bake ? (nxf * mesh->mTangents[v]) : mesh->mTangents[v];
                    tv.NormalizeSafe();
                    x.Tangent = { tv.x, tv.y, tv.z };
                }
                if (hasColor)   x.Color = { mesh->mColors[0][v].r, mesh->mColors[0][v].g, mesh->mColors[0][v].b, mesh->mColors[0][v].a };
                else            x.Color = { 1.0f, 1.0f, 1.0f, 1.0f };

                if (skinned)
                    vk.push_back(x);
                else
                    vs.push_back(x);
            }

            for (unsigned int f = 0; f < mesh->mNumFaces; f++)
            {
                const aiFace& face = mesh->mFaces[f];
                if (face.mNumIndices != 3)
                    continue;
                out.indices.push_back((VIndex)face.mIndices[0]);
                out.indices.push_back((VIndex)face.mIndices[1]);
                out.indices.push_back((VIndex)face.mIndices[2]);
            }

            sub.indexCount = (uint32_t)out.indices.size() - sub.indexStart;
            if (sub.indexCount == 0)
                continue;
            out.subMeshes.push_back(sub);
            if (info != nullptr)
                info->subMeshMaterial.push_back((uint16_t)mesh->mMaterialIndex);

            if (!skinned || !mesh->HasBones())
                continue;

            boneSlot.assign(mesh->mNumVertices, 0);

            for (unsigned int b = 0; b < mesh->mNumBones; b++)
            {
                const aiBone* bone = mesh->mBones[b];
                const std::string name(bone->mName.C_Str());

                uint32_t gi;
                auto it = boneLUT.find(name);
                if (it != boneLUT.end())
                    gi = it->second;
                else
                {
                    gi = (uint32_t)boneLUT.size();
                    if (gi >= SKIN_BONE_MAX)
                    {
                        Internal::PushHubReport(HubReport_Load, L"[Load] 본 수가 SKIN_BONE_MAX 초과");
                        OutputDebugStringW(L"[Load] 본 수가 SKIN_BONE_MAX 초과: "); OutputDebugStringW(filename); OutputDebugStringW(L"\n");
                        return false;
                    }
                    boneLUT.emplace(name, gi);
                }

                for (unsigned int w = 0; w < bone->mNumWeights; w++)
                {
                    const aiVertexWeight& vw = bone->mWeights[w];
                    if (vw.mVertexId >= mesh->mNumVertices)
                        continue;
                    uint8_t& k = boneSlot[vw.mVertexId];
                    if (k >= 4)
                        continue;
                    VertexSkinned& dst = vk[(size_t)sub.baseVertex + vw.mVertexId];
                    dst.BoneIdx[k] = (uint16_t)gi;
                    dst.BoneWeight[k] = vw.mWeight;
                    k++;
                }
            }
        }

        if (out.indices.empty() || out.subMeshes.empty())
            return false;

        if (info != nullptr)
        {
            info->subMeshCount = (uint16_t)out.subMeshes.size();
            info->boneNames.assign(boneLUT.size(), std::string());
            for (auto it = boneLUT.begin(); it != boneLUT.end(); ++it)
                info->boneNames[it->second] = it->first;
        }

        return true;
    }

    static DXGI_FORMAT FallbackFormat(DXGI_FORMAT f)
    {
        switch (FormatDataType(f))
        {
        case FORMAT_TYPE_UNORM: return IsSRGB(f) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
        case FORMAT_TYPE_FLOAT: return BitsPerColor(f) > 16 ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R16G16B16A16_FLOAT;
        default:                return DXGI_FORMAT_UNKNOWN;
        }
    }

    static bool ConvertImage(ScratchImage& scratch, DXGI_FORMAT target)
    {
        ScratchImage converted;
        if (FAILED(Convert(scratch.GetImages(), scratch.GetImageCount(), scratch.GetMetadata(),
                           target, TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, converted)))
            return false;

        scratch = std::move(converted);
        return true;
    }

    bool ReadTextureFile(const wchar_t* filename, TextureFileFormat format, TextureType type, TextureColorSpace colorSpace, Texture& out)
    {
		TexMetadata metadata = {};
		ScratchImage scratch;
		HRESULT hr;

		switch (format)
		{
		default:
		case TextureFileFormat_None:
			hr = LoadFromWICFile(filename, colorSpace == TextureColorSpace_Linear ? WIC_FLAGS_IGNORE_SRGB : WIC_FLAGS_NONE, &metadata, scratch);
			break;
		
		case TextureFileFormat_DDS:
			hr = LoadFromDDSFile(filename, DDS_FLAGS_NONE, &metadata, scratch);
			break;
		}

		if (FAILED(hr))
        {
            Internal::PushHubReport(HubReport_Load, L"[Load] 텍스처 파일 로드 실패");
            OutputDebugStringW(L"[Load] 텍스처 파일 로드 실패: ");
            OutputDebugStringW(filename);
            OutputDebugStringW(L"\n");
            return false;
        }


        TextureData texData;

        DXGI_FORMAT fmt = metadata.format;

        if (FromDXGI(fmt) == TextureReadFormat::Unknown && !IsCompressed(fmt))
        {
            const DXGI_FORMAT target = FallbackFormat(fmt);
            if (target != DXGI_FORMAT_UNKNOWN && ConvertImage(scratch, target))
                fmt = target;
        }

        if (colorSpace == TextureColorSpace_Linear)
        {
            if (IsSRGB(fmt))
                fmt = MakeLinear(fmt);
        }
        else if (colorSpace == TextureColorSpace_SRGB && !IsSRGB(fmt))
        {
            if (MakeSRGB(fmt) != fmt)
                fmt = MakeSRGB(fmt);
            else if (FormatDataType(fmt) == FORMAT_TYPE_UNORM && !IsCompressed(fmt) && ConvertImage(scratch, DXGI_FORMAT_R8G8B8A8_UNORM))
                fmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            else if (FormatDataType(fmt) != FORMAT_TYPE_FLOAT)
                Internal::PushHubReport(HubReport_Load, L"[Load] sRGB 로 받을 수 없는 포맷 (선형으로 유지)");
        }

        scratch.OverrideFormat(fmt);
        metadata = scratch.GetMetadata();

        texData.format = FromDXGI(metadata.format);

        if (texData.format == TextureReadFormat::Unknown)
        {
            Internal::PushHubReport(HubReport_Load, L"[Load] 지원하지 않는 텍스처 포맷");
            OutputDebugStringW(L"[Load] 지원하지 않는 텍스처 포맷: ");
            OutputDebugStringW(filename);
            OutputDebugStringW(L"\n");
            return false;
        }
        
		texData.width = metadata.width;
		texData.height = metadata.height;
		texData.depth = metadata.depth;

		texData.mipLevels = metadata.mipLevels;
		texData.arraySize = metadata.arraySize;
        
        switch (type)
        {
        default:
        case TextureType_2D:
            if (metadata.dimension != TEX_DIMENSION_TEXTURE2D)
                Internal::PushHubReport(HubReport_Load, L"[Load] 2D 타입인데 파일이 2D 가 아님 (그대로 진행)");
            break;

        case TextureType_Cube:
            if (metadata.dimension != TEX_DIMENSION_TEXTURE2D || !metadata.IsCubemap())
                Internal::PushHubReport(HubReport_Load, L"[Load] Cube 타입인데 파일이 큐브맵이 아님 (그대로 진행, 6의 배수 장수가 아니면 렌더러가 거부)");
            break;

        case TextureType_1D:
            if (metadata.dimension != TEX_DIMENSION_TEXTURE1D && metadata.height != 1)
                Internal::PushHubReport(HubReport_Load, L"[Load] 1D 타입인데 파일 높이가 1 이 아님 (그대로 진행, 렌더러가 거부)");
            break;

        case TextureType_3D:
            if (metadata.dimension != TEX_DIMENSION_TEXTURE3D)
                Internal::PushHubReport(HubReport_Load, L"[Load] 3D 타입인데 파일이 3D 가 아님 (그대로 진행)");
            break;
        }

        texData.textureType = type;

        out.data = texData;


        const uint8_t* base = scratch.GetPixels();
        const size_t size = scratch.GetPixelsSize();
        out.rawData.resize(size);
        memcpy(out.rawData.data(), base, size);
        

		const UINT numSubresources = metadata.arraySize * metadata.mipLevels;

        out.subresources.resize(numSubresources);

        size_t i = 0;
		for (size_t arr = 0; arr < metadata.arraySize; arr++)
		{
			for (size_t mip = 0; mip < metadata.mipLevels; mip++)
			{
				const DirectX::Image* img = scratch.GetImage(mip, arr, 0);
                if (!img)
                {
                    Internal::PushHubReport(HubReport_Load, L"[Load] 텍스처 서브리소스 읽기 실패");
                    return false;
                }

				out.subresources[i].offset = img->pixels - base;
				out.subresources[i].rowPitch = img->rowPitch;
				out.subresources[i].slicePitch = img->slicePitch;
				
				i++;
			}
		}


        out.usage = TextureUsage_ReadOnly;


        return true;
    }


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

    TexID LoadTexture(const wchar_t* path, TextureFileFormat format, TextureType type, TextureColorSpace colorSpace)
    {
        const TexID existing = GetTexId(path);
        if (existing != 0)
            return existing;

        Texture tex = {};
        if (!ReadTextureFile(path, format, type, colorSpace, tex))
            return 0;

        return CreateTexture(path, std::move(tex));
    }

    #pragma endregion
    

}