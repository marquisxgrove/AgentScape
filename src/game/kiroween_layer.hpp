#pragma once

#include <imgui.h>

#include <vector>

#include "game/kiroween_fx.hpp"
#include "render/texture_cache.hpp"

namespace as::game {

// Draws the Kiroween overlays into an ImGui draw list. Owns its own animation state; the caller
// steps it each frame with (dt, reduceMotion). Textures come from a shared TextureCache.
class KiroweenLayer {
 public:
  // Fog + jack-o'-lanterns over a region (the World map). pumpkinSpots are frame-pixel anchors.
  void initWorld(std::vector<ImVec2> pumpkinSpots);
  // Bats drifting over a region (the login screen).
  void initBats(float width, float height, std::size_t count);

  void step(double dt, bool reduceMotion);

  // Draw a tiled, drifting fog over [min,max). Static when reduce-motion (set via step()).
  void drawFog(ImDrawList* dl, render::TextureCache& tex, ImVec2 min, ImVec2 max);
  void drawPumpkins(ImDrawList* dl, render::TextureCache& tex, ImVec2 origin);
  void drawBats(ImDrawList* dl, render::TextureCache& tex, ImVec2 origin);

 private:
  Drifter fog_{};
  std::vector<Pumpkin> pumpkins_;
  std::vector<Bat> bats_;
  float batWidth_ = 0;
};

}  // namespace as::game
