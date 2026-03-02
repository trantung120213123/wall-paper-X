cmake_minimum_required(VERSION 3.20)
project(WallpaperXNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(WallpaperXNative WIN32
    src/main.cpp
)

target_compile_definitions(WallpaperXNative PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

target_link_libraries(WallpaperXNative PRIVATE
    advapi32
    comctl32
    comdlg32
    mf
    mfplat
    mfplay
    mfuuid
    ole32
    shell32
    shlwapi
    user32
)
