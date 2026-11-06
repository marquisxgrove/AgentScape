#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "app/screen_stack.hpp"
#include "domain/geometry.hpp"

namespace as::game {

// A clickable area of the Promptmoor map. The same data drives the map and the accessible list.
struct Hotspot {
  std::string_view id;
  std::string_view name;
  std::string_view defaultAction;  // verb shown in hover text / default menu item, e.g. "Enter"
  app::ScreenId screen;            // where the default action leads
  std::string_view examine;        // witty, factual examine text
  bool kiroweenOnly = false;       // e.g. the Graveyard
  std::vector<domain::Point> polygon;  // frame pixels, within the viewport
};

// All hotspots, in a stable order (used as the list order). Includes Kiroween-only ones; filter
// with kiroweenOnly when the season is off.
std::span<const Hotspot> allHotspots();

// Topmost hotspot containing the point (last in draw order wins), honouring the Kiroween filter.
// Returns nullptr if none.
const Hotspot* hotspotAt(domain::Point p, bool kiroween);

// Hotspots visible this season, in list order.
std::vector<const Hotspot*> visibleHotspots(bool kiroween);

}  // namespace as::game
