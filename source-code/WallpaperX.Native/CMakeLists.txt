cmake_minimum_required(VERSION 3.20)
project(WallpaperXNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WALLPAPERX_COMMON_LIBS
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

set(WALLPAPERX_ICON_RC
    resources/app_icon.rc
)

add_executable(WallpaperXEditor WIN32
    src/main.cpp
    ${WALLPAPERX_ICON_RC}
)

set_target_properties(WallpaperXEditor PROPERTIES OUTPUT_NAME "WallpaperXEditor")
target_compile_definitions(WallpaperXEditor PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(WallpaperXEditor PRIVATE ${WALLPAPERX_COMMON_LIBS})

add_executable(WallpaperXStartup WIN32
    src/startup.cpp
    ${WALLPAPERX_ICON_RC}
)

set_target_properties(WallpaperXStartup PROPERTIES OUTPUT_NAME "WallpaperXStartup")
target_compile_definitions(WallpaperXStartup PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(WallpaperXStartup PRIVATE shell32 user32)

add_executable(WallpaperXUninstaller WIN32
    src/uninstaller.cpp
    ${WALLPAPERX_ICON_RC}
)

set_target_properties(WallpaperXUninstaller PROPERTIES OUTPUT_NAME "WallpaperXUninstaller")
target_compile_definitions(WallpaperXUninstaller PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(WallpaperXUninstaller PRIVATE advapi32 shell32 user32)

add_executable(WallpaperXInstaller WIN32
    src/installer.cpp
    ${WALLPAPERX_ICON_RC}
)

set_target_properties(WallpaperXInstaller PROPERTIES OUTPUT_NAME "WallpaperXInstaller")
target_compile_definitions(WallpaperXInstaller PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(WallpaperXInstaller PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(WallpaperXInstaller PRIVATE advapi32 comctl32 ole32 shell32 user32)
