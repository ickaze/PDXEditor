cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)
project(PDXEditor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(PDXEditor WIN32
  src/main.cpp src/App.cpp src/Audio.cpp src/NdsifAudio.cpp src/Pdx.cpp src/Settings.cpp)
target_compile_definitions(PDXEditor PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(PDXEditor PRIVATE comctl32 winmm shell32 shlwapi ole32 uuid comdlg32 msimg32)

if (MSVC)
  set_property(TARGET PDXEditor PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(PDXEditor PRIVATE /utf-8)
  target_link_options(PDXEditor PRIVATE "/PDBALTPATH:%_PDB%" $<$<CONFIG:Release>:/DEBUG:NONE>)
endif()

add_custom_command(TARGET PDXEditor POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PDXEditor>/languages"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/languages/ja.lang"
    "${CMAKE_CURRENT_SOURCE_DIR}/languages/en.lang"
    "$<TARGET_FILE_DIR:PDXEditor>/languages"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/VALIDATION.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/RELEASE_NOTES.md"
    "$<TARGET_FILE_DIR:PDXEditor>"
  VERBATIM)
