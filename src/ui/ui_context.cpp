#include "ui/ui_context.hpp"

#include <SDL3/SDL.h>

#include <cassert>

namespace as::ui {

namespace {
UiContext* g_ctx = nullptr;

// Silkscreen is drawn on a 1/8 em grid, so 8 px (and exact multiples) rasterise with every
// edge on a pixel boundary: crisp without anti-aliasing.
constexpr float PIXEL_BODY_PX = 8.0F;
constexpr float PIXEL_TITLE_PX = 8.0F;  // bold cut at the same native size
constexpr float PLAIN_BODY_PX = 13.0F;
constexpr float PLAIN_TITLE_PX = 14.0F;

ImFont* addFont(const std::filesystem::path& path, float px, bool pixelPerfect) {
  ImGuiIO& io = ImGui::GetIO();
  ImFontConfig cfg;
  cfg.OversampleH = 1;
  cfg.OversampleV = 1;
  cfg.PixelSnapH = true;
  if (!pixelPerfect) cfg.RasterizerMultiply = 1.1F;  // slightly heavier strokes for legibility
  std::error_code ec;
  if (std::filesystem::exists(path, ec)) {
    if (ImFont* f = io.Fonts->AddFontFromFileTTF(path.string().c_str(), px, &cfg)) return f;
  }
  SDL_Log("Font %s missing; using the built-in fallback", path.string().c_str());
  ImFontConfig fallback;
  fallback.SizePixels = 13.0F;
  return io.Fonts->AddFontDefault(&fallback);
}
}  // namespace

void setContext(UiContext* ctx) { g_ctx = ctx; }

UiContext& ctx() {
  assert(g_ctx != nullptr && "ui::setContext must be called first");
  return *g_ctx;
}

Fonts loadFonts(const std::filesystem::path& fontsDir) {
  Fonts f;
  // The first font added is ImGui's default: the pixel body font.
  f.pixel.body = addFont(fontsDir / "Silkscreen-Regular.ttf", PIXEL_BODY_PX, true);
  f.pixel.title = addFont(fontsDir / "Silkscreen-Bold.ttf", PIXEL_TITLE_PX, true);
  f.plain.body = addFont(fontsDir / "AtkinsonHyperlegible-Regular.ttf", PLAIN_BODY_PX, false);
  f.plain.title = addFont(fontsDir / "AtkinsonHyperlegible-Bold.ttf", PLAIN_TITLE_PX, false);
  return f;
}

Skin loadSkin(render::TextureCache& textures, const std::filesystem::path& uiDir) {
  Skin s;
  s.stone = {textures.load(uiDir / "stone_panel.png"), 8};
  s.parchment = {textures.load(uiDir / "parchment_panel.png"), 8};
  s.button = {textures.load(uiDir / "button.png"), 4};
  s.buttonHover = {textures.load(uiDir / "button_hover.png"), 4};
  s.buttonPressed = {textures.load(uiDir / "button_pressed.png"), 4};
  return s;
}

const FontSet& activeFonts() { return ctx().plainFont ? ctx().fonts.plain : ctx().fonts.pixel; }

void beginFrame() {
  ctx().hoverMarkup.clear();
  ctx().surfaces.clear();
}

Surface currentSurface() { return ctx().surfaces.empty() ? Surface::Stone : ctx().surfaces.back(); }

void setHoverText(std::string markup) { ctx().hoverMarkup = std::move(markup); }

}  // namespace as::ui
