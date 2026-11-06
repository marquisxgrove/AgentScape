#include "screens/dev/ui_gallery_screen.hpp"

#include <imgui.h>

#include <array>
#include <string>

#include "ui/markup.hpp"
#include "ui/ui_context.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

namespace {
struct Swatch {
  ui::PaletteColor color;
  const char* name;
};
constexpr std::array<Swatch, 8> TEXT_COLORS{{
    {ui::PaletteColor::TextYellow, "Yellow"},
    {ui::PaletteColor::TextOrange, "Orange"},
    {ui::PaletteColor::TextCyan, "Cyan"},
    {ui::PaletteColor::TextGreen, "Green"},
    {ui::PaletteColor::TextRed, "Red"},
    {ui::PaletteColor::ChatBlue, "Blue"},
    {ui::PaletteColor::KiroweenPurple, "Purple"},
    {ui::PaletteColor::KiroweenPumpkin, "Pumpkin"},
}};
}  // namespace

void UiGalleryScreen::draw(app::FrameCtx& /*ctx*/) {
  ui::StonePanel panel{"##gallery", "UI Gallery"};

  ui::ShadowText("ShadowText in every palette colour:");
  for (std::size_t i = 0; i < TEXT_COLORS.size(); ++i) {
    if (i % 4 != 0) ImGui::SameLine(0, 12);
    ui::ShadowText(TEXT_COLORS[i].name, TEXT_COLORS[i].color);
  }

  ImGui::Spacing();
  ui::ShadowText("RichText markup:");
  ui::RichText("Enter <col=cyan>Grand Codex</col> / 3 more options");
  ui::RichText("<col=green>Congratulations, you just advanced a level!</col>");
  // User text is escaped, so it can't inject colours.
  const std::string sneaky = "Evil <col=red>Agent</col>";
  ui::RichText("Escaped name: <col=cyan>" + ui::escapeMarkup(sneaky) + "</col>");

  ImGui::Spacing();
  ui::ShadowText("PixelButton (hover me, Tab/arrows to focus):");
  if (ui::PixelButton("Play as Guest", {0, 0}, ui::hoverText("Play", "as Guest", ui::PaletteColor::TextCyan, 0))) {
    ++clicks_;
  }
  ImGui::SameLine();
  if (ui::PixelButton("Examine##gallery", {0, 0}, ui::hoverText("Examine", "Button", ui::PaletteColor::TextCyan, 1))) {
    ++clicks_;
  }
  ImGui::SameLine();
  ui::PixelButton("Wide button##wide", {140, 0});
  ImGui::SameLine();
  ui::ShadowText("Clicks: " + std::to_string(clicks_), ui::PaletteColor::TextGreen);

  ImGui::Spacing();
  ui::Toggle("Plain font", &ui::ctx().plainFont);
  ImGui::SameLine(0, 12);
  static bool sampleOff = false;
  ui::Toggle("Another toggle", &sampleOff);
  ImGui::SameLine(0, 12);
  ImGui::PushFont(ui::ctx().fonts.plain.body);
  ui::ShadowText("Plain font preview: The quick brown familiar jumps over 13 lazy bugs.");
  ImGui::PopFont();

  ImGui::Spacing();
  {
    ui::StonePanel scroll{"##parchment", "Parchment", {0, 0}, ui::PanelVariant::Parchment};
    ui::RichText(
        "<col=orange>Quest Log:</col> The familiar refactored my parser in one go, but it invented a flag "
        "that never existed. Wrapped text keeps whole words together on each line.",
        ui::PaletteColor::TextYellow, ImGui::GetContentRegionAvail().x);
  }
}

}  // namespace as::screens
