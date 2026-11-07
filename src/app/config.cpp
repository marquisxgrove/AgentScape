#include "app/config.hpp"

#include <array>

namespace as::app {

namespace {
bool startsWith(std::string_view s, std::string_view prefix) { return s.substr(0, prefix.size()) == prefix; }
}  // namespace

Config parseConfig(const ConfigSources& sources) {
  Config cfg;
  std::optional<std::string> cliKiroween;

  const auto& args = sources.args;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string_view arg = args[i];
    constexpr std::string_view kKiroween = "--kiroween=";
    constexpr std::string_view kDataDirEq = "--data-dir=";
    if (startsWith(arg, kKiroween)) {
      cliKiroween = std::string(arg.substr(kKiroween.size()));
      if (!domain::parseKiroweenSetting(*cliKiroween)) {
        cfg.warnings.push_back("Ignoring --kiroween value (use auto, on or off): " + *cliKiroween);
      }
    } else if (arg == "--data-dir") {
      if (i + 1 < args.size()) {
        cfg.dataDir = std::string(args[++i]);
      } else {
        cfg.warnings.push_back("--data-dir needs a path");
      }
    } else if (startsWith(arg, kDataDirEq)) {
      cfg.dataDir = std::string(arg.substr(kDataDirEq.size()));
    } else if (arg == "--reseed") {
      cfg.reseed = true;
    } else if (arg == "--reset-data") {
      cfg.resetData = true;
    } else if (arg == "--smoke") {
      cfg.headlessSmoke = true;
    } else if (startsWith(arg, "--screen=")) {
      cfg.startScreen = std::string(arg.substr(std::string_view{"--screen="}.size()));
    } else if (startsWith(arg, "--snapshot=")) {
      cfg.snapshotPath = std::string(arg.substr(std::string_view{"--snapshot="}.size()));
      cfg.headlessSmoke = true;
    } else if (startsWith(arg, "--export-avatar=")) {
      cfg.exportAvatarPath = std::string(arg.substr(std::string_view{"--export-avatar="}.size()));
      cfg.headlessSmoke = true;
    } else if (startsWith(arg, "--smoke-demo=")) {
      cfg.smokeDemoDir = std::string(arg.substr(std::string_view{"--smoke-demo="}.size()));
    } else if (startsWith(arg, "-psn_")) {
      // macOS Finder process serial number; ignore.
    } else {
      cfg.warnings.push_back("Unknown argument: " + std::string(arg));
    }
  }

  const auto envKiroween = sources.env ? sources.env("KIROWEEN_MODE") : std::nullopt;
  if (!cfg.dataDir && sources.env) {
    if (auto dir = sources.env("AGENTSCAPE_DATA_DIR"); dir && !dir->empty()) cfg.dataDir = *dir;
  }
  if (cfg.dataDir && cfg.dataDir->empty()) cfg.dataDir.reset();

  const auto view = [](const std::optional<std::string>& s) {
    return s ? std::optional<std::string_view>{*s} : std::nullopt;
  };
  const std::array<std::optional<std::string_view>, 4> kiroweenSources{
      view(cliKiroween), view(envKiroween), view(sources.urlKiroween), sources.buildKiroweenDefault};
  cfg.kiroween = domain::resolveKiroweenSetting(kiroweenSources);
  return cfg;
}

}  // namespace as::app
