cmake_minimum_required(VERSION 3.20)
project(simcity2000_port LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(sc2k_save src/save/rle.cpp src/save/city.cpp)
target_include_directories(sc2k_save PUBLIC src)
target_compile_options(sc2k_save PRIVATE -Wall -Wextra -Wpedantic)

add_library(sc2k_sim src/simulation/rng.cpp src/simulation/clock.cpp src/simulation/power.cpp src/simulation/water.cpp src/simulation/environment.cpp src/simulation/tile_ops.cpp src/simulation/destroy.cpp src/simulation/traffic.cpp src/simulation/zone_pass.cpp src/simulation/special.cpp src/simulation/tools.cpp src/simulation/highway.cpp src/simulation/availability.cpp src/simulation/bonds.cpp src/simulation/advisors.cpp src/simulation/terrain_gen.cpp src/simulation/news.cpp src/simulation/new_city.cpp src/simulation/query.cpp src/simulation/things.cpp src/simulation/disaster.cpp src/simulation/scenario.cpp src/simulation/economy.cpp src/simulation/tick.cpp src/simulation/yearly.cpp src/simulation/simulation.cpp src/simulation/city_state.cpp src/simulation/finance.cpp src/simulation/demand.cpp src/simulation/tables.cpp src/resources/pe_image.cpp src/resources/pe_resources.cpp src/world/world.cpp)
target_link_libraries(sc2k_sim PUBLIC sc2k_save)
target_compile_options(sc2k_sim PRIVATE -ffp-contract=off)
target_include_directories(sc2k_sim PUBLIC src)
target_compile_options(sc2k_sim PRIVATE -Wall -Wextra -Wpedantic)

add_library(sc2k_audio src/audio/midi.cpp src/audio/audio.cpp)
target_include_directories(sc2k_audio PUBLIC src)

add_executable(sc2k-audio tools/sc2k_audio.cpp)
target_link_libraries(sc2k-audio PRIVATE sc2k_audio)

add_library(sc2k_render src/render/map_renderer.cpp src/render/png.cpp src/resources/sprite_archive.cpp src/resources/bmp.cpp)
target_link_libraries(sc2k_render PUBLIC sc2k_sim)
target_compile_options(sc2k_render PRIVATE -Wall -Wextra -Wpedantic)

add_executable(sc2k-resources tools/sc2k_resources.cpp)
target_link_libraries(sc2k-resources PRIVATE sc2k_sim)

add_executable(sc2k-render tools/sc2k_render.cpp)
target_link_libraries(sc2k-render PRIVATE sc2k_render)

add_executable(sc2k-save-info tools/sc2k_save_info.cpp)
target_link_libraries(sc2k-save-info PRIVATE sc2k_save)

add_executable(sc2k-sim tools/sc2k_sim.cpp)
target_link_libraries(sc2k-sim PRIVATE sc2k_sim)

add_executable(sc2k-verify tools/sc2k_verify.cpp)
target_link_libraries(sc2k-verify PRIVATE sc2k_sim)

# Interactive application (SDL3, from Homebrew `sdl3`)
find_package(SDL3 QUIET CONFIG)
if(SDL3_FOUND)
  add_executable(sc2k-app src/app/main.cpp src/app/game_ui.cpp src/app/tool_catalog.cpp src/app/budget_window.cpp src/app/icons.cpp src/app/info_windows.cpp src/app/title_screen.cpp src/audio/audio_device.cpp)
  target_include_directories(sc2k-app PRIVATE src)
  target_link_libraries(sc2k-app PRIVATE sc2k_render sc2k_audio SDL3::SDL3)
else()
  message(STATUS "SDL3 not found: sc2k-app not built (brew install sdl3)")
endif()

enable_testing()
add_executable(test_sim tests/test_rng.cpp tests/test_clock.cpp tests/test_save.cpp tests/test_power.cpp tests/test_environment.cpp tests/test_economy.cpp tests/test_zone.cpp tests/test_things.cpp tests/test_tick.cpp tests/test_render.cpp tests/test_tools.cpp tests/test_audio.cpp tests/test_main.cpp)
target_link_libraries(test_sim PRIVATE sc2k_render sc2k_sim sc2k_save sc2k_audio)
add_test(NAME sim COMMAND test_sim)
set_tests_properties(sim PROPERTIES ENVIRONMENT "SC2K_DATA_PATH=$ENV{SC2K_DATA_PATH}")

# Dev-only differential testing against the original binary (needs Homebrew `unicorn`, GPL-2.0; never shipped).
option(SC2K_BUILD_EMU "Build the Unicorn-based differential-testing tools" ON)
if(SC2K_BUILD_EMU)
  find_library(UNICORN_LIB unicorn HINTS /opt/homebrew/lib /usr/local/lib)
  find_path(UNICORN_INC unicorn/unicorn.h HINTS /opt/homebrew/include /usr/local/include)
  if(UNICORN_LIB AND UNICORN_INC)
    add_executable(sc2k-emu-diff tools/emu/emu.cpp tools/emu/sim_image.cpp tools/emu/emu_diff.cpp)
    target_include_directories(sc2k-emu-diff PRIVATE tools ${UNICORN_INC})
    target_link_libraries(sc2k-emu-diff PRIVATE sc2k_sim ${UNICORN_LIB})
  else()
    message(STATUS "unicorn not found: sc2k-emu-diff not built")
  endif()
endif()
