#include "screens/codex/codex_widgets.hpp"

#include <cstdint>

#include "platform/paths.hpp"
#include "render/texture_cache.hpp"
#include "ui/theme.hpp"
#include "ui/ui_context.hpp"
#include "ui/widgets.hpp"

namespace as::screens::codex {

bool flowButton(const std::string& text, const char* id, bool& first) {
  const float w = ImGui::CalcTextSize(text.c_str()).x + 20.0F;
  if (!first) {
    const float lastRight = ImGui::GetItemRectMax().x;
    const float limit = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - 12.0F;
    if (lastRight + 6.0F + w <= limit) ImGui::SameLine(0, 6.0F);
  }
  first = false;
  return ui::PixelButton((text + "##" + id).c_str(), {w, 0});
}

void skillIcon(int index) {
  constexpr float SIZE = 11.0F;
  constexpr float STRIP_W = 110.0F;
  const auto tex = ui::ctx().mapTextures().load(platform::assetsDir() / "ui" / "skill_icons.png");
  if (!tex || index < 0 || index > 9) {
    ImGui::Dummy({SIZE, SIZE});
    return;
  }
  const float u0 = static_cast<float>(index) * SIZE / STRIP_W;
  const float u1 = static_cast<float>(index + 1) * SIZE / STRIP_W;
  ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(tex.tex)), {SIZE, SIZE}, {u0, 0}, {u1, 1});
}

void chip(const char* label, ImU32 fill, ImU32 text) {
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const ImVec2 sz = ImGui::CalcTextSize(label);
  const ImVec2 box{sz.x + 8.0F, sz.y + 2.0F};
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(pos, {pos.x + box.x, pos.y + box.y}, fill);
  dl->AddRect(pos, {pos.x + box.x, pos.y + box.y}, ui::toU32(ui::PaletteColor::Ink));
  dl->AddText({pos.x + 4.0F, pos.y + 1.0F}, text, label);
  ImGui::Dummy(box);
}

std::string levelText(const std::optional<int>& level) { return level ? "Lv " + std::to_string(*level) : "-"; }

int cycleNext(int current, int count) {
  const int next = current + 1;
  return next >= count ? -1 : next;
}

}  // namespace as::screens::codex
