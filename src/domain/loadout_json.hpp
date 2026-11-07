#pragma once

#include <nlohmann/json.hpp>

#include "domain/loadout.hpp"

namespace as::domain {

inline const char* slotKey(Slot slot) { return slotName(slot); }

inline std::optional<Slot> slotFromKey(std::string_view key) {
  for (int i = 0; i <= static_cast<int>(Slot::Feet); ++i) {
    const auto s = static_cast<Slot>(i);
    if (key == slotName(s)) return s;
  }
  return std::nullopt;
}

// Loadout <-> JSON object { "<Slot>": "<itemId>", ... }. from_json skips unrecognised slot keys.
inline void to_json(nlohmann::json& j, const Loadout& loadout) {
  j = nlohmann::json::object();
  for (const auto& [slot, itemId] : loadout) j[slotKey(slot)] = itemId;
}

inline void from_json(const nlohmann::json& j, Loadout& loadout) {
  loadout.clear();
  if (!j.is_object()) return;
  for (const auto& [key, value] : j.items()) {
    if (!value.is_string()) continue;
    if (const auto slot = slotFromKey(key)) loadout[*slot] = value.get<std::string>();
  }
}

}  // namespace as::domain
