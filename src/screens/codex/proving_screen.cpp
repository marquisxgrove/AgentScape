#include "screens/codex/proving_screen.hpp"

#include <imgui.h>

#include <algorithm>

#include "app/services.hpp"
#include "screens/codex/codex_widgets.hpp"
#include "ui/chatbox.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

void ProvingScreen::onEnter(const app::ScreenParams& params) {
  loaded_ = false;
  addOnEnter_.reset();
  scopeOnEnter_.reset();
  if (const auto it = params.find("add"); it != params.end()) addOnEnter_ = it->second;       // "Compare" on a card
  if (const auto it = params.find("scope"); it != params.end()) scopeOnEnter_ = it->second;
  if (const auto it = params.find("pick"); it != params.end()) picking_ = std::stoi(it->second);   // dev QA
  if (const auto it = params.find("with"); it != params.end()) {                                  // dev QA: --screen=proving
    sel_.slugs.clear();
    std::string rest = it->second;
    for (std::size_t p; (p = rest.find(',')) != std::string::npos; rest.erase(0, p + 1)) sel_.slugs.push_back(rest.substr(0, p));
    sel_.slugs.push_back(rest);
    devSelection_ = true;
  }
}

void ProvingScreen::load(app::FrameCtx& ctx) {
  loaded_ = true;
  dirty_ = true;
  if (ctx.services == nullptr) return;
  if (auto agents = ctx.services->codex.approvedAgents()) agents_ = std::move(*agents);
  if (!devSelection_) {
    if (auto restored = ctx.services->proving.restore(ctx.userId)) sel_ = std::move(*restored);
  }
  // Forget remembered Agents that are no longer in the Codex.
  sel_.slugs.erase(std::remove_if(sel_.slugs.begin(), sel_.slugs.end(),
                                  [&](const std::string& s) {
                                    return std::none_of(agents_.begin(), agents_.end(),
                                                        [&](const services::Agent& a) { return a.slug == s; });
                                  }),
                   sel_.slugs.end());
  if (scopeOnEnter_) sel_.questType = domain::questTypeFromStorage(*scopeOnEnter_);
  if (addOnEnter_ && std::find(sel_.slugs.begin(), sel_.slugs.end(), *addOnEnter_) == sel_.slugs.end()) {
    if (sel_.slugs.size() >= domain::MAX_COMPARE) sel_.slugs.pop_back();   // a full table gives up its last slot
    sel_.slugs.push_back(*addOnEnter_);
  }
  addOnEnter_.reset();
  scopeOnEnter_.reset();
  if (!devSelection_) save(ctx);
}

void ProvingScreen::save(app::FrameCtx& ctx) {
  if (ctx.services == nullptr) return;
  if (auto r = ctx.services->proving.remember(ctx.userId, sel_); !r) ui::pushGameMessage(r.error().message);
  dirty_ = true;
}

void ProvingScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##proving", "The Proving Grounds"};
  if (!loaded_) load(ctx);
  if (ui::PixelButton("Back", {70, 0}) && ctx.nav != nullptr) ctx.nav->back();
  ImGui::SameLine();
  ui::ShadowText("Compare 2 or 3 familiars.", ui::PaletteColor::Parchment);

  drawPicker(ctx);
  ImGui::Separator();
  ImGui::BeginChild("##compbody", {0, 0});   // the table scrolls on its own, so the panel never overflows
  drawTable(ctx);
  ImGui::EndChild();
}

