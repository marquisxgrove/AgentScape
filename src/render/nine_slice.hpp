#pragma once

#include <imgui.h>

#include "render/texture_cache.hpp"

namespace as::render {

struct NineSlice {
  Texture texture;
  int border = 0;  // corner size in texture pixels (same on all sides)
};

struct FallbackColors {
  ImU32 fill;
  ImU32 edge;
};

// Draws a 9-slice into [min, max) (whole frame pixels). Corners are drawn 1:1; edges and the centre
// are TILED (not stretched) so speckled stone keeps its pixel texture at any size. If the texture
// is missing, draws a flat fill with a 1 px edge so the UI stays usable without assets.
void drawNineSlice(ImDrawList* dl, const NineSlice& ns, ImVec2 min, ImVec2 max, FallbackColors fallback);

}  // namespace as::render
