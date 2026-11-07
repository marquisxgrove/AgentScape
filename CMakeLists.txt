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
include(EmbedFiles)

# ---------------------------------------------------------------------------
# as_domain: pure C++20 (no SDL, ImGui, SQLite, I/O). as_core: Result, errors.
# ---------------------------------------------------------------------------
add_library(as_core STATIC
  src/core/civil_time.cpp
  src/core/ids.cpp
  src/core/rng.cpp)
target_include_directories(as_core PUBLIC src)
target_link_libraries(as_core PUBLIC tl::expected)
as_enable_warnings(as_core)
as_enable_sanitizers(as_core)

add_library(as_domain STATIC
  src/domain/chat_log.cpp
  src/domain/context_menu.cpp
  src/domain/adventurer_name.cpp
  src/domain/appearance.cpp
  src/domain/codex_query.cpp
  src/domain/codex_validation.cpp
  src/domain/device_flow.cpp
  src/domain/frame.cpp
  src/domain/geometry.cpp
  src/domain/items.cpp
  src/domain/loadout.cpp
  src/domain/palette.cpp
  src/domain/rate_limit.cpp
  src/domain/skills.cpp
  src/domain/slug.cpp
  src/domain/stat_table.cpp
  src/domain/voting.cpp
  src/domain/wardrobe.cpp
  src/domain/guest_name.cpp
  src/domain/kiroween.cpp
  src/domain/settings.cpp)
target_include_directories(as_domain PUBLIC src)
target_link_libraries(as_domain PUBLIC as_core nlohmann_json::nlohmann_json)
as_enable_warnings(as_domain)
as_enable_sanitizers(as_domain)

# ---------------------------------------------------------------------------
# as_db: SQLite RAII layer + embedded migrations. as_app_core: EventBus, ScreenStack (SDL-free).
# ---------------------------------------------------------------------------
as_embed_migrations(AS_EMBEDDED_MIGRATIONS)
as_embed_seed(AS_EMBEDDED_SEED)
add_library(as_db STATIC
  src/db/database.cpp
  src/db/migrations.cpp
  src/db/seed_loader.cpp
  ${AS_EMBEDDED_MIGRATIONS}
  ${AS_EMBEDDED_SEED})
target_include_directories(as_db PUBLIC src)
target_link_libraries(as_db PUBLIC as_core SQLiteLib::SQLite3 PRIVATE as_domain nlohmann_json::nlohmann_json)
as_enable_warnings(as_db)
as_enable_sanitizers(as_db)

# SDL-free platform pieces (file ops, path resolution, flush scheduling) - unit-testable.
add_library(as_platform_core STATIC
  src/platform/data_files.cpp
  src/platform/flush_scheduler.cpp
  src/platform/paths.cpp)
target_include_directories(as_platform_core PUBLIC src)
target_link_libraries(as_platform_core PUBLIC as_core)
as_enable_warnings(as_platform_core)
as_enable_sanitizers(as_platform_core)

# Pure UI logic (palette, markup parser): no ImGui, unit-testable.
add_library(as_ui_core STATIC
  src/game/kiroween_fx.cpp
  src/ui/chat_model.cpp
  src/ui/costumer_draft.cpp
  src/ui/context_menu_model.cpp
  src/ui/markup.cpp)
target_include_directories(as_ui_core PUBLIC src)
target_link_libraries(as_ui_core PUBLIC as_core as_domain)
as_enable_warnings(as_ui_core)
as_enable_sanitizers(as_ui_core)

# Pure rendering logic (no SDL): atlas model, palette-swap compositor. Unit-testable without a window.
add_library(as_render_core STATIC
  src/render/avatar_atlas.cpp
  src/render/avatar_compositor.cpp)
target_include_directories(as_render_core PUBLIC src)
target_link_libraries(as_render_core PUBLIC as_domain nlohmann_json::nlohmann_json)
as_enable_warnings(as_render_core)
as_enable_sanitizers(as_render_core)

