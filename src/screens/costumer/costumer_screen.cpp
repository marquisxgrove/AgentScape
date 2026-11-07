#include "screens/costumer/costumer_screen.hpp"

#include <imgui.h>

#include <cmath>
#include <cstdint>

#include "app/context_request.hpp"
#include "app/services.hpp"
#include "domain/palette.hpp"
#include "domain/wardrobe.hpp"
#include "game/avatar_renderer.hpp"
#include "ui/chatbox.hpp"
#include "ui/theme.hpp"
#include "ui/ui_context.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

namespace {
using domain::ColorRegion;
using domain::Facing;
using domain::Slot;
using ui::StyleOption;

constexpr std::array<std::pair<Slot, const char*>, 9> SLOTS{{{Slot::Head, "Head"},
                                                             {Slot::Cape, "Cape"},
                                                             {Slot::Neck, "Neck"},
                                                             {Slot::Weapon, "Weapon"},
                                                             {Slot::Body, "Body"},
                                                             {Slot::Shield, "Shield"},
                                                             {Slot::Legs, "Legs"},
                                                             {Slot::Hands, "Hands"},
                                                             {Slot::Feet, "Feet"}}};

ImU32 swatchColor(ColorRegion region, int choice) {
  const auto s = domain::swatchFor(region, choice);
  return IM_COL32(s.r, s.g, s.b, 255);
}
}  // namespace

CostumerScreen::CostumerScreen() = default;

void CostumerScreen::onEnter(const app::ScreenParams& params) {
  loaded_ = false;
  if (const auto it = params.find("tab"); it != params.end()) {  // dev QA: --screen=costumer-gear etc.
    tab_ = it->second == "gear" ? Tab::Gear : (it->second == "adventurers" ? Tab::Adventurers : Tab::Look);
  }
}

void CostumerScreen::reload(app::FrameCtx& ctx) {
  loaded_ = true;
  active_.reset();
  list_.clear();
  if (ctx.services == nullptr) return;
  if (auto list = ctx.services->adventurers.list(ctx.userId)) list_ = *list;
  if (auto active = ctx.services->adventurers.getActive(ctx.userId); active && *active) {
    active_ = **active;
    auto owned = ctx.services->adventurers.ownedItems(ctx.userId);
    draft_.load(active_->appearance, active_->loadout, owned ? *owned : domain::defaultOwnedSet());
    if (active_->repaired) ui::pushGameMessage("Some of your gear has crumbled to dust.");
  }
}

void CostumerScreen::draw(app::FrameCtx& ctx) {
  if (!loaded_) reload(ctx);
  clock_ += ctx.dt;

  ui::StonePanel panel{"##costumer", "The Costumer"};
  if (!active_) {
    drawCreate(ctx);  // no way out until the first Adventurer exists (Req 2.6)
    return;
  }
  if (ui::PixelButton("Back", {70, 0}) && ctx.nav != nullptr) ctx.nav->back();

  // Left: the live preview. Right: the tabbed controls, with the save bar pinned under them. The
  // columns stop short of the panel bottom so the bar always fits (a full-height child pushed it out).
  const float barH = tab_ != Tab::Adventurers ? ui::ButtonHeight() + ImGui::GetStyle().ItemSpacing.y : 0.0F;
  ImGui::BeginChild("##preview", {150, 0});
  drawPreview(ctx);
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::BeginChild("##controls", {0, -barH});
  if (ui::TabButton("Look", tab_ == Tab::Look, {90, 0})) tab_ = Tab::Look;
  ImGui::SameLine();
  if (ui::TabButton("Gear", tab_ == Tab::Gear, {90, 0})) tab_ = Tab::Gear;
  ImGui::SameLine();
  if (ui::TabButton("Adventurers", tab_ == Tab::Adventurers, {130, 0})) tab_ = Tab::Adventurers;
  ImGui::Spacing();
  switch (tab_) {
    case Tab::Look: drawLook(); break;
    case Tab::Gear: drawGear(ctx); break;
    case Tab::Adventurers: drawAdventurers(ctx); break;
  }
  ImGui::EndChild();

  // Save bar (shared by Look and Gear).
  if (tab_ != Tab::Adventurers) {
    if (draft_.dirty()) {
      if (ui::PixelButton("Save", {90, 0})) save(ctx);
      ImGui::SameLine();
      ui::AlignTextToButton();
      ui::ShadowText("Unsaved changes", ui::PaletteColor::TextOrange);
    } else {
      ui::AlignTextToButton();
      ui::ShadowText("All changes saved.", ui::PaletteColor::Parchment);
    }
  }
  ImGui::EndGroup();
}

