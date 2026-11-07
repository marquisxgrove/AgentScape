#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "domain/appearance.hpp"
#include "domain/loadout.hpp"

namespace as::domain {

// The author's active Adventurer, frozen when a Quest Log was written (quest-logs: Adventurer_Snapshot). It is
// stored as JSON {"name", "appearance", "loadout"} and drawn later by the same avatar renderer as a live Adventurer.
struct AdventurerSnapshot {
  std::string name;
  Appearance appearance;
  Loadout loadout;

  bool operator==(const AdventurerSnapshot&) const = default;
};

std::string snapshotToJson(const AdventurerSnapshot& snapshot);

// Reads a stored snapshot defensively: nullopt if it is not a JSON object with a name and a valid Appearance. A
// loadout entry for an item that is no longer in the catalog, or sits in the wrong slot, is dropped (the avatar
// renders without it) instead of failing the whole snapshot, since the item catalog can change between versions.
std::optional<AdventurerSnapshot> snapshotFromJson(std::string_view json);

}  // namespace as::domain
