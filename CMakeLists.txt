cmake_minimum_required(VERSION 3.20)
project(EldenFree LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.5 GIT_SHALLOW TRUE)
FetchContent_Declare(minhook GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git GIT_TAG b4289327718fb9dacf3294a3739db86527e51913)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()
FetchContent_GetProperties(minhook)
if(NOT minhook_POPULATED)
  FetchContent_Populate(minhook)
endif()

add_library(EldenFree SHARED
  src/dllmain.cpp
  src/mem.cpp
  src/config.cpp
  src/game.cpp
  src/features.cpp
  src/overlay.cpp
  src/ui.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
  ${minhook_SOURCE_DIR}/src/buffer.c
  ${minhook_SOURCE_DIR}/src/hook.c
  ${minhook_SOURCE_DIR}/src/trampoline.c
  ${minhook_SOURCE_DIR}/src/hde/hde64.c
)

target_include_directories(EldenFree PRIVATE src ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${minhook_SOURCE_DIR}/include)
target_compile_definitions(EldenFree PRIVATE WIDL_EXPLICIT_AGGREGATE_RETURNS WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE DIRECTINPUT_VERSION=0x0800 IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
target_compile_options(EldenFree PRIVATE -O2 -Wno-unknown-pragmas)
target_link_options(EldenFree PRIVATE -static -static-libgcc -static-libstdc++ -s)
target_link_libraries(EldenFree PRIVATE d3d12 dxgi d3dcompiler_47 dinput8 dxguid dwmapi user32 kernel32)
set_target_properties(EldenFree PROPERTIES PREFIX "" OUTPUT_NAME "EldenFree")
set_source_files_properties(${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp PROPERTIES COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/src/d3d12_compat.h")

set_target_properties(EldenFree PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR} LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
file(WRITE ${CMAKE_BINARY_DIR}/payload.rc "1 RCDATA \"${CMAKE_BINARY_DIR}/EldenFree.dll\"\n")
add_executable(EldenFreeLauncher launcher/main.cpp ${CMAKE_BINARY_DIR}/payload.rc)
add_dependencies(EldenFreeLauncher EldenFree)
set_source_files_properties(${CMAKE_BINARY_DIR}/payload.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/EldenFree.dll)
target_compile_definitions(EldenFreeLauncher PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(EldenFreeLauncher PRIVATE -O2 -municode)
target_link_options(EldenFreeLauncher PRIVATE -static -static-libgcc -static-libstdc++ -municode -s)
target_link_libraries(EldenFreeLauncher PRIVATE comdlg32 shell32 advapi32 psapi)
set_target_properties(EldenFreeLauncher PROPERTIES OUTPUT_NAME "EldenFree" RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/launcher)
