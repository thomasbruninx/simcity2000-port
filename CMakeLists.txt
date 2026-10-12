cmake_minimum_required(VERSION 3.20)
project(simcity2000_port LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(sc2k_sim src/simulation/rng.cpp src/simulation/clock.cpp)
target_include_directories(sc2k_sim PUBLIC src)
target_compile_options(sc2k_sim PRIVATE -Wall -Wextra -Wpedantic)

add_library(sc2k_save src/save/rle.cpp src/save/city.cpp)
target_include_directories(sc2k_save PUBLIC src)
target_compile_options(sc2k_save PRIVATE -Wall -Wextra -Wpedantic)

add_executable(sc2k-save-info tools/sc2k_save_info.cpp)
target_link_libraries(sc2k-save-info PRIVATE sc2k_save)

enable_testing()
add_executable(test_sim tests/test_rng.cpp tests/test_clock.cpp tests/test_save.cpp tests/test_main.cpp)
target_link_libraries(test_sim PRIVATE sc2k_sim sc2k_save)
add_test(NAME sim COMMAND test_sim)
set_tests_properties(sim PROPERTIES ENVIRONMENT "SC2K_DATA_PATH=$ENV{SC2K_DATA_PATH}")
