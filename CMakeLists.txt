cmake_minimum_required(VERSION 3.20)

project(CentralDeAudio VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Este projeto usa ASIO + Win32 + DirectX 11 e so compila no Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty")
set(IMGUI_DIR "${THIRD_PARTY}/imgui")
set(ASIO_DIR "${THIRD_PARTY}/asio_sdk")

add_executable(CentralDeAudio WIN32
    src/main.cpp
    src/app/App.cpp
    src/audio/AppAudioCapture.cpp
    src/audio/AsioDevice.cpp
    src/audio/WasapiEngine.cpp
    src/dsp/Fx.cpp
    src/midi/MidiEngine.cpp
    src/mixer/Mixer.cpp
    src/platform/AppWindow.cpp
    src/ui/Widgets.cpp

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp

    ${ASIO_DIR}/host/pc/asiolist.cpp
)

target_include_directories(CentralDeAudio PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${ASIO_DIR}/common"
    "${ASIO_DIR}/host"
    "${ASIO_DIR}/host/pc"
)

target_link_libraries(CentralDeAudio PRIVATE
    d3d11
    d3dcompiler
    dxgi
    dwmapi
    ole32
    uuid
    comctl32
    winmm
    mmdevapi
    avrt
    propsys
    ksguid
)

set_target_properties(CentralDeAudio PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    OUTPUT_NAME "CentralDeAudio"
)

# Manuais HTML ficam sempre disponiveis ao lado do executavel (bin/docs).
add_custom_command(TARGET CentralDeAudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/docs"
            "${CMAKE_CURRENT_SOURCE_DIR}/bin/docs"
    COMMENT "Copiando manuais para bin/docs"
)

if(MSVC)
    target_compile_options(CentralDeAudio PRIVATE /W3 /permissive-)
else()
    target_compile_options(CentralDeAudio PRIVATE -Wall -Wextra)
    # O executavel deve rodar sem DLLs do MinGW ao lado dele (uso portatil).
    target_link_options(CentralDeAudio PRIVATE -static -static-libgcc -static-libstdc++)
endif()
