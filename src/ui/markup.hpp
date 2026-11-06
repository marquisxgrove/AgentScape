#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "ui/palette.hpp"

namespace as::ui {

struct TextSpan {
  std::string text;
  PaletteColor color = PaletteColor::TextYellow;
  bool operator==(const TextSpan&) const = default;
};

// Parses hover/chat markup:
//   <col=NAME>...</col>   colour a run (NAME from paletteFromMarkupName; no nesting - a new <col>
//                         replaces the current colour, </col> returns to the default)
//   <lt>                  a literal '<' (produced by escapeMarkup)
// Anything else, including unknown tags or colour names, is literal text. Adjacent runs with the
// same colour are merged and empty runs dropped.
std::vector<TextSpan> parseMarkup(std::string_view markup, PaletteColor defaultColor = PaletteColor::TextYellow);

// Makes arbitrary (user) text safe to embed in markup: parseMarkup(escapeMarkup(s)) renders s.
std::string escapeMarkup(std::string_view text);

// Concatenated text of all spans.
std::string plainText(const std::vector<TextSpan>& spans);

// "Enter <col=cyan>Grand Codex</col> / 3 more options". targetName is escaped here.
std::string hoverText(std::string_view action, std::string_view targetName, PaletteColor targetColor,
                      int moreOptions);

}  // namespace as::ui
