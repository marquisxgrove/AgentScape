#include "game/hotspots.hpp"

namespace as::game {

namespace {
// Polygons are in frame pixels, inside the 512x334 viewport (ui::VIEWPORT at 4,4). Rough building
// footprints on the town map; the art is drawn to match in tools/gen_map_art.py.
const std::vector<Hotspot>& table() {
  static const std::vector<Hotspot> HOTSPOTS = {
      {"codex", "Grand Codex", "Enter", app::ScreenId::Codex,
       "A towering library of familiars and their recorded deeds.", false,
       {{40, 50}, {140, 50}, {140, 140}, {40, 140}}},
      {"tavern", "Tavern", "Enter", app::ScreenId::Tavern,
       "Where adventurers swap tales of quests gone right and horribly wrong.", false,
       {{250, 55}, {350, 55}, {350, 143}, {250, 143}}},
      {"hiscores", "Hiscores Obelisk", "Read", app::ScreenId::Hiscores,
       "A black stone that remembers every champion's name.", false,
       {{178, 34}, {212, 34}, {212, 129}, {178, 129}}},
      {"costumer", "The Costumer", "Enter", app::ScreenId::Costumer,
       "Racks of tunics, hoods, and the occasional cursed hat.", false,
       {{55, 180}, {155, 180}, {155, 265}, {55, 265}}},
      {"graveyard", "Graveyard", "Enter", app::ScreenId::Graveyard,
       "The Hall of Hallucinations. Here lie the community's most-agreed failures.", true,
       {{240, 178}, {350, 178}, {350, 266}, {240, 266}}},
  };
  return HOTSPOTS;
}
}  // namespace

std::span<const Hotspot> allHotspots() { return table(); }

std::vector<const Hotspot*> visibleHotspots(bool kiroween) {
  std::vector<const Hotspot*> out;
  for (const auto& h : table()) {
    if (h.kiroweenOnly && !kiroween) continue;
    out.push_back(&h);
  }
  return out;
}

const Hotspot* hotspotAt(domain::Point p, bool kiroween) {
  const Hotspot* hit = nullptr;
  for (const auto& h : table()) {
    if (h.kiroweenOnly && !kiroween) continue;
    if (domain::pointInPolygon(h.polygon, p)) hit = &h;  // later entries win (drawn on top)
  }
  return hit;
}

}  // namespace as::game
