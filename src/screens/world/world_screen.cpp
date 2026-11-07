#include "screens/world/world_screen.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include "app/context_request.hpp"
#include "app/services.hpp"
#include "domain/coding_session.hpp"
#include "game/avatar_renderer.hpp"
#include "platform/paths.hpp"
#include "render/texture_cache.hpp"
#include "ui/chatbox.hpp"
#include "ui/layout.hpp"
#include "ui/markup.hpp"
#include "ui/theme.hpp"
#include "ui/ui_context.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

namespace {
constexpr float MAP_W = 380;
constexpr float MAP_H = 300;
constexpr float LIST_GAP = 8;
}  // namespace

void WorldScreen::onEnter(const app::ScreenParams&) {
  adventurerLoaded_ = false;
  codingRefreshIn_ = 0;
}

void WorldScreen::activate(app::FrameCtx& ctx, const game::Hotspot& h) {
  // Screens that exist open; the rest are narrated until their own specs land.
  if ((h.screen == app::ScreenId::Costumer || h.screen == app::ScreenId::Codex || h.screen == app::ScreenId::Graveyard || h.screen == app::ScreenId::Tavern || h.screen == app::ScreenId::Hiscores || h.screen == app::ScreenId::Wilds) && ctx.nav != nullptr) {
    ctx.nav->navigate(h.screen);
    return;
  }
  ui::pushGameMessage(std::string(h.defaultAction) + " " + std::string(h.name) + "...");
}

void WorldScreen::onContextAction(app::FrameCtx& ctx, const domain::MenuAction& action) {
  for (const auto* h : game::visibleHotspots(ctx.kiroween)) {
    if (action.id == h->id) activate(ctx, *h);  // the menu's default action is the hotspot id
  }
}

void WorldScreen::openMenu(app::FrameCtx& ctx, const game::Hotspot& h, float frameX, float frameY) {
  if (ctx.contextRequest == nullptr) return;
  std::vector<domain::MenuAction> actions{
      {std::string(h.id), std::string(h.defaultAction) + " " + std::string(h.name), std::string(h.id), true}};
  ctx.contextRequest->open(actions, std::string(h.examine), frameX, frameY);
}

