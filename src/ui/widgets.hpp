#pragma once

#include <imgui.h>

#include <string_view>

#include "ui/markup.hpp"
#include "ui/palette.hpp"

namespace as::ui {

// Text with a 1 px INK drop shadow (+1,+1), then the colour on top. Never a format string.
void ShadowText(std::string_view text, PaletteColor color = PaletteColor::TextYellow);

// Shadowed markup text (<col=...>). wrapWidth <= 0 means no wrapping (explicit '\n' still breaks).
void RichText(std::string_view markup, PaletteColor defaultColor = PaletteColor::TextYellow, float wrapWidth = 0.0F);

// Draws already-parsed spans at an absolute position without affecting layout (hover text, overlays).
void drawSpans(ImDrawList* dl, ImVec2 pos, const std::vector<TextSpan>& spans);

// Bevelled stone button with hover and pressed states. Returns true when activated (mouse,
// keyboard or gamepad). The label may carry an ImGui "##id" suffix. size {0,0} fits the label.
// hoverMarkup, if given, becomes the frame's hover text while the button is hovered.
bool PixelButton(const char* label, ImVec2 size = {0, 0}, std::string_view hoverMarkup = {});

enum class PanelVariant { Stone, Parchment };

// RAII panel: draws the 9-slice and an orange shadowed title, then opens a padded, clipped child
// region for content. size {0,0} fills the available space.
class StonePanel {
 public:
  StonePanel(const char* id, std::string_view title, ImVec2 size = {0, 0}, PanelVariant variant = PanelVariant::Stone);
  ~StonePanel();
  StonePanel(const StonePanel&) = delete;
  StonePanel& operator=(const StonePanel&) = delete;
  StonePanel(StonePanel&&) = delete;
  StonePanel& operator=(StonePanel&&) = delete;
};

// Draws this frame's hover text (ui::setHoverText) at the top-left of the given region.
void HoverText(ImVec2 regionMin);

}  // namespace as::ui
