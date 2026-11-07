#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "domain/kiroween.hpp"

namespace as::app {

struct Config {
  std::optional<domain::KiroweenSetting> kiroween;  // CLI > env > URL query > build default
  std::optional<std::string> dataDir;               // --data-dir, else AGENTSCAPE_DATA_DIR (desktop only)
  bool reseed = false;                              // --reseed
  bool resetData = false;                           // --reset-data
  bool headlessSmoke = false;                       // --smoke: run a few frames and quit (CI)
  std::optional<std::string> startScreen;           // --screen=<name> (dev builds: "ui-gallery")
  std::optional<std::string> snapshotPath;          // --snapshot=<file.png>: save the frame, implies --smoke
  std::optional<std::string> exportAvatarPath;      // --export-avatar=<png>: write the default Adventurer at 4x and exit
  std::optional<std::string> smokeDemoDir;          // --smoke-demo=<dir>: run the demo path, snapshot each step
  std::vector<std::string> warnings;                // unknown/invalid arguments, logged at startup
};

struct ConfigSources {
  std::span<const std::string_view> args;  // argv without the program name
  std::function<std::optional<std::string>(std::string_view)> env;  // environment lookup
  std::optional<std::string> urlKiroween;  // web: ?kiroween= value
  std::string_view buildKiroweenDefault = "auto";
};

// Pure (no SDL): all inputs are injected so precedence can be unit-tested.
Config parseConfig(const ConfigSources& sources);

}  // namespace as::app