# EventBus: typed pub/sub, no app/service deps (services publish onto it).
add_library(as_events STATIC src/app/event_bus.cpp)
target_include_directories(as_events PUBLIC src)
target_link_libraries(as_events PUBLIC as_domain)
as_enable_warnings(as_events)
as_enable_sanitizers(as_events)

# Services own transactions and orchestrate domain + db (no SDL/ImGui).
add_library(as_services STATIC
  src/services/activity_service.cpp
  src/services/adventurer_service.cpp
  src/services/codex_cache.cpp
  src/services/codex_service.cpp
  src/services/codex_store.cpp
  src/services/profile_service.cpp)
target_include_directories(as_services PUBLIC src)
target_link_libraries(as_services PUBLIC as_events as_db as_domain)
as_enable_warnings(as_services)
as_enable_sanitizers(as_services)

add_library(as_app_core STATIC
  src/app/config.cpp
  src/app/local_store.cpp
  src/app/screen_stack.cpp)
target_include_directories(as_app_core PUBLIC src)
target_link_libraries(as_app_core PUBLIC as_domain as_db as_services as_events as_platform_core)
as_enable_warnings(as_app_core)
as_enable_sanitizers(as_app_core)


# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
if(AS_BUILD_APP)
  add_executable(agentscape
    src/main.cpp
    src/app/app.cpp
    src/platform/paths_sdl.cpp
    src/platform/persist.cpp
    src/platform/accessibility.cpp
    src/platform/github_device_flow.cpp
    src/platform/png_export.cpp
    src/platform/web_url.cpp
    src/game/avatar_renderer.cpp
    src/game/hotspots.cpp
    src/game/kiroween_layer.cpp
    src/render/game_frame.cpp
    src/render/nine_slice.cpp
    src/render/texture_cache.cpp
    src/screens/codex/codex_card_screen.cpp
    src/screens/codex/codex_screen.cpp
    src/screens/codex/codex_widgets.cpp
    src/screens/costumer/costumer_screen.cpp
    src/screens/login/login_screen.cpp
    src/screens/world/world_screen.cpp
    src/ui/chatbox.cpp
    src/ui/context_menu_host.cpp
    src/ui/tab_panel.cpp
    src/ui/theme.cpp
    src/ui/ui_context.cpp
    src/ui/widgets.cpp)
  if(AS_ENABLE_DEV_SCREENS)
    target_sources(agentscape PRIVATE src/screens/dev/ui_gallery_screen.cpp src/screens/dev/avatar_gallery_screen.cpp)
  endif()
  # Assets ship next to the binary (desktop) or inside the .data file (web).
  if(NOT EMSCRIPTEN)
    add_custom_command(TARGET agentscape POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:agentscape>/assets
      VERBATIM)
  endif()
  target_include_directories(agentscape PRIVATE src)
  target_link_libraries(agentscape PRIVATE
    as_app_core as_services as_render_core as_ui_core as_db as_domain imgui SDL3::SDL3 SDL3_image::SDL3_image)
  target_compile_definitions(agentscape PRIVATE
    AS_KIROWEEN_DEFAULT="${AS_KIROWEEN_DEFAULT}"
    AS_GITHUB_CLIENT_ID="${AS_GITHUB_CLIENT_ID}")
  if(AS_HAVE_CURL)
    target_compile_definitions(agentscape PRIVATE AS_HAVE_CURL=1)
    target_link_libraries(agentscape PRIVATE CURL::libcurl)
  endif()
  if(AS_ENABLE_DEV_SCREENS)
    target_compile_definitions(agentscape PRIVATE AS_ENABLE_DEV_SCREENS=1)
  endif()
  as_enable_warnings(agentscape)
  as_enable_sanitizers(agentscape)
  if(EMSCRIPTEN)
    set_target_properties(agentscape PROPERTIES SUFFIX ".html")
    target_link_options(agentscape PRIVATE
      --shell-file=${CMAKE_SOURCE_DIR}/web/shell.html
      --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets
      -sALLOW_MEMORY_GROWTH=1 -lidbfs.js)
  endif()
endif()

