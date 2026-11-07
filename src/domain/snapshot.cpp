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

std::optional<AdventurerSnapshot> snapshotFromJson(std::string_view text) {
  const auto j = nlohmann::json::parse(text.begin(), text.end(), nullptr, /*allow_exceptions=*/false);
  if (!j.is_object() || !j.contains("name") || !j["name"].is_string() || !j.contains("appearance")) return std::nullopt;
  AdventurerSnapshot out;
  out.name = j["name"].get<std::string>();
  const auto appearance = parseAppearance(j["appearance"].dump());
  if (!appearance || !validateAppearance(*appearance)) return std::nullopt;
  out.appearance = *appearance;
  if (j.contains("loadout")) {
    Loadout raw;
    from_json(j["loadout"], raw);   // skips unknown slot names
    for (const auto& [slot, id] : raw) {
      const Item* item = findItem(id);
      if (item != nullptr && item->slot == slot) out.loadout[slot] = id;
    }
  }
  return out;
}

}  // namespace as::domain
