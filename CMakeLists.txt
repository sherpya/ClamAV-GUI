cmake_minimum_required(VERSION 3.15)
project(ClamAV-GUI VERSION "1.3.0")

option(UNICODE "Enable Unicode Build" ON)
option(FREESTANDING "Experimental no-crt Build" OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB headers ${CMAKE_SOURCE_DIR}/*.h)
file(GLOB sources
    ${CMAKE_SOURCE_DIR}/clamav-gui.c
    ${CMAKE_SOURCE_DIR}/pipes.c
)

add_executable(ClamAV-GUI WIN32
    ${headers} ${sources}
    ${CMAKE_SOURCE_DIR}/resource.h
    ${CMAKE_SOURCE_DIR}/clamav-gui.rc
)

target_link_options(ClamAV-GUI PRIVATE $<$<CXX_COMPILER_ID:GNU>:-static>)
target_compile_options(ClamAV-GUI PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror -nostdlib>
)

if(UNICODE)
    target_compile_definitions(ClamAV-GUI PRIVATE UNICODE _UNICODE)
endif()

if(FREESTANDING)
    target_sources(ClamAV-GUI PRIVATE ${CMAKE_SOURCE_DIR}/nocrt.c)
    target_link_options(ClamAV-GUI PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-fno-builtin -Wl,--gc-sections -nostdlib -mwindows -fno-asynchronous-unwind-tables>
    )
    target_link_libraries(ClamAV-GUI PRIVATE kernel32 user32 shell32)
endif()

target_compile_definitions(ClamAV-GUI PRIVATE _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=_WIN32_WINNT_WINXP)
target_include_directories(ClamAV-GUI PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ClamAV-GUI PRIVATE comctl32)
set_property(TARGET ClamAV-GUI PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ClamAV-GUI)
