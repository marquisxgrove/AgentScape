#pragma once

#include <imgui.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "render/nine_slice.hpp"
#include "render/texture_cache.hpp"
#include "ui/palette.hpp"

namespace as::ui {

struct FontSet {
  ImFont* body = nullptr;   // default UI text
  ImFont* title = nullptr;  // panel titles, headings
};

struct Fonts {
  FontSet pixel;  // Silkscreen (OFL), native 8 px grid
  FontSet plain;  // Atkinson Hyperlegible (OFL), for the "Plain font" setting
  int res = 1;    // rasterised at res x their size and drawn at 1/res scale (see render::GameFrame)
};

struct Skin {
  render::NineSlice stone;
  render::NineSlice parchment;
  render::NineSlice button;
  render::NineSlice buttonHover;
  render::NineSlice buttonPressed;
};

// Shared UI state for the retro widgets. One instance, owned by App, installed with setContext().
struct UiContext {
  Fonts fonts;
  Skin skin;
  bool plainFont = false;
  bool kiroween = false;
  std::function<void()> onClick;  // click sound hook (no-op until audio lands)

  // Hover text for this frame (markup). Cleared by beginFrame(), set by whatever is hovered.
  std::string hoverMarkup;

  // Surfaces of the open StonePanels (innermost last); text colours adapt to the innermost one.
  std::vector<Surface> surfaces;

  // Shared texture cache for screens (map, sprites). Set by App; may be null in tests.
  render::TextureCache* textures = nullptr;
  render::TextureCache& mapTextures();  // asserts textures != nullptr
};

Surface currentSurface();

void setContext(UiContext* ctx);
UiContext& ctx();

// Loads both font sets from <assets>/fonts into a fresh ImGui atlas, rasterised at `res` times their size and
// scaled by 1/res, so layout sizes are the same at every res. Call before NewFrame, and again (after destroying
// the backend's font texture) when the frame's render resolution changes. Missing files fall back to ImGui's
// default font so the app still runs.
Fonts loadFonts(const std::filesystem::path& fontsDir, int res = 1);
Skin loadSkin(render::TextureCache& textures, const std::filesystem::path& uiDir);

const FontSet& activeFonts();  // pixel or plain, per the setting

void beginFrame();  // resets per-frame state (hover text)
void setHoverText(std::string markup);

}  // namespace as::ui
