#include "screens/codex/codex_screen.hpp"

#include <imgui.h>

#include <cmath>
#include <cstdio>
#include <iterator>

#include "app/services.hpp"
#include "domain/skills.hpp"
#include "screens/codex/codex_widgets.hpp"
#include "ui/chatbox.hpp"
#include "ui/ui_context.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

namespace {
using domain::SortKey;

constexpr int SORT_OPTIONS = 13;  // Total, Quest Logs, Name, then the ten Skills

const char* sortName(int index) {
  if (index == 0) return "Total level";
  if (index == 1) return "Quest Logs";
  if (index == 2) return "Name";
  return domain::displayName(domain::SKILLS[static_cast<std::size_t>(index - 3)]);
}

// Book colours: muted, so parchment text stays readable. Picked by a stable hash of the vendor.
ImU32 spineColor(const std::string& vendor) {
  static constexpr ImU32 COLORS[] = {IM_COL32(0x6b, 0x3a, 0x2e, 255), IM_COL32(0x2e, 0x4a, 0x6b, 255),
                                     IM_COL32(0x3e, 0x5a, 0x2e, 255), IM_COL32(0x5a, 0x2e, 0x5a, 255),
                                     IM_COL32(0x6b, 0x5a, 0x2e, 255), IM_COL32(0x2e, 0x5a, 0x5a, 255)};
  std::size_t h = 0;
  for (const char c : vendor) h = h * 31 + static_cast<unsigned char>(c);
  return COLORS[h % std::size(COLORS)];
}
}  // namespace

int CodexScreen::sortIndex() const {
  switch (query_.sort) {
    case SortKey::Total: return 0;
    case SortKey::QuestLogs: return 1;
    case SortKey::Name: return 2;
    case SortKey::Skill: return 3 + static_cast<int>(query_.skill);
  }
  return 0;
}

void CodexScreen::setSortIndex(int index) {
  if (index == 0) {
    query_.sort = SortKey::Total;
  } else if (index == 1) {
    query_.sort = SortKey::QuestLogs;
  } else if (index == 2) {
    query_.sort = SortKey::Name;
  } else {
    query_.sort = SortKey::Skill;
    query_.skill = domain::SKILLS[static_cast<std::size_t>(index - 3)];
  }
}

void CodexScreen::onEnter(const app::ScreenParams& params) {
  dirty_ = true;  // ratings may have changed while a card was open
  if (const auto it = params.find("view"); it != params.end()) shelf_ = it->second != "table";  // dev QA
  if (const auto it = params.find("q"); it != params.end()) {
    std::snprintf(search_.data(), search_.size(), "%s", it->second.c_str());
    query_.text = search_.data();
  }
}

void CodexScreen::refresh(app::FrameCtx& ctx) {
  dirty_ = false;
  error_.clear();
  if (ctx.services == nullptr) return;
  auto page = ctx.services->codex.list(query_, scope_);
  if (!page) {
    page_.reset();
    error_ = page.error().message;
    return;
  }
  page_ = std::move(*page);
  query_.page = page_->slice.page;  // the service clamps a page that no longer exists
}

void CodexScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##codex", "The Grand Codex"};
  if (dirty_) refresh(ctx);
  drawControls(ctx);
  if (!error_.empty()) {
    ui::ShadowText(error_, ui::PaletteColor::TextOrange);
    return;
  }
  if (!page_) return;

  if (page_->rows.empty()) {
    ui::ShadowText("No familiars match your search.", ui::PaletteColor::Parchment);
  } else if (shelf_) {
    drawShelf(ctx);
  } else {
    drawTable(ctx);
  }
  drawPaging();
}

