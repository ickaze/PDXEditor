cmake_minimum_required(VERSION 3.20)
project(PDXEditor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(PDXEditor WIN32
  src/main.cpp src/App.cpp src/Audio.cpp src/Pdx.cpp src/Settings.cpp)
target_compile_definitions(PDXEditor PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(PDXEditor PRIVATE comctl32 winmm shell32 shlwapi ole32 uuid comdlg32)

if (MSVC)
  target_compile_options(PDXEditor PRIVATE /utf-8)
endif()
