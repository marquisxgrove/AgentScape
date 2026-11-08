#include "domain/settings.hpp"

#include <nlohmann/json.hpp>

namespace as::domain {

namespace {
bool boolOr(const nlohmann::json& obj, const char* key, bool fallback) {
  const auto it = obj.find(key);
  return it != obj.end() && it->is_boolean() ? it->get<bool>() : fallback;
}
}  // namespace

std::string settingsToJson(const Settings& s) {
  nlohmann::json j;
  j["plainFont"] = s.plainFont;
  j["reduceMotion"] = s.reduceMotion;
  j["sound"] = s.sound;
  j["fullscreen"] = s.fullscreen;
  return j.dump();
}

Settings settingsFromJson(std::string_view json, const Settings& defaults) {
  const auto j = nlohmann::json::parse(json.begin(), json.end(), nullptr, /*allow_exceptions=*/false);
  if (!j.is_object()) return defaults;
  Settings s;
  s.plainFont = boolOr(j, "plainFont", defaults.plainFont);
  s.reduceMotion = boolOr(j, "reduceMotion", defaults.reduceMotion);
  s.sound = boolOr(j, "sound", defaults.sound);
  // Saves from when this was "Pixel-perfect scaling" stored the opposite flag.
  const bool legacyFullscreen = j.contains("pixelPerfect") && j["pixelPerfect"].is_boolean()
                                    ? !j["pixelPerfect"].get<bool>()
                                    : defaults.fullscreen;
  s.fullscreen = boolOr(j, "fullscreen", legacyFullscreen);
  return s;
}

}  // namespace as::domain
