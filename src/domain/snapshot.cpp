#include "domain/snapshot.hpp"

#include <nlohmann/json.hpp>

#include "domain/appearance_json.hpp"
#include "domain/items.hpp"
#include "domain/loadout_json.hpp"

namespace as::domain {

std::string snapshotToJson(const AdventurerSnapshot& snapshot) {
  nlohmann::json j;
  j["name"] = snapshot.name;
  j["appearance"] = snapshot.appearance;
  j["loadout"] = snapshot.loadout;
  return j.dump();
}

std::optional<AdventurerSnapshot> snapshotFromParts(std::string_view name, std::string_view appearanceJson,
                                                    std::string_view loadoutJson) {
  AdventurerSnapshot out;
  out.name = std::string(name);
  const auto appearance = parseAppearance(appearanceJson);
  if (!appearance || !validateAppearance(*appearance)) return std::nullopt;
  out.appearance = *appearance;
  const auto loadout = nlohmann::json::parse(loadoutJson.begin(), loadoutJson.end(), nullptr, /*allow_exceptions=*/false);
  if (loadout.is_object()) {
    Loadout raw;
    from_json(loadout, raw);   // skips unknown slot names
    for (const auto& [slot, id] : raw) {
      const Item* item = findItem(id);
      if (item != nullptr && item->slot == slot) out.loadout[slot] = id;
    }
  }
  return out;
}

std::optional<AdventurerSnapshot> snapshotFromJson(std::string_view text) {
  const auto j = nlohmann::json::parse(text.begin(), text.end(), nullptr, /*allow_exceptions=*/false);
  if (!j.is_object() || !j.contains("name") || !j["name"].is_string() || !j.contains("appearance")) return std::nullopt;
  return snapshotFromParts(j["name"].get<std::string>(), j["appearance"].dump(), j.contains("loadout") ? j["loadout"].dump() : "{}");
}

}  // namespace as::domain
