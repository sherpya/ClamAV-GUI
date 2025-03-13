cmake_minimum_required(VERSION 3.14)
project(ClamAV-GUI VERSION "1.3.0")

option(UNICODE "Enable Unicode Build" ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB sources ${CMAKE_SOURCE_DIR}/*.c)
file(GLOB headers ${CMAKE_SOURCE_DIR}/*.h)

add_executable(ClamAV-GUI WIN32
    ${headers} ${sources}
    ${CMAKE_SOURCE_DIR}/resource.h
    ${CMAKE_SOURCE_DIR}/clamav-gui.rc
)

target_link_options(ClamAV-GUI PRIVATE $<$<CXX_COMPILER_ID:GNU>:-static>)
target_compile_options(ClamAV-GUI PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

if(UNICODE)
    target_compile_definitions(ClamAV-GUI PRIVATE UNICODE _UNICODE)
endif()

target_compile_definitions(ClamAV-GUI PRIVATE _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=_WIN32_WINNT_WINXP)
target_include_directories(ClamAV-GUI PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ClamAV-GUI PRIVATE comctl32)
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ClamAV-GUI)
