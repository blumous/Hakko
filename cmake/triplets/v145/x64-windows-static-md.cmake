# vcpkg 기본 x64-windows-static-md 와 같고, 패키지를 빌드할 툴셋만 VS 2026 (v145) 로 고정
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_PLATFORM_TOOLSET v145)