void ProvingScreen::drawPicker(app::FrameCtx& ctx) {
  bool first = true;
  for (std::size_t i = 0; i < domain::MAX_COMPARE; ++i) {
    const bool filled = i < sel_.slugs.size();
    std::string name = "(empty)";
    if (filled) {
      const auto it = std::find_if(agents_.begin(), agents_.end(), [&](const services::Agent& a) { return a.slug == sel_.slugs[i]; });
      if (it != agents_.end()) name = it->name;
    }
    // Slots fill in order, so the first empty slot is the only one that can be added to.
    const bool usable = i <= sel_.slugs.size();
    const std::string label = "Slot " + std::to_string(i + 1) + ": " + name;
    ImGui::BeginDisabled(!usable);
    if (codex::flowButton(label, ("slot" + std::to_string(i)).c_str(), first)) {
      picking_ = picking_ == static_cast<int>(i) ? -1 : static_cast<int>(i);
    }
    ImGui::EndDisabled();
  }
  if (codex::flowButton(std::string("Quest Type: ") + (sel_.questType ? domain::displayName(*sel_.questType) : "All"), "pscope", first)) {
    int cur = -1;
    for (std::size_t i = 0; i < domain::QUEST_TYPES.size(); ++i) {
      if (sel_.questType && domain::QUEST_TYPES[i] == *sel_.questType) cur = static_cast<int>(i);
    }
    const int next = codex::cycleNext(cur, static_cast<int>(domain::QUEST_TYPES.size()));
    sel_.questType = next < 0 ? std::nullopt : std::optional{domain::QUEST_TYPES[static_cast<std::size_t>(next)]};
    save(ctx);
  }

  if (picking_ >= 0 && static_cast<std::size_t>(picking_) <= sel_.slugs.size()) {
    const auto slot = static_cast<std::size_t>(picking_);
    ui::ShadowText("Choose a familiar for slot " + std::to_string(slot + 1) + ":", ui::PaletteColor::TextOrange);
    if (slot < sel_.slugs.size() && ui::PixelButton("Clear this slot##clear", {0, 0})) {
      sel_.slugs.erase(sel_.slugs.begin() + static_cast<std::ptrdiff_t>(slot));
      picking_ = -1;
      save(ctx);
    }
    ImGui::BeginChild("##pickerlist", {0, 82}, ImGuiChildFlags_Borders);
    for (const auto& a : agents_) {
      const bool taken = std::find(sel_.slugs.begin(), sel_.slugs.end(), a.slug) != sel_.slugs.end();
      if (taken) continue;   // each familiar once
      if (ImGui::Selectable((a.name + "  (" + a.vendor + ")##" + a.slug).c_str(), false)) {
        if (slot < sel_.slugs.size()) {
          sel_.slugs[slot] = a.slug;
        } else {
          sel_.slugs.push_back(a.slug);
        }
        picking_ = -1;
        save(ctx);
        break;   // the list changed under us
      }
    }
    ImGui::EndChild();
  }
}

void ProvingScreen::drawTable(app::FrameCtx& ctx) {
  if (sel_.slugs.size() < domain::MIN_COMPARE) {
    ui::ShadowText("Pick at least two familiars to compare.", ui::PaletteColor::Parchment);
    return;
  }
  if (dirty_ && ctx.services != nullptr) {
    dirty_ = false;
    columns_.clear();
    if (auto cols = ctx.services->codex.compareColumns(sel_.slugs, sel_.questType)) columns_ = std::move(*cols);
  }
  if (columns_.size() < domain::MIN_COMPARE) return;
  const auto cmp = domain::compare(columns_, sel_.questType);

  if (ui::PixelButton("Copy##table", {0, 0})) {
    ImGui::SetClipboardText(domain::toPlainText(cmp).c_str());
    ui::pushGameMessage("The comparison has been copied.");
  }
  ImGui::SameLine();
  ui::ShadowText("* marks the highest level in a row; - means unranked.", ui::PaletteColor::Parchment);

  const int cols = static_cast<int>(cmp.names.size()) + 1;
  if (!ImGui::BeginTable("##compare", cols, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) return;
  ImGui::TableSetupColumn("Skill", ImGuiTableColumnFlags_WidthStretch, 1.4F);
  for (const auto& n : cmp.names) ImGui::TableSetupColumn(n.c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0F);
  ImGui::TableHeadersRow();
  for (const auto& r : cmp.rows) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(r.label.c_str());
    for (std::size_t i = 0; i < r.levels.size(); ++i) {
      ImGui::TableSetColumnIndex(static_cast<int>(i) + 1);
      // Green fill AND a '*', so the mark never depends on colour alone.
      if (r.best[i]) ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0x2e, 0x6a, 0x2e, 255));
      const std::string text = (r.levels[i] ? std::to_string(*r.levels[i]) : std::string("-")) + (r.best[i] ? " *" : "");
      ImGui::TextUnformatted(text.c_str());
    }
  }
  ImGui::EndTable();
}

}  // namespace as::screens
