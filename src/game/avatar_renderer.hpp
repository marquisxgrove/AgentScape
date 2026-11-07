#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/result.hpp"
#include "domain/appearance.hpp"
#include "domain/loadout.hpp"
#include "render/avatar_atlas.hpp"

namespace as::game {

// Draws the paper-doll Adventurer. The compositing itself is pure (render::compositeAvatar); this class
// loads the atlas through SDL3_image, uploads frames as nearest-scaled textures, and caches them keyed by
// (appearance, loadout, facing, idle frame) so unchanged frames cost nothing (character-creator Req 3).
class AvatarRenderer {
 public:
  // Loads <spritesDir>/avatar.json and avatar.png. Returns an error if either is missing or invalid.
  static Result<std::unique_ptr<AvatarRenderer>> create(SDL_Renderer* renderer,
                                                        const std::filesystem::path& spritesDir);
  ~AvatarRenderer();
  AvatarRenderer(const AvatarRenderer&) = delete;
  AvatarRenderer& operator=(const AvatarRenderer&) = delete;

  // A 32x48 RGBA texture owned by the cache (valid until the next texture() call may evict it, so
  // draw it in the same frame). Never null on success.
  SDL_Texture* texture(const domain::Appearance& appearance, const domain::Loadout& loadout,
                       domain::Facing facing, int idleFrame);

  // Call once per frame AFTER the frame has been rendered: frees textures that a full cache retired during it.
  void endFrame();

  // The composited frame scaled up `scale` times with nearest-neighbour filtering (default 4x, the
  // portrait export size). The caller owns the surface (SDL_DestroySurface).
  SDL_Surface* exportSurface(const domain::Appearance& appearance, const domain::Loadout& loadout,
                             domain::Facing facing, int scale = 4);

  int cellWidth() const { return atlas_.meta.cellW; }
  int cellHeight() const { return atlas_.meta.cellH; }
  std::size_t cachedFrames() const { return cache_.size(); }

 private:
  AvatarRenderer(SDL_Renderer* renderer, render::AvatarAtlas atlas) : renderer_(renderer), atlas_(std::move(atlas)) {}
  void clearCache();

  SDL_Renderer* renderer_;
  render::AvatarAtlas atlas_;
  std::unordered_map<std::string, SDL_Texture*> cache_;
  std::vector<SDL_Texture*> retired_;   // evicted this frame, destroyed by endFrame()
};

}  // namespace as::game
