#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>

#include "app/config.hpp"

using namespace as::app;
using as::domain::KiroweenSetting;

namespace {
struct Fixture {
  std::vector<std::string_view> args;
  std::map<std::string, std::string, std::less<>> env;
  std::optional<std::string> url;
  std::string_view buildDefault = "auto";

  Config parse() const {
    ConfigSources s;
    s.args = args;
    s.env = [this](std::string_view name) -> std::optional<std::string> {
      const auto it = env.find(name);
      return it != env.end() ? std::optional<std::string>{it->second} : std::nullopt;
    };
    s.urlKiroween = url;
    s.buildKiroweenDefault = buildDefault;
    return parseConfig(s);
  }
};
}  // namespace

TEST_CASE("Kiroween precedence is CLI, then env, then URL, then build default", "[unit][config]") {
  Fixture f;
  f.buildDefault = "off";
  CHECK(f.parse().kiroween == KiroweenSetting::Off);
  f.url = "auto";
  CHECK(f.parse().kiroween == KiroweenSetting::Auto);
  f.env["KIROWEEN_MODE"] = "off";
  CHECK(f.parse().kiroween == KiroweenSetting::Off);
  f.args = {"--kiroween=on"};
  CHECK(f.parse().kiroween == KiroweenSetting::On);
}

TEST_CASE("an invalid Kiroween value falls through to the next source and warns", "[unit][config]") {
  Fixture f;
  f.args = {"--kiroween=spooky"};
  f.env["KIROWEEN_MODE"] = "ON";
  const auto cfg = f.parse();
  CHECK(cfg.kiroween == KiroweenSetting::On);
  CHECK(cfg.warnings.size() == 1);
}

TEST_CASE("the data-dir flag beats AGENTSCAPE_DATA_DIR, in both spellings", "[unit][config]") {
  Fixture f;
  f.env["AGENTSCAPE_DATA_DIR"] = "/from/env";
  CHECK(f.parse().dataDir == "/from/env");
  f.args = {"--data-dir", "/from/cli"};
  CHECK(f.parse().dataDir == "/from/cli");
  f.args = {"--data-dir=/eq/form"};
  CHECK(f.parse().dataDir == "/eq/form");
}

TEST_CASE("an empty data dir means the platform default", "[unit][config]") {
  Fixture f;
  f.env["AGENTSCAPE_DATA_DIR"] = "";
  CHECK_FALSE(f.parse().dataDir.has_value());
}

TEST_CASE("flags are parsed and unknown arguments are reported", "[unit][config]") {
  Fixture f;
  f.args = {"--reseed", "--reset-data", "--smoke", "--bogus", "--data-dir"};
  const auto cfg = f.parse();
  CHECK(cfg.reseed);
  CHECK(cfg.resetData);
  CHECK(cfg.headlessSmoke);
  CHECK_FALSE(cfg.dataDir.has_value());
  CHECK(cfg.warnings.size() == 2);  // --bogus, and --data-dir without a path
}

#include "platform/paths.hpp"

TEST_CASE("an explicit data dir never touches the platform default", "[unit][config]") {
  int defaultCalls = 0;
  const auto platformDefault = [&] {
    ++defaultCalls;
    return std::filesystem::path{"/pref/AgentScape"};
  };
  CHECK(as::platform::resolveDataDir(std::string{"/mine"}, platformDefault) == "/mine");
  CHECK(defaultCalls == 0);
  CHECK(as::platform::resolveDataDir(std::nullopt, platformDefault) == "/pref/AgentScape");
  CHECK(defaultCalls == 1);
}