void CostumerScreen::drawCreate(app::FrameCtx& ctx) {
  ui::ShadowText("Name your first Adventurer:", ui::PaletteColor::Parchment);
  const float rowY = ImGui::GetCursorPosY();  // the box is nudged to centre on the button; the button isn't
  ImGui::SetNextItemWidth(160);
  ui::AlignFrameToButton();
  const bool entered =
      ImGui::InputText("##name", nameBuf_.data(), nameBuf_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  ImGui::SetCursorPosY(rowY);
  if (ui::PixelButton("Create", {100, 0}) || entered) create(ctx);
  ui::ShadowText("Up to 12 letters or numbers; single spaces, - or _ between them.", ui::PaletteColor::Parchment);
  ui::ShadowText("You can change their look and gear once they exist.", ui::PaletteColor::Parchment);
}

void CostumerScreen::drawPreview(app::FrameCtx& ctx) {
  const int idle = ctx.reduceMotion ? 0 : static_cast<int>(clock_ * 2.0) % 2;  // 2-frame idle (Req 3.3)
  if (ctx.avatars != nullptr) {
    SDL_Texture* tex = ctx.avatars->texture(draft_.appearance(), draft_.loadout(), draft_.facing(), idle);
    const float scale = 3.0F;
    const ImVec2 size{static_cast<float>(ctx.avatars->cellWidth()) * scale,
                      static_cast<float>(ctx.avatars->cellHeight()) * scale};
    if (tex != nullptr) {
      ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(tex)), size);
    } else {
      ImGui::Dummy(size);
    }
  }
  if (ui::PixelButton("<##rotl", {36, 0}, ui::hoverText("Rotate", "left", ui::PaletteColor::TextCyan, 0))) {
    draft_.rotate(-1);
  }
  ImGui::SameLine();
  if (ui::PixelButton(">##rotr", {36, 0}, ui::hoverText("Rotate", "right", ui::PaletteColor::TextCyan, 0))) {
    draft_.rotate(1);
  }
  ImGui::SameLine();
  ui::AlignTextToButton();
  ui::Toggle("Describe", &describe_);
  if (ctx.exportPortrait && ui::PixelButton("Export", {78, 0})) {
    ctx.exportPortrait(draft_.appearance(), draft_.loadout(), draft_.facing(), active_->name);
  }
  if (describe_) {
    ui::StonePanel scroll{"##describe", "", {0, 110}, ui::PanelVariant::Parchment};
    ui::RichText(ui::escapeMarkup(domain::describeAvatar(draft_.appearance(), draft_.loadout())),
                 ui::PaletteColor::TextYellow, ImGui::GetContentRegionAvail().x);
  }
}

void CostumerScreen::drawLook() {
  const auto arrows = [&](const char* label, StyleOption o) {
    ImGui::PushID(label);
    if (ui::PixelButton("<", {28, 0})) draft_.cycle(o, -1);
    ImGui::SameLine();
    ui::AlignTextToButton();
    ui::ShadowText(label + std::string(" ") + std::to_string(draft_.value(o) + 1) + "/" +
                   std::to_string(draft_.count(o)));
    ImGui::SameLine(150);
    if (ui::PixelButton(">", {28, 0})) draft_.cycle(o, 1);
    ImGui::PopID();
  };
  arrows("Body", StyleOption::BodyType);
  arrows("Hair", StyleOption::Hair);
  arrows("Beard", StyleOption::FacialHair);
  arrows("Torso", StyleOption::Torso);
  arrows("Arms", StyleOption::Arms);
  arrows("Legs", StyleOption::Legs);

  ImGui::Spacing();
  // One row per region: label, then a 12px swatch per colour; the chosen one gets a yellow frame.
  const auto swatches = [&](const char* label, ColorRegion region) {
    ui::ShadowText(label, ui::PaletteColor::Parchment);
    ImGui::SameLine(64);
    for (int i = 0; i < domain::colorCount(region); ++i) {
      ImGui::PushID(label);
      ImGui::PushID(i);
      ImGui::PushStyleColor(ImGuiCol_Button, swatchColor(region, i));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, swatchColor(region, i));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, swatchColor(region, i));
      if (ImGui::Button("", {12, 12})) draft_.setColor(region, i);
      ImGui::PopStyleColor(3);
      if (draft_.color(region) == i) {
        const ImVec2 lo = ImGui::GetItemRectMin();
        const ImVec2 hi = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRect({lo.x - 1, lo.y - 1}, {hi.x + 1, hi.y + 1},
                                            ui::toU32(ui::PaletteColor::TextYellow));
      }
      if (ImGui::IsItemHovered()) ui::setHoverText(std::string(domain::swatchFor(region, i).name));
      ImGui::PopID();
      ImGui::PopID();
      if (i + 1 < domain::colorCount(region)) ImGui::SameLine(0, 3);
    }
  };
  swatches("Skin", ColorRegion::Skin);
  swatches("Hair", ColorRegion::Hair);
  swatches("Shirt", ColorRegion::Torso);
  swatches("Legs", ColorRegion::Legs);
  swatches("Shoes", ColorRegion::Feet);

  ImGui::Spacing();
  if (ui::PixelButton("Randomise", {110, 0})) draft_.randomise(rng_);
}

