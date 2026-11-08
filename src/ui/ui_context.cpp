#include "ui/ui_context.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace as::ui {

namespace {
UiContext* g_ctx = nullptr;

// Silkscreen is drawn on a 1/8 em grid, so 8 px (and exact multiples) rasterise with every edge on a pixel
// boundary. At 8 px the text was too small to read comfortably on a 765x503 frame, so the body and title use 12 px
// (1.5x). At 1x that is a little soft. When the frame is drawn at 2x or more (render::GameFrame), the font is
// rasterised at 24, 36 or 48 px instead and its glyphs are hardened to solid pixels, so the text is as sharp as the
// art around it. The "Plain font" setting uses a vector face, so any size is crisp there.
constexpr float PIXEL_BODY_PX = 12.0F;
constexpr float PIXEL_TITLE_PX = 12.0F;
constexpr float PLAIN_BODY_PX = 16.0F;
constexpr float PLAIN_TITLE_PX = 17.0F;

ImFont* addFont(const std::filesystem::path& path, float px, bool pixelFont) {
  ImGuiIO& io = ImGui::GetIO();
  ImFontConfig cfg;
  cfg.OversampleH = 1;
  cfg.OversampleV = 1;
  cfg.PixelSnapH = true;
  if (!pixelFont) cfg.RasterizerMultiply = 1.1F;  // slightly heavier strokes for legibility
  std::error_code ec;
  if (std::filesystem::exists(path, ec)) {
    if (ImFont* f = io.Fonts->AddFontFromFileTTF(path.string().c_str(), px, &cfg)) return f;
  }
  SDL_Log("Font %s missing; using the built-in fallback", path.string().c_str());
  ImFontConfig fallback;
  fallback.SizePixels = px;  // already multiplied by the render resolution
  return io.Fonts->AddFontDefault(&fallback);
}

// Turns a pixel font's anti-aliased edges into solid pixels. At 2x and 4x Silkscreen's edges already fall on
// pixel boundaries, so this only cleans up rounding; at 3x it decides the half-covered pixels.
void hardenGlyphs(const ImFont* font, unsigned char* pixels, int texW, int texH) {
  if (font == nullptr || pixels == nullptr) return;
  for (const ImFontGlyph& g : font->Glyphs) {
    const int x0 = std::clamp(static_cast<int>(g.U0 * static_cast<float>(texW) + 0.5F), 0, texW);
    const int x1 = std::clamp(static_cast<int>(g.U1 * static_cast<float>(texW) + 0.5F), 0, texW);
    const int y0 = std::clamp(static_cast<int>(g.V0 * static_cast<float>(texH) + 0.5F), 0, texH);
    const int y1 = std::clamp(static_cast<int>(g.V1 * static_cast<float>(texH) + 0.5F), 0, texH);
    for (int y = y0; y < y1; ++y) {
      unsigned char* row = pixels + static_cast<std::ptrdiff_t>(y) * texW;
      for (int x = x0; x < x1; ++x) row[x] = row[x] >= 128 ? 255 : 0;
    }
  }
}
}  // namespace

void setContext(UiContext* ctx) { g_ctx = ctx; }

UiContext& ctx() {
  assert(g_ctx != nullptr && "ui::setContext must be called first");
  return *g_ctx;
}

Fonts loadFonts(const std::filesystem::path& fontsDir, int res) {
  ImFontAtlas* atlas = ImGui::GetIO().Fonts;
  atlas->Clear();
  Fonts f;
  f.res = std::max(1, res);
  const auto px = [&f](float size) { return size * static_cast<float>(f.res); };
  // The first font added is ImGui's default: the pixel body font.
  f.pixel.body = addFont(fontsDir / "Silkscreen-Regular.ttf", px(PIXEL_BODY_PX), true);
  f.pixel.title = addFont(fontsDir / "Silkscreen-Bold.ttf", px(PIXEL_TITLE_PX), true);
  f.plain.body = addFont(fontsDir / "AtkinsonHyperlegible-Regular.ttf", px(PLAIN_BODY_PX), false);
  f.plain.title = addFont(fontsDir / "AtkinsonHyperlegible-Bold.ttf", px(PLAIN_TITLE_PX), false);
  atlas->Build();
  if (f.res > 1) {
    unsigned char* pixels = nullptr;
    int texW = 0;
    int texH = 0;
    atlas->GetTexDataAsAlpha8(&pixels, &texW, &texH);
    hardenGlyphs(f.pixel.body, pixels, texW, texH);
    if (f.pixel.title != f.pixel.body) hardenGlyphs(f.pixel.title, pixels, texW, texH);
  }
  // Draw at 1/res so every layout measures exactly as it does at 1x.
  for (ImFont* font : {f.pixel.body, f.pixel.title, f.plain.body, f.plain.title}) {
    if (font != nullptr) font->Scale = 1.0F / static_cast<float>(f.res);
  }
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

render::TextureCache& UiContext::mapTextures() {
  assert(textures != nullptr && "UiContext::textures not set");
  return *textures;
}

void setHoverText(std::string markup) { ctx().hoverMarkup = std::move(markup); }

}  // namespace as::ui
