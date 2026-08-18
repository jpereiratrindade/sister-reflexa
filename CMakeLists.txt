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
  # T1 / original tests (preserved)
  add_test(NAME sister_reflexa_self_test COMMAND sister-reflexa --self-test)
  add_test(NAME sister_reflexa_status    COMMAND sister-reflexa --status)
  set_tests_properties(sister_reflexa_status PROPERTIES PASS_REGULAR_EXPRESSION "core=READY")

  # T2: bundle creation
  add_test(NAME T2_bundle_creation    COMMAND sister-reflexa --test-bundle-creation)
  set_tests_properties(T2_bundle_creation PROPERTIES PASS_REGULAR_EXPRESSION "PASS")

  # T3: temporal preservation (critical)
  add_test(NAME T3_temporal           COMMAND sister-reflexa --test-temporal)
  set_tests_properties(T3_temporal PROPERTIES PASS_REGULAR_EXPRESSION "PASS")

  # T4: deterministic assessment
  add_test(NAME T4_deterministic      COMMAND sister-reflexa --test-deterministic)
  set_tests_properties(T4_deterministic PROPERTIES PASS_REGULAR_EXPRESSION "PASS")

  # T5: restart reproducibility
  add_test(NAME T5_restart            COMMAND sister-reflexa --test-restart)
  set_tests_properties(T5_restart PROPERTIES PASS_REGULAR_EXPRESSION "PASS")

  # T6: API vertical slice (store layer, no sockets)
  add_test(NAME T6_api_slice          COMMAND sister-reflexa --test-api-slice)
  set_tests_properties(T6_api_slice PROPERTIES PASS_REGULAR_EXPRESSION "PASS")

  # T7: web availability (starts local server on port 18092, checks /api/health)
  add_test(NAME T7_web                COMMAND sister-reflexa --test-web)
  set_tests_properties(T7_web PROPERTIES PASS_REGULAR_EXPRESSION "PASS" TIMEOUT 10)
endif()