void CodexScreen::drawControls(app::FrameCtx& ctx) {
  const float rowY = ImGui::GetCursorPosY();
  if (ui::PixelButton("Back", {70, 0}) && ctx.nav != nullptr) ctx.nav->back();
  ImGui::SameLine();
  ui::AlignFrameToButton();
  ImGui::SetNextItemWidth(150);
  if (ImGui::InputTextWithHint("##search", "Search name or vendor", search_.data(), search_.size())) {
    query_.text = search_.data();
    query_.page = 0;
    dirty_ = true;
  }
  ImGui::SameLine();
  ImGui::SetCursorPosY(rowY);
  ui::AlignTextToButton();
  ui::ShadowText("Sample snapshot: fictional agents", ui::PaletteColor::Parchment);

  bool first = true;
  const std::string kindText = std::string("Kind: ") + (query_.kind ? (*query_.kind == domain::AgentKind::Model ? "Model" : "Tool") : "All");
  if (codex::flowButton(kindText, "kind", first)) {
    if (!query_.kind) {
      query_.kind = domain::AgentKind::Model;
    } else if (*query_.kind == domain::AgentKind::Model) {
      query_.kind = domain::AgentKind::Tool;
    } else {
      query_.kind.reset();
    }
    query_.page = 0;
    dirty_ = true;
  }
  const auto& vendors = page_ ? page_->vendors : std::vector<std::string>{};
  if (codex::flowButton("Vendor: " + (query_.vendor ? *query_.vendor : std::string("All")), "vendor", first)) {
    int cur = -1;
    for (std::size_t i = 0; i < vendors.size(); ++i) {
      if (query_.vendor && vendors[i] == *query_.vendor) cur = static_cast<int>(i);
    }
    const int next = codex::cycleNext(cur, static_cast<int>(vendors.size()));
    if (next < 0) {
      query_.vendor.reset();
    } else {
      query_.vendor = vendors[static_cast<std::size_t>(next)];
    }
    query_.page = 0;
    dirty_ = true;
  }
  if (codex::flowButton(std::string("Quest: ") + (scope_ ? domain::displayName(*scope_) : "All"), "scope", first)) {
    int cur = -1;
    for (std::size_t i = 0; i < domain::QUEST_TYPES.size(); ++i) {
      if (scope_ && domain::QUEST_TYPES[i] == *scope_) cur = static_cast<int>(i);
    }
    const int next = codex::cycleNext(cur, static_cast<int>(domain::QUEST_TYPES.size()));
    scope_ = next < 0 ? std::nullopt : std::optional{domain::QUEST_TYPES[static_cast<std::size_t>(next)]};
    query_.page = 0;
    dirty_ = true;
  }
  if (codex::flowButton(std::string("Sort: ") + sortName(sortIndex()), "sort", first)) {
    setSortIndex((sortIndex() + 1) % SORT_OPTIONS);
    query_.page = 0;
    dirty_ = true;
  }
  if (codex::flowButton(query_.reversed ? "Reversed" : "Reverse", "rev", first)) {
    query_.reversed = !query_.reversed;
    dirty_ = true;
  }
  if (codex::flowButton(shelf_ ? "View: Shelf" : "View: Table", "view", first)) shelf_ = !shelf_;
  if (ctx.isKeeper && codex::flowButton("Keeper queue", "keeper", first) && ctx.nav != nullptr) {
    ctx.nav->navigate(app::ScreenId::Keeper);
  }
  if (codex::flowButton("Proving Grounds", "proving", first) && ctx.nav != nullptr) {
    app::ScreenParams p;
    if (scope_) p["scope"] = domain::storageName(*scope_);
    ctx.nav->navigate(app::ScreenId::Proving, p);
  }
  if (codex::flowButton("Propose a familiar", "propose", first) && ctx.nav != nullptr) ctx.nav->navigate(app::ScreenId::Propose);
  ImGui::Spacing();
}

void CodexScreen::open(app::FrameCtx& ctx, const services::CodexRow& row) {
  app::ScreenParams p{{"agent", row.agent.slug}};
  if (scope_) p["scope"] = domain::storageName(*scope_);
  if (ctx.nav != nullptr) ctx.nav->navigate(app::ScreenId::CodexCard, p);
}

