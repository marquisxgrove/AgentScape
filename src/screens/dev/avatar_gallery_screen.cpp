#include "screens/dev/avatar_gallery_screen.hpp"

#include <imgui.h>

#include <cstdint>
#include <functional>
#include <string>

#include "domain/items.hpp"
#include "domain/loadout.hpp"
#include "game/avatar_renderer.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

namespace {
using domain::Appearance;
using domain::Facing;
using domain::Loadout;

constexpr Facing FACINGS[] = {Facing::S, Facing::E, Facing::N, Facing::W};

Appearance sampleLook() {
  Appearance a;
  a.hair = 2;
  a.colors = {3, 5, 4, 2, 1};
  return a;
}

// Draws one sprite at the cursor at an integer scale and advances the layout (SameLine-friendly).
void sprite(game::AvatarRenderer& r, const Appearance& a, const Loadout& l, Facing f, int frame, float scale) {
  SDL_Texture* tex = r.texture(a, l, f, frame);
  const ImVec2 size{static_cast<float>(r.cellWidth()) * scale, static_cast<float>(r.cellHeight()) * scale};
  if (tex != nullptr) {
    ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(tex)), size);
  } else {
    ImGui::Dummy(size);
  }
}

void facingRow(game::AvatarRenderer& r, const Appearance& a, const Loadout& l, int frame, float scale) {
  for (std::size_t i = 0; i < 4; ++i) {
    if (i != 0) ImGui::SameLine(0, 2);
    sprite(r, a, l, FACINGS[i], frame, scale);
  }
}

void styleRow(game::AvatarRenderer& r, const char* label, int count, int first, const Loadout& gear,
              Facing facing, const std::function<void(Appearance&, int)>& set) {
  ui::ShadowText(label, ui::PaletteColor::TextOrange);
  for (int i = first; i < count; ++i) {
    Appearance a = sampleLook();
    set(a, i);
    if (i != first) ImGui::SameLine(0, 2);
    sprite(r, a, gear, facing, 0, 1.0F);
  }
}
}  // namespace

void AvatarGalleryScreen::onEnter(const app::ScreenParams& params) {
  const auto it = params.find("page");
  if (it != params.end() && it->second == "looks") page_ = Page::Looks;
}

void AvatarGalleryScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##avatars", "Avatar Gallery"};
  if (ctx.avatars == nullptr) {
    ui::ShadowText("The avatar atlas could not be loaded.", ui::PaletteColor::TextRed);
    return;
  }
  game::AvatarRenderer& r = *ctx.avatars;
  clock_ += ctx.dt;
  const int idle = ctx.reduceMotion ? 0 : static_cast<int>(clock_ * 2.0) % 2;  // 2-frame idle (Req 3.3)

  if (ui::PixelButton("Items", {70, 0})) page_ = Page::Items;
  ImGui::SameLine();
  if (ui::PixelButton("Looks", {70, 0})) page_ = Page::Looks;
  ImGui::SameLine();
  ui::ShadowText("A = body type A, B = body type B; S E N W facings", ui::PaletteColor::Parchment);

  ImGui::BeginChild("##gallery-scroll", {0, 0});
  const Loadout bare = domain::defaultLoadout();

  if (page_ == Page::Items) {
    for (const auto& item : domain::ITEMS()) {
      if (item.sprite.empty()) continue;  // base-layer defaults have no overlay of their own
      Loadout gear = bare;
      gear[item.slot] = std::string(item.id);
      if (item.slot == domain::Slot::Shield) gear.erase(domain::Slot::Weapon);  // keep two-handed rule tidy
      ui::ShadowText(std::string(item.name) + (item.twoHanded ? "  (two-handed)" : ""),
                     domain::isKiroween(item) ? ui::PaletteColor::KiroweenPumpkin : ui::PaletteColor::TextCyan);
      for (const auto body : {domain::BodyType::A, domain::BodyType::B}) {
        Appearance a = sampleLook();
        a.bodyType = body;
        facingRow(r, a, gear, idle, 1.0F);
        ImGui::SameLine(0, 14);
      }
      ImGui::NewLine();
    }
  } else {
    ui::ShadowText("Default look, idle frame 0 then 1 (2x)", ui::PaletteColor::TextOrange);
    for (const int frame : {0, 1}) facingRow(r, sampleLook(), bare, frame, 2.0F);
    styleRow(r, "Hair (front)", domain::APPEARANCE_LIMITS.hair, 0, bare, Facing::S, [](Appearance& a, int i) { a.hair = i; });
    styleRow(r, "Hair (back)", domain::APPEARANCE_LIMITS.hair, 0, bare, Facing::N, [](Appearance& a, int i) { a.hair = i; });
    styleRow(r, "Facial hair", domain::APPEARANCE_LIMITS.facialHair, 1, bare, Facing::S, [](Appearance& a, int i) { a.facialHair = i; });
    styleRow(r, "Torso styles", domain::APPEARANCE_LIMITS.torso, 0, bare, Facing::S, [](Appearance& a, int i) { a.torso = i; });
    styleRow(r, "Arm styles", domain::APPEARANCE_LIMITS.arms, 0, bare, Facing::S, [](Appearance& a, int i) { a.arms = i; });
    styleRow(r, "Leg styles", domain::APPEARANCE_LIMITS.legs, 0, bare, Facing::S, [](Appearance& a, int i) { a.legs = i; });
    styleRow(r, "Skin tones", domain::APPEARANCE_LIMITS.colors.skin, 0, bare, Facing::S, [](Appearance& a, int i) { a.colors.skin = i; });
    styleRow(r, "Hair colours", domain::APPEARANCE_LIMITS.colors.hair, 0, bare, Facing::S, [](Appearance& a, int i) { a.colors.hair = i; });
    styleRow(r, "Torso colours", domain::APPEARANCE_LIMITS.colors.torso, 0, bare, Facing::S, [](Appearance& a, int i) { a.colors.torso = i; });
  }
  ImGui::EndChild();
}

}  // namespace as::screens
