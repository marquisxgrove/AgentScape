#pragma once

#include <string>
#include <string_view>

namespace as::domain {

// Per-User UI settings (game-shell Requirement 7.1), persisted as JSON in user.settings.
struct Settings {
  bool plainFont = false;
  bool reduceMotion = false;
  bool sound = false;  // sound effects are off by default (art-direction.md)
  bool pixelPerfect = false;  // whole-number frame scaling only; off = fit the window

  bool operator==(const Settings&) const = default;
};

// {"plainFont":bool,"reduceMotion":bool,"sound":bool,"pixelPerfect":bool}
std::string settingsToJson(const Settings& s);

// Lenient by design: this reads a database column, so it never throws or rejects. Missing keys, wrong
// types, unknown keys, and malformed JSON all fall back to `defaults` field by field.
Settings settingsFromJson(std::string_view json, const Settings& defaults = {});

}  // namespace as::domain
