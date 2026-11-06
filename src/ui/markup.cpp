#include "ui/markup.hpp"

#include <array>
#include <utility>

namespace as::ui {

namespace {
struct NamedColor {
  std::string_view name;
  PaletteColor color;
};
constexpr std::array<NamedColor, 8> MARKUP_COLORS{{
    {"yellow", PaletteColor::TextYellow},
    {"orange", PaletteColor::TextOrange},
    {"cyan", PaletteColor::TextCyan},
    {"green", PaletteColor::TextGreen},
    {"red", PaletteColor::TextRed},
    {"blue", PaletteColor::ChatBlue},
    {"purple", PaletteColor::KiroweenPurple},
    {"pumpkin", PaletteColor::KiroweenPumpkin},
}};

void append(std::vector<TextSpan>& spans, std::string_view text, PaletteColor color) {
  if (text.empty()) return;
  if (!spans.empty() && spans.back().color == color) {
    spans.back().text.append(text);
  } else {
    spans.push_back({std::string(text), color});
  }
}
}  // namespace

PaletteColor textColorOn(Surface surface, PaletteColor requested) {
  if (surface == Surface::Stone) return requested;
  switch (requested) {
    case PaletteColor::TextYellow:
    case PaletteColor::TextCyan:
    case PaletteColor::TextGreen:
      return PaletteColor::Ink;
    case PaletteColor::TextOrange:  // headings
    case PaletteColor::KiroweenPumpkin:
      return PaletteColor::StoneDark;
    default:
      return requested;
  }
}

std::optional<PaletteColor> paletteFromMarkupName(std::string_view name) {
  for (const auto& nc : MARKUP_COLORS) {
    if (nc.name == name) return nc.color;
  }
  return std::nullopt;
}

std::string_view markupNameOf(PaletteColor c) {
  for (const auto& nc : MARKUP_COLORS) {
    if (nc.color == c) return nc.name;
  }
  return {};
}

std::vector<TextSpan> parseMarkup(std::string_view markup, PaletteColor defaultColor) {
  constexpr std::string_view kOpen = "<col=";
  constexpr std::string_view kClose = "</col>";
  constexpr std::string_view kLt = "<lt>";

  std::vector<TextSpan> spans;
  PaletteColor current = defaultColor;
  std::size_t i = 0;
  std::size_t runStart = 0;
  const auto flush = [&](std::size_t end) { append(spans, markup.substr(runStart, end - runStart), current); };

  while (i < markup.size()) {
    if (markup[i] != '<') {
      ++i;
      continue;
    }
    const auto rest = markup.substr(i);
    if (rest.substr(0, kLt.size()) == kLt) {
      flush(i);
      append(spans, "<", current);
      i += kLt.size();
      runStart = i;
    } else if (rest.substr(0, kClose.size()) == kClose) {
      flush(i);
      current = defaultColor;
      i += kClose.size();
      runStart = i;
    } else if (rest.substr(0, kOpen.size()) == kOpen) {
      const auto close = rest.find('>');
      const auto color =
          close == std::string_view::npos ? std::nullopt
                                          : paletteFromMarkupName(rest.substr(kOpen.size(), close - kOpen.size()));
      if (color) {
        flush(i);
        current = *color;
        i += close + 1;
        runStart = i;
      } else {
        ++i;  // not a valid tag: keep it as literal text
      }
    } else {
      ++i;
    }
  }
  flush(markup.size());
  return spans;
}

std::string escapeMarkup(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    if (c == '<') {
      out += "<lt>";
    } else {
      out += c;
    }
  }
  return out;
}

std::string plainText(const std::vector<TextSpan>& spans) {
  std::string out;
  for (const auto& s : spans) out += s.text;
  return out;
}

std::string hoverText(std::string_view action, std::string_view targetName, PaletteColor targetColor,
                      int moreOptions) {
  std::string out{action};
  if (!targetName.empty()) {
    const auto name = markupNameOf(targetColor);
    out += ' ';
    if (!name.empty()) {
      out += "<col=";
      out += name;
      out += '>';
      out += escapeMarkup(targetName);
      out += "</col>";
    } else {
      out += escapeMarkup(targetName);
    }
  }
  if (moreOptions > 0) {
    out += " / " + std::to_string(moreOptions) + (moreOptions == 1 ? " more option" : " more options");
  }
  return out;
}

}  // namespace as::ui