void CostumerScreen::drawGear(app::FrameCtx& ctx) {
  // The nine paper-doll slots: click an occupied slot for Remove / Examine.
  ui::ShadowText("Equipped (click a slot):", ui::PaletteColor::Parchment);
  // Two equal columns, measured once (after SameLine the space left on the line is smaller).
  const float slotW = std::floor((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0F);
  for (std::size_t i = 0; i < SLOTS.size(); ++i) {
    const auto [slot, name] = SLOTS[i];
    const auto it = draft_.loadout().find(slot);
    const bool filled = it != draft_.loadout().end();
    const domain::Item* item = filled ? domain::findItem(it->second) : nullptr;
    // A worn item shows its name (the slot is in the hover text, so long names fit); an empty slot shows the slot.
    const std::string label = (item ? std::string(item->name) : std::string(name) + ": -") + "##slot" + std::to_string(i);
    const std::string hover = std::string(name) + ": " + (item ? std::string(item->name) : "empty");
    if (ui::PixelButton(label.c_str(), {slotW, 0}, ui::escapeMarkup(hover)) && filled && ctx.contextRequest != nullptr) {
      std::vector<domain::MenuAction> actions{{"remove:" + std::string(name), "Remove", std::string(name), true}};
      const ImVec2 at = ImGui::GetItemRectMin();
      ctx.contextRequest->open(actions, std::string(item->examine), at.x + 8, at.y + 8);
    }
    if ((i + 1) % 2 != 0) ImGui::SameLine();
  }
  ImGui::NewLine();
  ImGui::Separator();

  // Wardrobe filters.
  ui::AlignTextToButton();
  ui::Toggle("Owned only", &ownedOnly_);
  ImGui::SameLine();
  const std::string filterLabel = std::string("Slot: ") + (slotFilter_ ? domain::slotName(*slotFilter_) : "All");
  if (ui::PixelButton((filterLabel + "##slotfilter").c_str(), {120, 0})) {
    // All -> Head -> Cape -> ... -> Feet -> All
    if (!slotFilter_) {
      slotFilter_ = Slot::Head;
    } else if (*slotFilter_ == Slot::Feet) {
      slotFilter_.reset();
    } else {
      slotFilter_ = static_cast<Slot>(static_cast<int>(*slotFilter_) + 1);
    }
  }

  ImGui::BeginChild("##wardrobe", {0, 0});
  const auto entries = domain::wardrobe(draft_.loadout(), draft_.owned(), ctx.kiroween, ctx.userLevel, slotFilter_,
                                        ownedOnly_);
  domain::Slot lastSlot = domain::Slot::Head;
  bool first = true;
  for (const auto& e : entries) {
    if (first || e.item->slot != lastSlot) {
      ui::ShadowText(domain::slotName(e.item->slot), ui::PaletteColor::TextOrange);
      lastSlot = e.item->slot;
      first = false;
    }
    const auto color = domain::isKiroween(*e.item) ? ui::PaletteColor::KiroweenPumpkin : ui::PaletteColor::TextCyan;
    using S = domain::ItemStatus;
    ImGui::PushID(e.item->id.data(), e.item->id.data() + e.item->id.size());
    switch (e.status) {
      case S::Equipped:
        ui::ShadowText(std::string(e.item->name) + "  (worn)", ui::PaletteColor::TextGreen);
        break;
      case S::Owned:
        if (ui::PixelButton((std::string("Wear##") + std::string(e.item->id)).c_str(), {70, 0})) {
          if (auto err = draft_.equip(e.item->id)) ui::pushGameMessage(*err);
        }
        ImGui::SameLine();
        ui::AlignTextToButton();
        ui::ShadowText(std::string(e.item->name), color);
        break;
      case S::Claimable:
        if (ui::PixelButton((std::string("Claim##") + std::string(e.item->id)).c_str(), {70, 0})) claim(ctx, *e.item);
        ImGui::SameLine();
        ui::AlignTextToButton();
        ui::ShadowText(std::string(e.item->name), color);
        break;
      default:
        ui::ShadowText(std::string(e.item->name) + "  (" + e.requirement + ")", ui::PaletteColor::Parchment);
        break;
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
}

void CostumerScreen::drawAdventurers(app::FrameCtx& ctx) {
  ui::ShadowText("Your Adventurers (" + std::to_string(list_.size()) + "/5):", ui::PaletteColor::Parchment);
  for (const auto& a : list_) {
    ImGui::PushID(a.id.c_str());
    if (a.isActive) {
      ui::AlignTextToButton();
      ui::ShadowText(a.name + "  (active)", ui::PaletteColor::TextGreen);
    } else {
      if (ui::PixelButton(("Play##" + a.id).c_str(), {70, 0})) {
        if (draft_.dirty()) {
          ui::pushGameMessage("Save your changes before switching Adventurers.");  // never lose edits silently
        } else if (ctx.services->adventurers.setActiveAdventurer(ctx.userId, a.id)) {
          reload(ctx);
        }
      }
      ImGui::SameLine();
      ui::AlignTextToButton();
      ui::ShadowText(a.name, ui::PaletteColor::TextCyan);
    }
    ImGui::SameLine(260);
    if (pendingDeleteId_ == a.id) {
      ImGui::SetNextItemWidth(110);
      ImGui::InputText("##confirm", confirmBuf_.data(), confirmBuf_.size());
      ImGui::SameLine();
      if (ui::PixelButton(("Confirm##" + a.id).c_str(), {80, 0})) {
        auto r = ctx.services->adventurers.deleteAdventurer(ctx.userId, a.id, confirmBuf_.data());
        if (r) {
          pendingDeleteId_.clear();
          reload(ctx);
          ImGui::PopID();
          break;
        }
        ui::pushGameMessage(r.error().message);
      }
    } else if (ui::PixelButton(("Delete##" + a.id).c_str(), {70, 0})) {
      pendingDeleteId_ = a.id;
      confirmBuf_.fill('\0');
    }
    ImGui::PopID();
  }
  if (!pendingDeleteId_.empty()) {
    ui::RichText("<col=orange>Type the Adventurer's name to confirm deletion.</col>");
  }
  ImGui::Spacing();
  if (static_cast<int>(list_.size()) < services::MAX_ADVENTURERS) {
    ui::ShadowText("New Adventurer:", ui::PaletteColor::Parchment);
    const float rowY = ImGui::GetCursorPosY();
    ImGui::SetNextItemWidth(160);
    ui::AlignFrameToButton();
    const bool entered =
        ImGui::InputText("##newname", nameBuf_.data(), nameBuf_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::SetCursorPosY(rowY);
    if (ui::PixelButton("Create", {90, 0}) || entered) create(ctx);
  }
}

void CostumerScreen::onContextAction(app::FrameCtx&, const domain::MenuAction& action) {
  if (action.id.rfind("remove:", 0) == 0) {
    for (const auto& [slot, name] : SLOTS) {
      if (action.id == "remove:" + std::string(name)) {
        draft_.unequip(slot);
        return;
      }
    }
  }
}

void CostumerScreen::save(app::FrameCtx& ctx) {
  auto r = ctx.services->adventurers.saveAdventurer(ctx.userId, active_->id, draft_.appearance(), draft_.loadout());
  if (r) {
    active_ = *r;
    draft_.markSaved();
    ui::pushGameMessage("Your Adventurer has been saved.");
  } else {
    ui::pushGameMessage(r.error().message);
  }
}

void CostumerScreen::create(app::FrameCtx& ctx) {
  const std::string name = nameBuf_.data();
  auto r = ctx.services->adventurers.createAdventurer(ctx.userId, name);
  if (r) {
    nameBuf_.fill('\0');
    reload(ctx);
  } else {
    ui::pushGameMessage(r.error().message);
  }
}

void CostumerScreen::claim(app::FrameCtx& ctx, const domain::Item& item) {
  auto r = ctx.services->adventurers.claimKiroweenItem(ctx.userId, item.id, ctx.kiroween, ctx.userLevel);
  if (r) {
    if (auto owned = ctx.services->adventurers.ownedItems(ctx.userId)) draft_.setOwned(*owned);
    ui::pushGameMessage(std::string("You claimed the ") + std::string(item.name) + "!");
  } else {
    ui::pushGameMessage(r.error().message);
  }
}

}  // namespace as::screens
