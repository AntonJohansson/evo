cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(evo)

#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# this is so stupid
#set(CMAKE_MODULE_PATH "/usr/local/share/SFML/cmake/Modules")
find_package(SFML REQUIRED system window graphics)

set(SOURCES
	src/main.cpp
	src/evo.cpp
	src/perlin.cpp
	)


add_executable(evo ${SOURCES})

set(CMAKE_CXX_COMPILER "clang++")
target_compile_options(evo PRIVATE
	-std=c++1z -Wall -Wextra -Wfatal-errors -g
        -O3
	)

set_target_properties(evo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${SFML_INCLUDE_DIR})
target_link_libraries(evo ${SFML_LIBRARIES} sfml-system sfml-window sfml-graphics)
