#pragma once

#include "DataHub.h"

// Assimp / DirectXTex 기반 파일로더입니다
// 선택 기능 헤더


namespace Hakko::DataHub
{

    #pragma region FileLoader

    struct MeshFileMaterial
    {
        std::wstring diffusePath;
        std::wstring normalPath;
        std::wstring ormPath;
        std::wstring emissionPath;
        float baseColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    struct MeshFileInfo
    {
        uint16_t subMeshCount = 0;
        std::vector<uint16_t> subMeshMaterial;
        std::vector<MeshFileMaterial> materials;
        std::vector<std::string> boneNames;
    };


    bool ReadMeshFile(const wchar_t* path, Mesh& out, MeshFileInfo* info = nullptr);

    bool ReadTextureFile(const wchar_t* path, TextureFileFormat format, TextureType type, TextureColorSpace colorSpace, Texture& out);


    // 파일을 읽어서 허브 리소스로 바로 등록 (리소스 이름 = 경로, 이미 올라간 경로면 그 id 를 그대로 돌려줌)
    MeshID LoadMesh(const wchar_t* path, MeshFileInfo* info = nullptr);

    TexID LoadTexture(const wchar_t* path, TextureFileFormat format, TextureType type, TextureColorSpace colorSpace = TextureColorSpace_Auto);

    #pragma endregion

}
