#pragma once

#include <imgui.h>

#include "ui/palette.hpp"

namespace as::ui {

constexpr ImVec4 toVec4(PaletteColor c, float alpha = 1.0F) {
  const Rgb rgb = rgbOf(c);
  return {static_cast<float>(rgb.r) / 255.0F, static_cast<float>(rgb.g) / 255.0F, static_cast<float>(rgb.b) / 255.0F,
          alpha};
}

inline ImU32 toU32(PaletteColor c, float alpha = 1.0F) { return ImGui::ColorConvertFloat4ToU32(toVec4(c, alpha)); }

// Maps the palette onto ImGuiStyle: zero rounding, no anti-aliasing, whole-pixel spacing,
// transparent window chrome (panels are drawn by StonePanel), yellow 2 px nav cursor.
// Kiroween swaps selection/active accents to purple and pumpkin.
void applyTheme(bool kiroween);

}  // namespace as::ui