void CodexScreen::drawShelf(app::FrameCtx& ctx) {
  constexpr int COLS = 5;
  constexpr float GAP = 4.0F;
  const float availW = ImGui::GetContentRegionAvail().x;
  const float bookW = std::floor((availW - GAP * (COLS - 1) - 4.0F) / COLS);
  const float bookH = 50.0F;
  const float bottomReserve = ImGui::GetFrameHeightWithSpacing() + 6.0F;
  ImGui::BeginChild("##shelf", {0, ImGui::GetContentRegionAvail().y - bottomReserve});
  ImDrawList* dl = ImGui::GetWindowDrawList();
  ImFont* font = ImGui::GetFont();
  const float fs = ImGui::GetFontSize();
  const ImU32 ink = ui::toU32(ui::PaletteColor::Ink);
  const ImU32 text = ui::toU32(ui::PaletteColor::Parchment);
  int n = 0;
  for (const auto& row : page_->rows) {
    if (n % COLS != 0) ImGui::SameLine(0, GAP);
    ImGui::PushID(row.agent.slug.c_str());
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("##book", {bookW, bookH})) open(ctx, row);
    const bool active = ImGui::IsItemHovered() || ImGui::IsItemFocused();
    const ImVec2 p1{p0.x + bookW, p0.y + bookH};
    dl->AddRectFilled(p0, p1, spineColor(row.agent.vendor));
    dl->AddRectFilled(p0, {p0.x + 4, p1.y}, ui::toU32(ui::PaletteColor::TextOrange, 0.8F));  // gold-ish spine band
    dl->AddRect(p0, p1, active ? ui::toU32(ui::PaletteColor::TextYellow) : ink);
    const float tw = bookW - 12.0F;
    dl->AddText(font, fs, {p0.x + 8, p0.y + 3}, ink, row.agent.name.c_str(), nullptr, tw);
    dl->AddText(font, fs, {p0.x + 7, p0.y + 2}, text, row.agent.name.c_str(), nullptr, tw);
    const std::string lv = "Total " + std::to_string(row.levels.total);
    dl->AddText(font, fs, {p0.x + 8, p1.y - fs - 3}, ink, lv.c_str());
    dl->AddText(font, fs, {p0.x + 7, p1.y - fs - 4}, ui::toU32(ui::PaletteColor::TextYellow), lv.c_str());
    if (active) {
      ui::setHoverText(ui::hoverText("Examine", row.agent.name, ui::PaletteColor::TextCyan, 0));
    }
    ImGui::PopID();
    ++n;
  }
  ImGui::EndChild();
}

void CodexScreen::drawTable(app::FrameCtx& ctx) {
  const float bottomReserve = ImGui::GetFrameHeightWithSpacing() + 6.0F;
  const ImVec2 size{0, ImGui::GetContentRegionAvail().y - bottomReserve};
  if (!ImGui::BeginTable("##codextable", 5, ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp, size)) return;
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 3.0F);
  ImGui::TableSetupColumn("Vendor", ImGuiTableColumnFlags_WidthStretch, 3.0F);
  ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 44.0F);
  ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, 44.0F);
  ImGui::TableSetupColumn("Logs", ImGuiTableColumnFlags_WidthFixed, 40.0F);
  ImGui::TableHeadersRow();
  for (const auto& row : page_->rows) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    // The selectable spans the row for click/nav; the cells draw shadowed text like the rest of the UI.
    ImGui::PushID(row.agent.slug.c_str());
    const ImVec2 cell = ImGui::GetCursorPos();
    const bool clicked =
        ImGui::Selectable("##row", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                          {0, ImGui::GetFontSize() + 2});
    const bool active = ImGui::IsItemHovered() || ImGui::IsItemFocused();
    if (active) ui::setHoverText(ui::hoverText("Examine", row.agent.name, ui::PaletteColor::TextCyan, 0));
    ImGui::PopID();
    ImGui::SetCursorPos(cell);
    ui::ShadowText(row.agent.name, ui::PaletteColor::TextCyan);
    ImGui::TableSetColumnIndex(1);
    ui::ShadowText(row.agent.vendor);
    ImGui::TableSetColumnIndex(2);
    ui::ShadowText(services::kindDisplay(row.agent.kind));
    ImGui::TableSetColumnIndex(3);
    ui::ShadowText(std::to_string(row.levels.total));
    ImGui::TableSetColumnIndex(4);
    ui::ShadowText(std::to_string(row.levels.overallCount));
    if (clicked) open(ctx, row);
  }
  ImGui::EndTable();
}

void CodexScreen::drawPaging() {
  const auto& s = page_->slice;
  ImGui::Spacing();
  ImGui::BeginDisabled(s.page == 0);
  if (ui::PixelButton("<##prev", {32, 0})) {
    query_.page = s.page - 1;
    dirty_ = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ui::AlignTextToButton();
  ui::ShadowText("Page " + std::to_string(s.page + 1) + "/" + std::to_string(s.pages) + "  (" +
                     std::to_string(page_->total) + " familiars)",
                 ui::PaletteColor::Parchment);
  ImGui::SameLine();
  ImGui::BeginDisabled(s.page + 1 >= s.pages);
  if (ui::PixelButton(">##next", {32, 0})) {
    query_.page = s.page + 1;
    dirty_ = true;
  }
  ImGui::EndDisabled();
}

}  // namespace as::screens
