cmake_minimum_required(VERSION 3.20)

# ---- Project ----
project(chip8 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # for clangd / Neovim LSP

# ---- Sources ----
add_executable(chip8
  main.cpp
  # add other .cpp files here explicitly
)
# find_package(SDL2 REQUIRED)

# ---- Include directories ----
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
target_include_directories(chip8 PRIVATE
      "${CMAKE_SOURCE_DIR}/include"
      "C:/libs/SDL2/include"
)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
target_include_directories(chip8 PRIVATE
      "${CMAKE_SOURCE_DIR}/include"
      "/usr/include/SDL2"
)
endif()


# ---- Compiler options ----
if(MSVC)
  target_compile_options(chip8 PRIVATE /W4 /permissive- /Zc:__cplusplus)
  target_compile_definitions(chip8 PRIVATE _CRT_SECURE_NO_WARNINGS -D_REENTRANT)
else()
  target_compile_options(chip8 PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_definitions(chip8 PRIVATE -D_REENTRANT)
endif()

# ---- Link SDL2 ----
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
target_link_libraries(chip8 PRIVATE
      "C:/libs/SDL2/lib/x64/SDL2main.lib"
      "C:/libs/SDL2/lib/x64/SDL2.lib"
)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
target_link_libraries(chip8 PRIVATE
      SDL2::SDL2
)
endif()

# Copy SDL2.dll so the EXE runs from build folder
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_custom_command(TARGET chip8 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "C:/libs/SDL2/lib/x64/SDL2.dll"
    "$<TARGET_FILE_DIR:chip8>/SDL2.dll"
    )
endif()

# ---- Export compile_commands.json for clangd ----
add_custom_target(copy-compile-commands ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json"
  DEPENDS chip8
)

# ---- Clean-all target (removes the entire build dir) ----
add_custom_target(clean_all
  COMMAND ${CMAKE_COMMAND} -E echo "Removing all build artifacts..."
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "✅ Build directory cleaned."
)

# ---- Rebuild target (clean + configure + build) ----
# This uses CMake's own executable to call itself cleanly
add_custom_target(rebuild
  COMMAND ${CMAKE_COMMAND} -E echo "🧹 Cleaning old build..."
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "⚙️  Reconfiguring and building..."
  COMMAND ${CMAKE_COMMAND} -S "${CMAKE_SOURCE_DIR}" -B "${CMAKE_BINARY_DIR}" -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
  COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Full clean + rebuild"
)

