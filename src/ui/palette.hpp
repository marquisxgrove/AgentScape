#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace as::ui {

// The strict AgentScape palette (.kiro/steering/art-direction.md). Widgets must use these, never
// inline colour literals. Pure C++ (no ImGui) so the markup parser can be unit-tested.
enum class PaletteColor : std::uint8_t {
  StoneDark,
  Stone,
  Parchment,
  Ink,
  TextYellow,
  TextOrange,
  TextCyan,
  TextGreen,
  TextRed,
  ChatBlue,
  KiroweenPurple,
  KiroweenPumpkin,
};

struct Rgb {
  std::uint8_t r, g, b;
};

constexpr Rgb rgbOf(PaletteColor c) {
  switch (c) {
    case PaletteColor::StoneDark: return {0x3e, 0x35, 0x29};
    case PaletteColor::Stone: return {0x5d, 0x54, 0x47};
    case PaletteColor::Parchment: return {0xc8, 0xb8, 0x8a};
    case PaletteColor::Ink: return {0x00, 0x00, 0x00};
    case PaletteColor::TextYellow: return {0xff, 0xff, 0x00};
    case PaletteColor::TextOrange: return {0xff, 0x98, 0x1f};
    case PaletteColor::TextCyan: return {0x00, 0xff, 0xff};
    case PaletteColor::TextGreen: return {0x00, 0xff, 0x00};
    case PaletteColor::TextRed: return {0xff, 0x00, 0x00};
    case PaletteColor::ChatBlue: return {0x00, 0x00, 0xff};
    case PaletteColor::KiroweenPurple: return {0x6b, 0x2f, 0xa0};
    case PaletteColor::KiroweenPumpkin: return {0xff, 0x75, 0x18};
  }
  return {0, 0, 0};
}

enum class Surface : std::uint8_t { Stone, Parchment };

// Colour actually used for text on a surface. Stone: unchanged. Parchment: the light text colours
// (yellow, orange, cyan, green, pumpkin all fall below 2:1 on PARCHMENT) map to INK / STONE_DARK,
// which pass 4.5:1. Red, blue and purple are kept as-is.
PaletteColor textColorOn(Surface surface, PaletteColor requested);
// Text on stone gets the 1 px INK drop shadow; on parchment it would only muddy dark text.
constexpr bool shadowOn(Surface surface) { return surface == Surface::Stone; }

// Names accepted by <col=NAME> markup. Only text colours are allowed.
std::optional<PaletteColor> paletteFromMarkupName(std::string_view name);
std::string_view markupNameOf(PaletteColor c);  // "" for colours that aren't markup-addressable

}  // namespace as::ui
