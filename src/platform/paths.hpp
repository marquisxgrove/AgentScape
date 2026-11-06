#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace as::platform {

inline constexpr const char* DB_FILE_NAME = "agentscape.db";
inline constexpr const char* WEB_PERSIST_DIR = "/persist";

// An explicit --data-dir / AGENTSCAPE_DATA_DIR wins over the platform default. The default is only
// evaluated when needed, because SDL_GetPrefPath creates its directory as a side effect.
std::filesystem::path resolveDataDir(const std::optional<std::string>& explicitDir,
                                     const std::function<std::filesystem::path()>& platformDefault);

// --- SDL-backed (paths_sdl.cpp; app target only) ---------------------------------------------
// Desktop: SDL_GetPrefPath("AgentScape", "AgentScape"). Web: /persist (the IDBFS mount).
std::filesystem::path defaultDataDir();
// Desktop: SDL_GetBasePath()/assets. Web: /assets (preloaded into the .data file).
std::filesystem::path assetsDir();

}  // namespace as::platform
