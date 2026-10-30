cmake_minimum_required(VERSION 3.20)
project(pixel_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_options(/W3 /utf-8 /fp:fast /D_CRT_SECURE_NO_WARNINGS)
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

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
add_library(engine STATIC engine/audio.cpp engine/pix.cpp)
target_include_directories(engine PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(engine PUBLIC SDL3::SDL3)

# --- EMBERVALE: procedural open-world pixel RPG ---
# M3: the culture engine (rpg/culture) is its own library: world generation (rpg_sim) and the painters (rpg_art) both
# read it. Its choices are structural (which culture lives where), so it builds like rpg/world: no fast-math, no FMA.
# M3b: the builder (rpg/build: the construction grammar and its parts) lives here too: the settlement generator, the
# interiors and the painters all read its blueprints, and its choices are structural (deterministic, integer maths).
file(GLOB RPG_CULTURE CONFIGURE_DEPENDS rpg/culture/*.cpp rpg/build/*.cpp)
add_library(rpg_culture STATIC ${RPG_CULTURE})
target_include_directories(rpg_culture PUBLIC ${CMAKE_SOURCE_DIR})   # headers only from engine/ (music_style.h): no SDL link
if(MSVC)
  set_source_files_properties(${RPG_CULTURE} PROPERTIES COMPILE_OPTIONS "/fp:precise")
else()
  set_source_files_properties(${RPG_CULTURE} PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/rpg/art/art_core.cpp)
  # P2a Clear Sight: the v2 rig's painters (rpg/art/rig) build into the art library. P2b New People: the DOLL
  # (rpg/art/doll: the pool, compositor, resolver, ramps; gen/doll_pool.cpp is tools/doll/pxc.mjs's generated pool, one
  # chunked translation unit) draws rig 2; rpg/art/rig stays compiled, detached (legacy*), for tools/preview/rigdraft.cpp
  # and art_fx.cpp's arc until the P2c flip deletes it
  # The doll's DATA (the pool: doll_pool.cpp + the generated gen/doll_pool.cpp; the body sets and walk tables:
  # doll_walk.cpp) is its own library, rpg_doll, below both rpg_sim and rpg_art: the simulation reads the walk and run
  # tables (rpg/sim/anim.cpp: the gait, the walk clock), so a target that links rpg_sim without the painters (save_test)
  # still links. No painter, no SDL: headers only from engine/ (pix.h, color.h), like rpg_culture. The alloy families
  # (doll_alloy.cpp) live here too: the gear ladder (rpg/sim/gear_ladder.cpp) picks each people's alloys from them.
  set(RPG_DOLL_DATA ${CMAKE_SOURCE_DIR}/rpg/art/doll/doll_pool.cpp ${CMAKE_SOURCE_DIR}/rpg/art/doll/doll_walk.cpp
                    ${CMAKE_SOURCE_DIR}/rpg/art/doll/gen/doll_pool.cpp
                    ${CMAKE_SOURCE_DIR}/rpg/art/doll/doll_alloy.cpp)
  add_library(rpg_doll STATIC ${RPG_DOLL_DATA})
  target_include_directories(rpg_doll PUBLIC ${CMAKE_SOURCE_DIR})
  file(GLOB RPG_ART CONFIGURE_DEPENDS rpg/art/*.cpp rpg/art/rig/*.cpp rpg/art/doll/*.cpp)
  list(REMOVE_ITEM RPG_ART ${RPG_DOLL_DATA})
  add_library(rpg_art STATIC ${RPG_ART})
  target_link_libraries(rpg_art PUBLIC engine rpg_culture rpg_doll)
  if(EXISTS ${CMAKE_SOURCE_DIR}/tools/art_preview.cpp)
    add_executable(art_preview tools/art_preview.cpp)
    target_link_libraries(art_preview PRIVATE rpg_art)
  endif()
  add_executable(art_hash tools/art_hash.cpp)
  target_link_libraries(art_hash PRIVATE rpg_art)
  # per-lane art galleries: every tools/preview/<name>.cpp is an executable <name> (helpers: tools/preview/preview_util.h)
  file(GLOB PREVIEW_SRCS CONFIGURE_DEPENDS tools/preview/*.cpp)
  # (P2b) the rejected procedural rig's galleries give way to doll_gallery (their sources go with the P2c flip)
  list(FILTER PREVIEW_SRCS EXCLUDE REGEX "/(rig_gallery|rigarms_gallery|rigposes_gallery)\.cpp$")
  foreach(src ${PREVIEW_SRCS})
    get_filename_component(pname ${src} NAME_WE)
    add_executable(${pname} ${src})
    target_link_libraries(${pname} PRIVATE rpg_art rpg_sim)
  endforeach()
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/tools/audio_preview.cpp)
  add_executable(audio_preview tools/audio_preview.cpp)
  target_link_libraries(audio_preview PRIVATE engine)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/rpg/sim/game.cpp)
  file(GLOB RPG_SIM CONFIGURE_DEPENDS rpg/sim/*.cpp)
  # rpg/world: the endless generator (M1). Structural generation must give the same bits on MSVC, clang (iOS) and
  # Emscripten (VISION_PLAN 2.4): no fast-math, no FMA contraction in these files.
  file(GLOB RPG_WORLD CONFIGURE_DEPENDS rpg/world/*.cpp)
  if(MSVC)
    set_source_files_properties(${RPG_WORLD} PROPERTIES COMPILE_OPTIONS "/fp:precise")
  else()
    set_source_files_properties(${RPG_WORLD} PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
  endif()
  # M8b: the loot rolls (rarity odds, item level, affix values, power) feed a golden the clang CI build must match
  # (rpg_test --loot): gear.cpp and items.cpp build like rpg/world, no fast-math and no FMA, on every compiler
  set(RPG_LOOT_FP ${CMAKE_SOURCE_DIR}/rpg/sim/gear.cpp ${CMAKE_SOURCE_DIR}/rpg/sim/items.cpp)
  if(MSVC)
    set_source_files_properties(${RPG_LOOT_FP} PROPERTIES COMPILE_OPTIONS "/fp:precise")
  else()
    set_source_files_properties(${RPG_LOOT_FP} PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
  endif()
  # M4: the quest and campaign engine (rpg/story: the DSL, the caster, the campaigns, rumours and lore) builds with the
  # simulation (it reads Game, the realm and the world; ordinary floating point like rpg/sim)
  # M6b: the saga generator's archetype and campaign tables live one folder down (rpg/story/arch, rpg/story/campaigns)
  file(GLOB RPG_STORY CONFIGURE_DEPENDS rpg/story/*.cpp rpg/story/*/*.cpp)
  add_library(rpg_sim STATIC ${RPG_SIM} ${RPG_WORLD} ${RPG_STORY})
  target_include_directories(rpg_sim PUBLIC ${CMAKE_SOURCE_DIR})
  target_link_libraries(rpg_sim PUBLIC rpg_culture)
  # (P2b) the doll's walk and run tables (rpg/sim/anim.cpp) come from its data library (rpg_doll, above)
  if(TARGET rpg_doll)
    target_link_libraries(rpg_sim PUBLIC rpg_doll)
  endif()
  file(GLOB RPG_TESTS CONFIGURE_DEPENDS tools/tests/*.cpp)
  add_executable(rpg_test ${RPG_TESTS})
  target_link_libraries(rpg_test PRIVATE rpg_sim)
  target_compile_definitions(rpg_test PRIVATE "EMB_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")
  # P2a: the painters are linked in (the rig, animation and coverage checks, the hero and arms checks), so the test files
  # no longer compile painter sources into themselves (EMB_RPG_TEST_HAS_ART retires the #include hack)
  if(TARGET rpg_art)
    target_link_libraries(rpg_test PRIVATE rpg_art)
    target_compile_definitions(rpg_test PRIVATE EMB_RPG_TEST_HAS_ART=1)
  endif()
  if(EXISTS ${CMAKE_SOURCE_DIR}/tools/save_test.cpp)
    add_executable(save_test tools/save_test.cpp)
    target_link_libraries(save_test PRIVATE rpg_sim)
    target_compile_definitions(save_test PRIVATE "EMB_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")
  endif()
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/rpg/main.cpp AND TARGET rpg_art)
  file(GLOB RPG_VIEW CONFIGURE_DEPENDS rpg/view/*.cpp)
  add_executable(embervale WIN32 rpg/main.cpp ${RPG_VIEW})
  target_link_libraries(embervale PRIVATE rpg_sim rpg_art engine)
  # Witness (engine/witness.h): a test harness drives the game headless over a local socket when it is started with
  # WITNESS_ADDR set or --witness (rpg/main.cpp). On by default on the desktop.
  # The web build has no sockets: EMBERVALE_WITNESS_WEB (default OFF, so the deployed web build is unchanged) builds the
  # Witness web bridge instead (adapters/cpp/GUIDE.md section 13): witness.h talks through engine/witness_emscripten.js
  # (linked with --pre-js), which holds a WebSocket to the CLI and is active only when the page URL has ?witness=host:port.
  # A test variant (.github/workflows/web-witness.yml, tools/witness_web/README.md), never deployed.
  if(EMSCRIPTEN)
    option(EMBERVALE_WITNESS_WEB "Witness web bridge in the web build (a test variant; never deployed)" OFF)
    set(EMB_WITNESS_ON ${EMBERVALE_WITNESS_WEB})
  else()
    option(EMBERVALE_WITNESS "Witness test harness in the game (desktop)" ON)
    set(EMB_WITNESS_ON ${EMBERVALE_WITNESS})
  endif()
  if(EMB_WITNESS_ON)
    target_sources(embervale PRIVATE engine/witness_impl.cpp)
    target_compile_definitions(embervale PRIVATE EMB_WITNESS=1)
    # the adapter's sockets: MSVC (and clang-cl) link ws2_32 through the header's #pragma comment; MinGW needs it named
    if(WIN32 AND NOT MSVC)
      target_link_libraries(embervale PRIVATE ws2_32)
    endif()
    if(EMSCRIPTEN)
      # the browser half of the bridge, part of embervale.js (it does nothing without ?witness= in the page URL)
      target_link_options(embervale PRIVATE "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/engine/witness_emscripten.js")
      set_property(TARGET embervale APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/engine/witness_emscripten.js)
    endif()
  endif()
  if(EMSCRIPTEN)
    # web build: embervale.js + embervale.wasm, loaded by web/index.html (GitHub Pages, add-to-Home-Screen)
    set_target_properties(embervale PROPERTIES SUFFIX ".js")
    target_link_options(embervale PRIVATE
      -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=4194304
      # IDBFS must be listed here, not only linked: from Emscripten 6.0.13 a -l library symbol nothing in the build
      # references is dead-code eliminated, so the page's preRun hit "IDBFS is not defined" and never started (the web
      # build stuck on "forging the world..." from 2026-10-20). Exporting it keeps it and gives web/index.html Module.IDBFS.
      -lidbfs.js "-sEXPORTED_RUNTIME_METHODS=['FS','IDBFS','addRunDependency','removeRunDependency']")
  endif()
endif()