void WorldScreen::draw(app::FrameCtx& ctx) {
  const auto visible = game::visibleHotspots(ctx.kiroween);

  // --- The map image, drawn into the top-left of the viewport. -----------------------------------
  const ImVec2 mapOrigin = ImGui::GetCursorScreenPos();  // viewport content origin (frame pixels)
  ImDrawList* dl = ImGui::GetWindowDrawList();
  auto& textures = ui::ctx().mapTextures();
  const auto tex = textures.load(platform::assetsDir() / "sprites" / "promptmoor.png");
  if (tex) {
    const auto id = static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(tex.tex));
    dl->AddImage(id, mapOrigin, {mapOrigin.x + MAP_W, mapOrigin.y + MAP_H});
  } else {
    dl->AddRectFilled(mapOrigin, {mapOrigin.x + MAP_W, mapOrigin.y + MAP_H}, ui::toU32(ui::PaletteColor::Stone));
  }

  // The active Adventurer stands in the town square (Req 3.5).
  clock_ += ctx.dt;
  if (!adventurerLoaded_ && ctx.services != nullptr) {
    adventurerLoaded_ = true;
    adventurer_.reset();
    if (auto active = ctx.services->adventurers.getActive(ctx.userId); active && *active) adventurer_ = **active;
  }
  if (adventurer_ && ctx.avatars != nullptr) {
    const int idle = ctx.reduceMotion ? 0 : static_cast<int>(clock_ * 2.0) % 2;
    if (SDL_Texture* sprite = ctx.avatars->texture(adventurer_->appearance, adventurer_->loadout, domain::Facing::S, idle)) {
      const ImVec2 at{mapOrigin.x + 174, mapOrigin.y + 172};
      const auto spriteId = static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(sprite));
      dl->AddImage(spriteId, at, {at.x + static_cast<float>(ctx.avatars->cellWidth()),
                            at.y + static_cast<float>(ctx.avatars->cellHeight())});
    }
  }

  // Kiroween overlays: jack-o'-lanterns at the doorways, drifting fog over the whole map.
  if (ctx.kiroween) {
    if (!kiroweenInit_) {
      kiroween_.initWorld({{128, 128}, {296, 128}, {60, 118}, {340, 118}});
      kiroweenInit_ = true;
    }
    kiroween_.step(ctx.dt, ctx.reduceMotion);
    kiroween_.drawPumpkins(dl, textures, mapOrigin);
    kiroween_.drawFog(dl, textures, mapOrigin, {mapOrigin.x + MAP_W, mapOrigin.y + MAP_H});
  }

  // Invisible button over the map captures clicks and hover without stealing nav from the list.
  ImGui::InvisibleButton("##map", {MAP_W, MAP_H}, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  const bool mapHovered = ImGui::IsItemHovered();
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const domain::Point local{mouse.x - mapOrigin.x, mouse.y - mapOrigin.y};
  const game::Hotspot* hovered = mapHovered ? game::hotspotAt(local, ctx.kiroween) : nullptr;

  if (hovered != nullptr) {
    // Outline the hovered footprint and set hover text.
    const auto bb = domain::boundingBox(hovered->polygon);
    dl->AddRect({mapOrigin.x + bb.x, mapOrigin.y + bb.y}, {mapOrigin.x + bb.x + bb.w, mapOrigin.y + bb.y + bb.h},
                ui::toU32(ui::PaletteColor::TextYellow));
    ui::setHoverText(ui::hoverText(std::string(hovered->defaultAction), std::string(hovered->name),
                                   ui::PaletteColor::TextCyan, /*moreOptions=*/1));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) activate(ctx, *hovered);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) openMenu(ctx, *hovered, mouse.x, mouse.y);
  }

  // --- The accessible hotspot list, in the right column of the viewport (keyboard/gamepad). ------
  const float listX = mapOrigin.x + MAP_W + LIST_GAP;
  const float listW = static_cast<float>(ui::VIEWPORT.w) - MAP_W - LIST_GAP - 12;
  ImGui::SetCursorScreenPos({listX, mapOrigin.y});
  ui::ShadowText("Places:", ui::PaletteColor::Parchment);
  if (listFocus_ >= static_cast<int>(visible.size())) listFocus_ = 0;
  for (std::size_t i = 0; i < visible.size(); ++i) {
    const auto& h = *visible[i];
    ImGui::SetCursorPosX(ImGui::GetCursorStartPos().x + MAP_W + LIST_GAP);
    const std::string label = std::string(h.name) + "##list-" + std::string(h.id);
    const std::string hover = ui::hoverText(std::string(h.defaultAction), std::string(h.name),
                                            ui::PaletteColor::TextCyan, 1);
    if (ui::PixelButton(label.c_str(), {listW, 0}, hover)) activate(ctx, h);
    if (ImGui::IsItemFocused()) {
      listFocus_ = static_cast<int>(i);
      // Menu key / Shift+F10 / gamepad Y opens the context menu at the button.
      if (ImGui::IsKeyPressed(ImGuiKey_Menu) ||
          (ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_F10)) ||
          ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp)) {
        const ImVec2 at = ImGui::GetItemRectMin();
        openMenu(ctx, h, at.x + 8, at.y + 8);
      }
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
      const ImVec2 at = ImGui::GetItemRectMin();
      openMenu(ctx, h, at.x + 8, at.y + 8);
    }
  }

  // A running coding session, under the Places list (coding-sessions Requirement 3.5).
  codingRefreshIn_ -= ctx.dt;
  if (codingRefreshIn_ <= 0 && ctx.services != nullptr) {
    codingRefreshIn_ = 5;
    coding_.reset();
    if (auto r = ctx.services->coding.running(ctx.userId); r && *r) coding_ = **r;
  }
  if (coding_) {
    const auto elapsed = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()) - coding_->startedAt;
    // Two short lines, wrapped to the column: familiar names can be long.
    ImGui::Spacing();
    ImGui::SetCursorPosX(ImGui::GetCursorStartPos().x + MAP_W + LIST_GAP);
    ui::ShadowText("Coding: " + domain::formatSessionDuration(std::min(elapsed, std::chrono::seconds{domain::CODING_SESSION_MAX})),
                   ui::PaletteColor::TextGreen);
    ImGui::SetCursorPosX(ImGui::GetCursorStartPos().x + MAP_W + LIST_GAP);
    ui::RichText(ui::escapeMarkup(coding_->solo() ? std::string("solo, in the Wilds") : "with " + coding_->agentName),
                 ui::PaletteColor::TextCyan, listW);
  }
}

}  // namespace as::screens
