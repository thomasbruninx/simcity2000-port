cmake_minimum_required(VERSION 3.20)
project(simcity2000_port LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(sc2k_sim src/simulation/rng.cpp src/simulation/clock.cpp)
target_include_directories(sc2k_sim PUBLIC src)
target_compile_options(sc2k_sim PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_executable(test_sim tests/test_rng.cpp tests/test_clock.cpp tests/test_main.cpp)
target_link_libraries(test_sim PRIVATE sc2k_sim)
add_test(NAME sim COMMAND test_sim)
