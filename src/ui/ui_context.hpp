#pragma once

#include <imgui.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "render/nine_slice.hpp"
#include "ui/palette.hpp"

namespace as::ui {

struct FontSet {
  ImFont* body = nullptr;   // default UI text
  ImFont* title = nullptr;  // panel titles, headings
};

struct Fonts {
  FontSet pixel;  // Silkscreen (OFL), native 8 px grid
  FontSet plain;  // Atkinson Hyperlegible (OFL), for the "Plain font" setting
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
};

Surface currentSurface();

void setContext(UiContext* ctx);
UiContext& ctx();

// Loads both font sets from <assets>/fonts into the ImGui atlas (before the first NewFrame).
// Missing files fall back to ImGui's default font so the app still runs.
Fonts loadFonts(const std::filesystem::path& fontsDir);
Skin loadSkin(render::TextureCache& textures, const std::filesystem::path& uiDir);

const FontSet& activeFonts();  // pixel or plain, per the setting

void beginFrame();  // resets per-frame state (hover text)
void setHoverText(std::string markup);

}  // namespace as::ui
