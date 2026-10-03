cmake_minimum_required(VERSION 3.20)
project(tailspin CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_options(/W3 /utf-8 /fp:fast /D_CRT_SECURE_NO_WARNINGS)
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# --- game simulation: pure C++, no SDL, shared by the app and the headless tool ---
add_library(tailspin_game STATIC game/game.cpp)
target_include_directories(tailspin_game PUBLIC ${CMAKE_SOURCE_DIR})

# --- headless sim / self-test (console) ---
add_executable(tailspin_sim tools/sim.cpp)
target_link_libraries(tailspin_sim PRIVATE tailspin_game)

# --- the app: SDL3 platform layer + engine ---
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.16
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL3)

# --- shared engine (renderer + audio) ---
add_library(engine STATIC engine/gfx.cpp engine/audio.cpp engine/pix.cpp)
target_include_directories(engine PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(engine PUBLIC SDL3::SDL3)

add_executable(tailspin WIN32 main.cpp game/render.cpp)
target_link_libraries(tailspin PRIVATE tailspin_game engine)

# --- HOLDLINE: merge-and-defend (enabled once its sources exist) ---
if(EXISTS ${CMAKE_SOURCE_DIR}/game2/mg_game.cpp)
  add_library(holdline_game STATIC game2/mg_game.cpp)
  target_include_directories(holdline_game PUBLIC ${CMAKE_SOURCE_DIR})
  add_executable(holdline_sim tools/mg_sim.cpp)
  target_link_libraries(holdline_sim PRIVATE holdline_game)
  add_executable(holdline WIN32 main_mg.cpp game2/mg_render.cpp)
  target_link_libraries(holdline PRIVATE holdline_game engine)
endif()
