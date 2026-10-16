cmake_minimum_required(VERSION 3.15)
project(boosterik LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Статичний CRT: exe запускається без Visual C++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_executable(boosterik WIN32 src/main.cpp res/boosterik.rc)

target_compile_definitions(boosterik PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)

if(MSVC)
    target_compile_options(boosterik PRIVATE /utf-8 /O2 /W3 /EHsc)
    # маніфест (адмін-права + DPI) вшитий через res/boosterik.rc
    target_link_options(boosterik PRIVATE /MANIFEST:NO)
endif()

target_link_libraries(boosterik PRIVATE
    gdiplus powrprof psapi shell32 ole32 user32 gdi32 advapi32)
