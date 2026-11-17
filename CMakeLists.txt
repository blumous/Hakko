cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
endif()


# vcpkg 패키지를 제너레이터와 같은 VS 툴셋으로 빌드하게 고정 (트리플렛 덮어쓰기)
# vcpkg 는 설치된 VS 중 최신 걸 고르기 때문에, VS 가 두 버전 깔린 PC 에서 옛 VS 로 프로젝트를 만들면
# 새 컴파일러로 빌드된 정적 라이브러리를 옛 링커가 못 받음 (LNK2019 __std_* 확인할 수 없는 외부 기호)
if(NOT VCPKG_OVERLAY_TRIPLETS)
    if(CMAKE_GENERATOR MATCHES "Visual Studio 17")
        set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/triplets/v143" CACHE STRING "vcpkg overlay triplets" FORCE)
    elseif(CMAKE_GENERATOR MATCHES "Visual Studio 18")
        set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/triplets/v145" CACHE STRING "vcpkg overlay triplets" FORCE)
    endif()
endif()


option(HAKKO_USE_IMGUI "ImGui module: the renderer draws ImGui draw data (RENDER_IMGUI_MODE)" OFF)
if(HAKKO_USE_IMGUI)
    list(APPEND VCPKG_MANIFEST_FEATURES "imgui")
endif()


project(Hakko VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HAKKO_BUILD_SAMPLES "Build the sample applications" ON)


add_subdirectory(Hakko)

if(HAKKO_BUILD_SAMPLES)
    add_subdirectory(Samples/Basic)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT HakkoSample)
endif()
