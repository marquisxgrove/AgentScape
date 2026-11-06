#include "ui/theme.hpp"

namespace as::ui {

void applyTheme(bool kiroween) {
  ImGuiStyle& s = ImGui::GetStyle();
  s = ImGuiStyle{};

  s.AntiAliasedLines = false;
  s.AntiAliasedLinesUseTex = false;
  s.AntiAliasedFill = false;
  s.WindowRounding = s.ChildRounding = s.FrameRounding = s.PopupRounding = 0.0F;
  s.ScrollbarRounding = s.GrabRounding = s.TabRounding = 0.0F;
  s.WindowBorderSize = 0.0F;
  s.ChildBorderSize = 0.0F;
  s.PopupBorderSize = 1.0F;
  s.FrameBorderSize = 1.0F;
  s.WindowPadding = {0.0F, 0.0F};
  s.FramePadding = {4.0F, 2.0F};
  s.ItemSpacing = {4.0F, 2.0F};
  s.ItemInnerSpacing = {4.0F, 4.0F};
  s.ScrollbarSize = 8.0F;
  s.GrabMinSize = 8.0F;
  s.WindowMinSize = {1.0F, 1.0F};

  const ImVec4 clear{0, 0, 0, 0};
  const ImVec4 accent = toVec4(kiroween ? PaletteColor::KiroweenPurple : PaletteColor::StoneDark);
  const ImVec4 highlight = toVec4(kiroween ? PaletteColor::KiroweenPumpkin : PaletteColor::TextYellow);

  ImVec4* c = s.Colors;
  c[ImGuiCol_Text] = toVec4(PaletteColor::TextYellow);
  c[ImGuiCol_TextDisabled] = toVec4(PaletteColor::Parchment, 0.6F);
  c[ImGuiCol_WindowBg] = clear;
  c[ImGuiCol_ChildBg] = clear;
  c[ImGuiCol_PopupBg] = toVec4(PaletteColor::Stone);
  c[ImGuiCol_Border] = toVec4(PaletteColor::Ink);
  c[ImGuiCol_BorderShadow] = clear;
  c[ImGuiCol_FrameBg] = toVec4(PaletteColor::StoneDark);
  c[ImGuiCol_FrameBgHovered] = toVec4(PaletteColor::Stone);
  c[ImGuiCol_FrameBgActive] = accent;
  c[ImGuiCol_CheckMark] = highlight;
  c[ImGuiCol_SliderGrab] = highlight;
  c[ImGuiCol_SliderGrabActive] = toVec4(PaletteColor::TextOrange);
  c[ImGuiCol_Button] = toVec4(PaletteColor::Stone);
  c[ImGuiCol_ButtonHovered] = toVec4(PaletteColor::Stone);
  c[ImGuiCol_ButtonActive] = accent;
  c[ImGuiCol_Header] = accent;
  c[ImGuiCol_HeaderHovered] = toVec4(PaletteColor::Stone);
  c[ImGuiCol_HeaderActive] = accent;
  c[ImGuiCol_Separator] = toVec4(PaletteColor::StoneDark);
  c[ImGuiCol_ScrollbarBg] = toVec4(PaletteColor::StoneDark);
  c[ImGuiCol_ScrollbarGrab] = toVec4(PaletteColor::Stone);
  c[ImGuiCol_ScrollbarGrabHovered] = toVec4(PaletteColor::Parchment, 0.6F);
  c[ImGuiCol_ScrollbarGrabActive] = toVec4(PaletteColor::Parchment);
  c[ImGuiCol_TextSelectedBg] = toVec4(PaletteColor::ChatBlue, 0.6F);
  c[ImGuiCol_NavCursor] = toVec4(PaletteColor::TextYellow);  // 2 px outline, drawn by ImGui
  c[ImGuiCol_ModalWindowDimBg] = toVec4(PaletteColor::Ink, 0.5F);
}

}  // namespace as::ui