# ---------------------------------------------------------------------------
# Tests (labels: unit, property, integration, smoke)
# ---------------------------------------------------------------------------
if(AS_BUILD_TESTS)
  enable_testing()
  include(Catch)

  add_executable(unit_tests
    tests/unit/avatar_render_test.cpp
    tests/unit/costumer_draft_test.cpp
    tests/unit/civil_time_test.cpp
    tests/unit/codex_query_test.cpp
    tests/unit/codex_scoring_test.cpp
    tests/unit/codex_validation_test.cpp
    tests/unit/config_test.cpp
    tests/unit/core_test.cpp
    tests/unit/context_menu_test.cpp
    tests/unit/device_flow_test.cpp
    tests/unit/domain_shell_test.cpp
    tests/unit/event_bus_test.cpp
    tests/unit/chatbox_test.cpp
    tests/unit/flush_scheduler_test.cpp
    tests/unit/geometry_test.cpp
    tests/unit/kiroween_fx_test.cpp
    tests/unit/markup_test.cpp
    tests/unit/palette_test.cpp
    tests/unit/wardrobe_test.cpp
    tests/unit/regressions_game_shell.cpp
    tests/unit/screen_stack_test.cpp)
  target_link_libraries(unit_tests PRIVATE as_app_core as_events as_platform_core as_render_core as_ui_core as_domain Catch2::Catch2WithMain)
  target_compile_definitions(unit_tests PRIVATE AS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  as_enable_warnings(unit_tests)
  as_enable_sanitizers(unit_tests)
  catch_discover_tests(unit_tests PROPERTIES LABELS "unit")

  add_executable(property_tests
    tests/property/game_shell_props.cpp
    tests/property/character_creator_props.cpp
    tests/property/loadout_props.cpp
    tests/property/agent_codex_props.cpp)
  target_link_libraries(property_tests PRIVATE as_domain Catch2::Catch2WithMain rapidcheck)
  target_include_directories(property_tests PRIVATE tests)
  as_enable_warnings(property_tests)
  as_enable_sanitizers(property_tests)
  catch_discover_tests(property_tests PROPERTIES LABELS "property" ENVIRONMENT "RC_PARAMS=max_success=200")

  add_executable(integration_tests
    tests/integration/activity_service_test.cpp
    tests/integration/adventurer_service_test.cpp
    tests/integration/codex_service_test.cpp
    tests/integration/codex_store_test.cpp
    tests/integration/db_test.cpp
    tests/integration/local_store_test.cpp
    tests/integration/profile_service_test.cpp
    tests/integration/seed_codex_test.cpp
    tests/integration/seed_test.cpp)
  target_link_libraries(integration_tests PRIVATE as_app_core as_services as_db Catch2::Catch2WithMain)
  as_enable_warnings(integration_tests)
  as_enable_sanitizers(integration_tests)
  catch_discover_tests(integration_tests PROPERTIES LABELS "integration")

  # Smoke test: drives the real app (SDL dummy video + software renderer) through the demo path and
  # saves a screen snapshot per step. Only when the app is built.
  if(AS_BUILD_APP AND NOT EMSCRIPTEN)
    add_test(NAME smoke_demo
      COMMAND ${CMAKE_COMMAND}
        -DAGENTSCAPE=$<TARGET_FILE:agentscape>
        -DOUTDIR=${CMAKE_BINARY_DIR}/smoke
        -P ${CMAKE_SOURCE_DIR}/cmake/RunSmoke.cmake)
    set_tests_properties(smoke_demo PROPERTIES LABELS "smoke"
      ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software")
    add_test(NAME smoke_costumer
      COMMAND ${CMAKE_COMMAND}
        -DAGENTSCAPE=$<TARGET_FILE:agentscape>
        -DOUTDIR=${CMAKE_BINARY_DIR}/smoke-costumer
        -P ${CMAKE_SOURCE_DIR}/cmake/RunSmokeCostumer.cmake)
    set_tests_properties(smoke_costumer PROPERTIES LABELS "smoke"
      ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software")
  endif()
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
