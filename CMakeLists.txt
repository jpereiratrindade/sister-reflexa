cmake_minimum_required(VERSION 3.24)
project(sister_reflexa VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(sister-reflexa src/main.cpp)
target_compile_definitions(sister-reflexa PRIVATE SISTER_REFLEXA_VERSION="0.1.0")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(sister-reflexa PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
  add_test(NAME sister_reflexa_self_test COMMAND sister-reflexa --self-test)
  add_test(NAME sister_reflexa_status COMMAND sister-reflexa --status)
  set_tests_properties(sister_reflexa_status PROPERTIES PASS_REGULAR_EXPRESSION "core=READY")
endif()
