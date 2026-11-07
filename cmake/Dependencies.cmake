# All third-party dependencies, pinned to exact tags. Never vendor sources into src/.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# --- Always needed (core/domain/db/tests) -----------------------------------
FetchContent_Declare(tl_expected
  GIT_REPOSITORY https://github.com/TartanLlama/expected.git
  GIT_TAG        v1.1.0
  GIT_SHALLOW    TRUE
  SYSTEM)  # third-party headers must not trip our -Werror warning set
set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
  SYSTEM)

FetchContent_MakeAvailable(tl_expected nlohmann_json)

# SQLite: the pinned amalgamation is the canonical source (required for the web build).
# Pass -DAS_USE_SYSTEM_SQLITE=ON on desktop machines without access to sqlite.org.
option(AS_USE_SYSTEM_SQLITE "Use the system SQLite instead of the pinned amalgamation" OFF)
if(AS_USE_SYSTEM_SQLITE)
  find_package(SQLite3 REQUIRED)
  add_library(as_sqlite INTERFACE)
  target_link_libraries(as_sqlite INTERFACE SQLite::SQLite3)
  add_library(SQLiteLib::SQLite3 ALIAS as_sqlite)
else()
  FetchContent_Declare(sqlite3_amalgamation
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip)
  FetchContent_MakeAvailable(sqlite3_amalgamation)
  add_library(sqlite3 STATIC ${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c)
  target_include_directories(sqlite3 SYSTEM PUBLIC ${sqlite3_amalgamation_SOURCE_DIR})
  target_compile_definitions(sqlite3 PUBLIC SQLITE_THREADSAFE=0 SQLITE_OMIT_LOAD_EXTENSION=1)
  add_library(SQLiteLib::SQLite3 ALIAS sqlite3)
endif()

# --- Tests -------------------------------------------------------------------
if(AS_BUILD_TESTS)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE)
  FetchContent_Declare(rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG        2c3c4365aca21ef4e612768fdc95b1ce8b39a651
    GIT_SUBMODULES "")
  set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

  # RapidCheck's own CMakeLists unconditionally adds ext/ (git submodules), so populate the
  # sources and define the two targets we need (library + Catch2 integration) ourselves.
  FetchContent_GetProperties(rapidcheck)
  if(NOT rapidcheck_POPULATED)
    FetchContent_Populate(rapidcheck)
  endif()
  file(GLOB_RECURSE RC_SOURCES CONFIGURE_DEPENDS ${rapidcheck_SOURCE_DIR}/src/*.cpp)
  add_library(rapidcheck STATIC ${RC_SOURCES})
  target_include_directories(rapidcheck SYSTEM PUBLIC ${rapidcheck_SOURCE_DIR}/include)
  target_compile_features(rapidcheck PUBLIC cxx_std_14)
  add_library(rapidcheck_catch INTERFACE)
  target_include_directories(rapidcheck_catch SYSTEM INTERFACE ${rapidcheck_SOURCE_DIR}/extras/catch/include)
  target_link_libraries(rapidcheck_catch INTERFACE rapidcheck)
endif()

# --- Application (SDL3 + ImGui) ----------------------------------------------
if(AS_BUILD_APP)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  # Vendored image codecs (libpng etc.) are git submodules; needed for 8-bit indexed PNG palette
  # support (character creator). Disable only in restricted environments (falls back to stb_image).
  option(AS_FETCH_SUBMODULES "Fetch SDL3_image's vendored codec submodules" ON)
  if(AS_FETCH_SUBMODULES)
    set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3_image
      GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
      GIT_TAG        release-3.2.4
      GIT_SHALLOW    TRUE)
  else()
    set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3_image
      GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
      GIT_TAG        release-3.2.4
      GIT_SHALLOW    TRUE
      GIT_SUBMODULES "")
  endif()
  set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.8
    GIT_SHALLOW    TRUE)
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(SDL3 SDL3_image imgui)

  # libcurl is needed only for the optional desktop GitHub device-flow login. Fetch it only when a
  # client id is configured, so the default build (and restricted CI) stays curl-free.
  if(AS_GITHUB_CLIENT_ID AND NOT EMSCRIPTEN)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
    set(CURL_ENABLE_EXPORT_TARGET OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(curl
      GIT_REPOSITORY https://github.com/curl/curl.git
      GIT_TAG        curl-8_10_1
      GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(curl)
    # Dependencies.cmake is include()d, so this lands in the same scope as CMakeLists.txt (no PARENT_SCOPE).
    set(AS_HAVE_CURL ON)
  endif()

  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
  target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(imgui PUBLIC SDL3::SDL3)
endif()
