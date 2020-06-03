cmake_minimum_required(VERSION 3.11)
project(VoxelTerrain)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_LIST_DIR}/src/*.h)

add_library(voxelterrain SHARED ${SOURCES} ${HEADERS})
target_include_directories(voxelterrain SYSTEM
  PUBLIC "${CMAKE_CURRENT_LIST_DIR}/godot-cpp/godot_headers/"
  PUBLIC "${CMAKE_CURRENT_LIST_DIR}/godot-cpp/include/"
  PUBLIC "${CMAKE_CURRENT_LIST_DIR}/godot-cpp/include/core/"
  PUBLIC "${CMAKE_CURRENT_LIST_DIR}/godot-cpp/include/gen/")

target_link_directories(voxelterrain PUBLIC "${CMAKE_CURRENT_LIST_DIR}/godot-cpp/bin/")
target_link_libraries(voxelterrain "godot-cpp.linux.debug.64")

set(BUILD_TESTS OFF CACHE BOOL "Build Tests")

if (BUILD_TESTS)
  enable_testing()
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/extern/googletest" "extern/googletest")
  set_target_properties(gtest PROPERTIES FOLDER extern)
  set_target_properties(gtest_main PROPERTIES FOLDER extern)
  set_target_properties(gmock PROPERTIES FOLDER extern)
  set_target_properties(gmock_main PROPERTIES FOLDER extern)

  include(GoogleTest)
  include_directories(src)

  add_executable(ChunkTest test/ChunkTest.cpp)
  target_link_libraries(ChunkTest voxelterrain gtest gtest_main)
  add_test(ChunkTest ChunkTest)
endif (BUILD_TESTS)
