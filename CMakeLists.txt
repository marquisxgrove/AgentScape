cmake_minimum_required(VERSION 3.22)
project(AgentScape VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

option(AS_BUILD_APP "Build the SDL3 + ImGui application" ON)
option(AS_BUILD_TESTS "Build the test suites" ON)
option(AS_SANITIZE "Enable AddressSanitizer + UBSan" OFF)
option(AS_ENABLE_DEV_SCREENS "Enable UI/avatar gallery screens" OFF)
set(AS_KIROWEEN_DEFAULT "auto" CACHE STRING "Baked-in Kiroween default: auto|on|off")
set(AS_GITHUB_CLIENT_ID "" CACHE STRING "Public GitHub OAuth App client ID (device flow)")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(Warnings)
include(Dependencies)

# ---------------------------------------------------------------------------
# as_domain: pure C++20 (no SDL, ImGui, SQLite, I/O). as_core: Result, errors.
# ---------------------------------------------------------------------------
add_library(as_core INTERFACE)
target_include_directories(as_core INTERFACE src)
target_link_libraries(as_core INTERFACE tl::expected)

add_library(as_domain STATIC
  src/domain/frame.cpp
  src/domain/kiroween.cpp)
target_include_directories(as_domain PUBLIC src)
target_link_libraries(as_domain PUBLIC as_core nlohmann_json::nlohmann_json)
as_enable_warnings(as_domain)
as_enable_sanitizers(as_domain)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
if(AS_BUILD_APP)
  add_executable(agentscape
    src/main.cpp
    src/app/app.cpp
    src/render/game_frame.cpp)
  target_include_directories(agentscape PRIVATE src)
  target_link_libraries(agentscape PRIVATE
    as_domain imgui SDL3::SDL3 SDL3_image::SDL3_image SQLiteLib::SQLite3)
  target_compile_definitions(agentscape PRIVATE
    AS_KIROWEEN_DEFAULT="${AS_KIROWEEN_DEFAULT}"
    AS_GITHUB_CLIENT_ID="${AS_GITHUB_CLIENT_ID}")
  if(AS_ENABLE_DEV_SCREENS)
    target_compile_definitions(agentscape PRIVATE AS_ENABLE_DEV_SCREENS=1)
  endif()
  as_enable_warnings(agentscape)
  as_enable_sanitizers(agentscape)
  if(EMSCRIPTEN)
    set_target_properties(agentscape PROPERTIES SUFFIX ".html")
    target_link_options(agentscape PRIVATE
      --shell-file=${CMAKE_SOURCE_DIR}/web/shell.html
      -sALLOW_MEMORY_GROWTH=1 -lidbfs.js)
  endif()
endif()

# ---------------------------------------------------------------------------
# Tests (labels: unit, property, integration, smoke)
# ---------------------------------------------------------------------------
if(AS_BUILD_TESTS)
  enable_testing()
  include(Catch)

  add_executable(unit_tests tests/unit/core_test.cpp)
  target_link_libraries(unit_tests PRIVATE as_domain Catch2::Catch2WithMain)
  as_enable_warnings(unit_tests)
  as_enable_sanitizers(unit_tests)
  catch_discover_tests(unit_tests PROPERTIES LABELS "unit")

  add_executable(property_tests tests/property/game_shell_props.cpp)
  target_link_libraries(property_tests PRIVATE as_domain Catch2::Catch2WithMain rapidcheck)
  target_include_directories(property_tests PRIVATE tests)
  as_enable_warnings(property_tests)
  as_enable_sanitizers(property_tests)
  catch_discover_tests(property_tests PROPERTIES LABELS "property" ENVIRONMENT "RC_PARAMS=max_success=200")

  # integration_tests and smoke_test are added with the db layer and the app (tasks 3 and 11).
endif()

# ---------------------------------------------------------------------------
# Formatting / static analysis helpers
# ---------------------------------------------------------------------------
file(GLOB_RECURSE AS_ALL_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.hpp tests/*.cpp tests/*.hpp)
find_program(CLANG_FORMAT_EXE NAMES clang-format)
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_FORMAT_EXE)
  add_custom_target(format COMMAND ${CLANG_FORMAT_EXE} -i ${AS_ALL_SOURCES} VERBATIM)
endif()
if(CLANG_TIDY_EXE)
  add_custom_target(tidy
    COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${AS_ALL_SOURCES} VERBATIM)
endif()
