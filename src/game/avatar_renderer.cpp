#include "game/avatar_renderer.hpp"

#include <SDL3_image/SDL_image.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "domain/appearance_json.hpp"
#include "domain/loadout_json.hpp"
#include "render/avatar_compositor.hpp"

namespace as::game {

namespace {
constexpr std::size_t MAX_CACHED_FRAMES = 256;  // ample for the Costumer; cleared wholesale if exceeded

tl::unexpected<GameError> loadFailed(const std::string& detail) {
  return makeError(GameErrorCode::Internal, "Your Adventurer could not be drawn.", detail);
}

std::string cacheKey(const domain::Appearance& a, const domain::Loadout& l, domain::Facing f, int frame) {
  const nlohmann::json ja = a;
  const nlohmann::json jl = l;
  return ja.dump() + jl.dump() + "|" + std::to_string(static_cast<int>(f)) + "|" + std::to_string(frame);
}
}  // namespace

Result<std::unique_ptr<AvatarRenderer>> AvatarRenderer::create(SDL_Renderer* renderer,
                                                               const std::filesystem::path& spritesDir) {
  std::ifstream jsonFile(spritesDir / "avatar.json");
  if (!jsonFile.good()) return loadFailed("missing " + (spritesDir / "avatar.json").string());
  std::stringstream jsonText;
  jsonText << jsonFile.rdbuf();
  auto meta = render::parseAtlasMeta(jsonText.str());
  if (!meta) return loadFailed("avatar.json is invalid");

  const std::string pngPath = (spritesDir / "avatar.png").string();
  SDL_Surface* loaded = IMG_Load(pngPath.c_str());
  if (loaded == nullptr) return loadFailed(std::string("could not load avatar.png: ") + SDL_GetError());
  // Whatever the decoder produced (indexed or RGBA), normalise to RGBA32 and recover palette indices.
  SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(loaded);
  if (rgba == nullptr) return loadFailed(std::string("could not convert avatar.png: ") + SDL_GetError());

  render::AvatarAtlas atlas;
  atlas.meta = std::move(*meta);
  std::vector<std::uint8_t> tight(static_cast<std::size_t>(rgba->w) * static_cast<std::size_t>(rgba->h) * 4);
  for (int y = 0; y < rgba->h; ++y) {  // the surface pitch may include padding
    const auto* src = static_cast<const std::uint8_t*>(rgba->pixels) + static_cast<std::size_t>(y) * static_cast<std::size_t>(rgba->pitch);
    std::copy(src, src + static_cast<std::size_t>(rgba->w) * 4, tight.begin() + static_cast<std::ptrdiff_t>(y) * rgba->w * 4);
  }
  int unknown = 0;
  atlas.image = render::indexFromRgba(tight.data(), rgba->w, rgba->h, atlas.meta.palette, &unknown);
  const int w = rgba->w;
  const int h = rgba->h;
  SDL_DestroySurface(rgba);
  if (unknown != 0) SDL_Log("avatar.png: %d pixels not in the avatar.json palette (drawn transparent)", unknown);

  const int rowsNeeded = static_cast<int>(atlas.meta.rows.size());
  if (w != atlas.meta.cellW * atlas.meta.facings * atlas.meta.frames || h < rowsNeeded * atlas.meta.cellH) {
    return loadFailed("avatar.png size does not match avatar.json");
  }
  return std::unique_ptr<AvatarRenderer>(new AvatarRenderer(renderer, std::move(atlas)));
}

AvatarRenderer::~AvatarRenderer() {
  clearCache();
  endFrame();
}

// Textures are drawn at the END of the frame (ImGui records draw commands that point at them), so a full cache
// must not destroy what earlier code in the same frame has already handed to the draw list. They are retired here
// and destroyed by endFrame() once the frame has been rendered.
void AvatarRenderer::clearCache() {
  for (auto& [key, tex] : cache_) retired_.push_back(tex);
  cache_.clear();
}

void AvatarRenderer::endFrame() {
  for (SDL_Texture* tex : retired_) SDL_DestroyTexture(tex);
  retired_.clear();
}

SDL_Texture* AvatarRenderer::texture(const domain::Appearance& appearance, const domain::Loadout& loadout,
                                     domain::Facing facing, int idleFrame) {
  const std::string key = cacheKey(appearance, loadout, facing, idleFrame);
  if (const auto it = cache_.find(key); it != cache_.end()) return it->second;
  if (cache_.size() >= MAX_CACHED_FRAMES) clearCache();

  const render::RgbaImage image = render::compositeAvatar(atlas_, appearance, loadout, facing, idleFrame);
  SDL_Texture* tex = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.w, image.h);
  if (tex == nullptr) {
    SDL_Log("Avatar texture failed: %s", SDL_GetError());
    return nullptr;
  }
  SDL_UpdateTexture(tex, nullptr, image.px.data(), image.w * 4);
  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  cache_.emplace(key, tex);
  return tex;
}

SDL_Surface* AvatarRenderer::exportSurface(const domain::Appearance& appearance, const domain::Loadout& loadout,
                                           domain::Facing facing, int scale) {
  const render::RgbaImage small = render::compositeAvatar(atlas_, appearance, loadout, facing, 0);
  const render::RgbaImage big = render::scaleNearest(small, scale);
  SDL_Surface* surface = SDL_CreateSurface(big.w, big.h, SDL_PIXELFORMAT_RGBA32);
  if (surface == nullptr) return nullptr;
  for (int y = 0; y < big.h; ++y) {
    auto* dst = static_cast<std::uint8_t*>(surface->pixels) + static_cast<std::size_t>(y) * static_cast<std::size_t>(surface->pitch);
    std::copy(big.at(0, y), big.at(0, y) + static_cast<std::size_t>(big.w) * 4, dst);
  }
  return surface;
}

}  // namespace as::game
