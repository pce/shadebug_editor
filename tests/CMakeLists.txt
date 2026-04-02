cmake_minimum_required(VERSION 3.25)
include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(test_vfs test_vfs.cpp)
target_link_libraries(test_vfs PRIVATE Catch2::Catch2WithMain)
target_include_directories(test_vfs PRIVATE ${CMAKE_SOURCE_DIR}/src)

include(CTest)
add_test(NAME vfs_tests COMMAND test_vfs)

