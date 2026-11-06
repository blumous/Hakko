cmake_minimum_required(VERSION 3.24)

# 형제 디렉터리 (샘플 / 엔진) 에서도 의존성 타깃이 보이게
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL ON)

find_package(directx-headers CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(directx-dxc CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)


add_library(Hakko STATIC
        Data/private/DataHub.cpp
        Data/private/DataHub_Interface.cpp
        Data/private/DataHub_FileLoader.cpp
        Data/private/DataHub_Resource.cpp
        Core/private/D3D12/D3D12Core.cpp
        Render/private/RenderFactory.cpp
        Render/private/D3D12/D3D12Renderer_BasicImpl.cpp
        Render/private/D3D12/D3D12Renderer_FrameNPass.cpp
        Render/private/D3D12/D3D12Renderer_ResCtrl.cpp
        Render/private/D3D12/D3D12Renderer_MsgProcess.cpp
        Render/private/D3D12/D3D12Renderer_Allocator.cpp
        )

target_compile_features(Hakko PUBLIC cxx_std_20)

target_include_directories(Hakko
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/Data/public
            ${CMAKE_CURRENT_SOURCE_DIR}/Render/public
            ${CMAKE_CURRENT_SOURCE_DIR}/Service/public
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Render/private/D3D12
            ${CMAKE_CURRENT_SOURCE_DIR}/Core/private/D3D12
        )

target_link_libraries(Hakko
        PRIVATE
            d3d12.lib dxgi.lib dxguid.lib
            Microsoft::DirectX-Headers Microsoft::DirectXTex Microsoft::DirectXShaderCompiler
            assimp::assimp
        )

if(MSVC)
    target_compile_options(Hakko PUBLIC /utf-8 /permissive-)
endif()


# 실행 파일 옆에 렌더러 런타임 배치: 디폴트 셰이더 (Render/D3D12/shader) + dxil.dll
# dxil.dll 은 dxcompiler.dll 이 LoadLibrary 로 여는 거라 applocal (import 테이블 스캔) 이 못 잡음
# 디폴트 셰이더를 바이트코드로 올리기 전까지 쓰는 임시 배치
function(hakko_deploy target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Render/private/D3D12/shader"
            "$<TARGET_FILE_DIR:${target}>/Render/D3D12/shader"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE_DIR:Microsoft::DirectXShaderCompiler>/dxil.dll"
            "$<TARGET_FILE_DIR:${target}>/dxil.dll"
    )
endfunction()
