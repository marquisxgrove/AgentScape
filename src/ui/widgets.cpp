#include "ui/widgets.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>

#include "ui/theme.hpp"
#include "ui/ui_context.hpp"

namespace as::ui {

namespace {

ImVec2 floorVec(ImVec2 v) { return {std::floor(v.x), std::floor(v.y)}; }

float lineHeight() { return std::floor(ImGui::GetFontSize()) + 2.0F; }

ImVec2 measure(ImFont* font, float size, std::string_view s) {
  return font->CalcTextSizeA(size, FLT_MAX, 0.0F, s.data(), s.data() + s.size());
}

void drawShadowed(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, PaletteColor color, std::string_view s,
                  Surface surface) {
  if (s.empty()) return;
  pos = floorVec(pos);
  const char* b = s.data();
  const char* e = s.data() + s.size();
  if (shadowOn(surface)) dl->AddText(font, size, {pos.x + 1, pos.y + 1}, toU32(PaletteColor::Ink), b, e);
  dl->AddText(font, size, pos, toU32(textColorOn(surface, color)), b, e);
}

void drawShadowed(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, PaletteColor color, std::string_view s) {
  drawShadowed(dl, font, size, pos, color, s, currentSurface());
}

// Visible part of an ImGui label ("Play##guest" -> "Play").
std::string_view visibleLabel(const char* label) {
  const char* hash = std::strstr(label, "##");
  return hash != nullptr ? std::string_view(label, static_cast<std::size_t>(hash - label)) : std::string_view(label);
}

const render::NineSlice& sliceFor(PanelVariant v) {
  return v == PanelVariant::Parchment ? ctx().skin.parchment : ctx().skin.stone;
}

render::FallbackColors fallbackFor(PanelVariant v) {
  return v == PanelVariant::Parchment
             ? render::FallbackColors{toU32(PaletteColor::Parchment), toU32(PaletteColor::StoneDark)}
             : render::FallbackColors{toU32(PaletteColor::Stone), toU32(PaletteColor::StoneDark)};
}

// Lays out spans with greedy word wrapping; calls draw(pos, color, text) for each run.
// Returns the total size.
template <typename DrawFn>
ImVec2 layoutSpans(const std::vector<TextSpan>& spans, float wrapWidth, DrawFn&& draw) {
  ImFont* font = ImGui::GetFont();
  const float size = ImGui::GetFontSize();
  const float lh = lineHeight();
  float x = 0;
  float y = 0;
  float maxX = 0;
  for (const auto& span : spans) {
    std::string_view rest = span.text;
    while (!rest.empty()) {
      if (rest.front() == '\n') {
        x = 0;
        y += lh;
        rest.remove_prefix(1);
        continue;
      }
      // Next token: a run of spaces, or a word (up to a space or newline).
      std::size_t n = 0;
      if (rest.front() == ' ') {
        while (n < rest.size() && rest[n] == ' ') ++n;
      } else {
        while (n < rest.size() && rest[n] != ' ' && rest[n] != '\n') ++n;
      }
      const auto token = rest.substr(0, n);
      rest.remove_prefix(n);
      const float w = std::floor(measure(font, size, token).x);
      if (wrapWidth > 0 && x > 0 && x + w > wrapWidth) {
        x = 0;
        y += lh;
        if (token.front() == ' ') continue;  // swallow spaces at a wrap point
      }
      draw(ImVec2{x, y}, span.color, token);
      x += w;
      maxX = std::max(maxX, x);
    }
  }
  return {maxX + 1, y + lh};
}

}  // namespace

void ShadowText(std::string_view text, PaletteColor color) {
  ImFont* font = ImGui::GetFont();
  const float size = ImGui::GetFontSize();
  const ImVec2 pos = floorVec(ImGui::GetCursorScreenPos());
  drawShadowed(ImGui::GetWindowDrawList(), font, size, pos, color, text);
  const ImVec2 sz = measure(font, size, text);
  ImGui::Dummy({std::floor(sz.x) + 1, lineHeight()});
}

void RichText(std::string_view markup, PaletteColor defaultColor, float wrapWidth) {
  const auto spans = parseMarkup(markup, defaultColor);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  ImFont* font = ImGui::GetFont();
  const float size = ImGui::GetFontSize();
  const ImVec2 origin = floorVec(ImGui::GetCursorScreenPos());
  const ImVec2 total = layoutSpans(spans, wrapWidth, [&](ImVec2 p, PaletteColor c, std::string_view t) {
    drawShadowed(dl, font, size, {origin.x + p.x, origin.y + p.y}, c, t);
  });
  ImGui::Dummy(total);
}

void drawSpans(ImDrawList* dl, ImVec2 pos, const std::vector<TextSpan>& spans) {
  ImFont* font = ImGui::GetFont();
  const float size = ImGui::GetFontSize();
  pos = floorVec(pos);
  layoutSpans(spans, 0.0F, [&](ImVec2 p, PaletteColor c, std::string_view t) {
    drawShadowed(dl, font, size, {pos.x + p.x, pos.y + p.y}, c, t);
  });
}

bool PixelButton(const char* label, ImVec2 size, std::string_view hoverMarkup) {
  const auto text = visibleLabel(label);
  ImFont* font = ImGui::GetFont();
  const float fontSize = ImGui::GetFontSize();
  const ImVec2 textSize = floorVec(measure(font, fontSize, text));
  const ImVec2 sz{size.x > 0 ? std::floor(size.x) : textSize.x + 16, size.y > 0 ? std::floor(size.y) : lineHeight() + 8};

  ImGui::SetCursorScreenPos(floorVec(ImGui::GetCursorScreenPos()));
  const bool pressed = ImGui::InvisibleButton(label, sz, ImGuiButtonFlags_EnableNav);
  const bool hovered = ImGui::IsItemHovered();
  const bool held = ImGui::IsItemActive() && hovered;
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();

  const Skin& skin = ctx().skin;
  const auto& slice = held ? skin.buttonPressed : (hovered ? skin.buttonHover : skin.button);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  render::drawNineSlice(dl, slice, min, max, {toU32(PaletteColor::Stone), toU32(PaletteColor::Ink)});

  const float push = held ? 1.0F : 0.0F;
  const ImVec2 textPos{min.x + std::floor((sz.x - textSize.x) / 2) + push,
                       min.y + std::floor((sz.y - fontSize) / 2) + push};
  drawShadowed(dl, font, fontSize, textPos, PaletteColor::TextYellow, text, Surface::Stone);  // buttons are stone

  if (hovered && !hoverMarkup.empty()) setHoverText(std::string(hoverMarkup));
  if (pressed && ctx().onClick) ctx().onClick();
  return pressed;
}

bool Toggle(const char* label, bool* value, std::string_view hoverMarkup) {
  constexpr float kBox = 11.0F;
  const auto text = visibleLabel(label);
  ImFont* font = ImGui::GetFont();
  const float fontSize = ImGui::GetFontSize();
  const ImVec2 textSize = floorVec(measure(font, fontSize, text));
  const float height = std::max(kBox, lineHeight());

  ImGui::SetCursorScreenPos(floorVec(ImGui::GetCursorScreenPos()));
  const bool pressed = ImGui::InvisibleButton(label, {kBox + 6 + textSize.x + 2, height}, ImGuiButtonFlags_EnableNav);
  const bool hovered = ImGui::IsItemHovered();
  const ImVec2 min = ImGui::GetItemRectMin();
  if (pressed) *value = !*value;

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 box{min.x, min.y + std::floor((height - kBox) / 2)};
  const ImVec2 boxMax{box.x + kBox, box.y + kBox};
  dl->AddRectFilled(box, boxMax, toU32(hovered ? PaletteColor::Stone : PaletteColor::StoneDark));
  dl->AddRect(box, boxMax, toU32(PaletteColor::Ink));
  if (*value) {  // a 7x7 pixel tick, drawn pixel by pixel so it stays crisp at any scale
    constexpr int kTick[][2] = {{0, 4}, {1, 5}, {2, 6}, {3, 5}, {4, 4}, {5, 3}, {6, 2}, {7, 1}};
    for (const auto& p : kTick) {
      const float x = box.x + 1 + static_cast<float>(p[0]);
      const float y = box.y + 1 + static_cast<float>(p[1]);
      dl->AddRectFilled({x, y}, {x + 1, y + 2}, toU32(PaletteColor::TextYellow));
    }
  }
  drawShadowed(dl, font, fontSize, {min.x + kBox + 6, min.y + std::floor((height - fontSize) / 2)},
               PaletteColor::TextYellow, text);

  if (hovered && !hoverMarkup.empty()) setHoverText(std::string(hoverMarkup));
  if (pressed && ctx().onClick) ctx().onClick();
  return pressed;
}

StonePanel::StonePanel(const char* id, std::string_view title, ImVec2 size, PanelVariant variant) {
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const ImVec2 sz = floorVec({size.x > 0 ? size.x : avail.x, size.y > 0 ? size.y : avail.y});
  const ImVec2 pos = floorVec(ImGui::GetCursorScreenPos());
  ImDrawList* dl = ImGui::GetWindowDrawList();
  render::drawNineSlice(dl, sliceFor(variant), pos, {pos.x + sz.x, pos.y + sz.y}, fallbackFor(variant));

  constexpr float kPad = 10.0F;
  float top = kPad;
  if (!title.empty()) {
    ImFont* tf = activeFonts().title;
    const float ts = tf->FontSize;
    const ImVec2 tsz = floorVec(measure(tf, ts, title));
    drawShadowed(dl, tf, ts, {pos.x + std::floor((sz.x - tsz.x) / 2), pos.y + 8}, PaletteColor::TextOrange, title,
                 variant == PanelVariant::Parchment ? Surface::Parchment : Surface::Stone);
    top = 8 + std::floor(ts) + 7;
  }

  ImGui::SetCursorScreenPos(pos);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {kPad, top});
  ImGui::BeginChild(id, sz, ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_NavFlattened,
                    ImGuiWindowFlags_NoBackground);
  ImGui::PopStyleVar();
  ctx().surfaces.push_back(variant == PanelVariant::Parchment ? Surface::Parchment : Surface::Stone);
}

StonePanel::~StonePanel() {
  if (!ctx().surfaces.empty()) ctx().surfaces.pop_back();
  ImGui::EndChild();
}

void HoverText(ImVec2 regionMin) {
  const auto& markup = ctx().hoverMarkup;
  if (markup.empty()) return;
  // Hover text always sits over the stone viewport frame.
  ImDrawList* dl = ImGui::GetForegroundDrawList();
  ImFont* font = ImGui::GetFont();
  const float size = ImGui::GetFontSize();
  const ImVec2 pos = floorVec({regionMin.x + 4, regionMin.y + 4});
  layoutSpans(parseMarkup(markup), 0.0F, [&](ImVec2 p, PaletteColor c, std::string_view t) {
    drawShadowed(dl, font, size, {pos.x + p.x, pos.y + p.y}, c, t, Surface::Stone);
  });
}

}  // namespace as::ui
