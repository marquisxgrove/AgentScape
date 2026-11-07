#include "screens/codex/propose_screen.hpp"

#include <imgui.h>

#include "app/services.hpp"
#include "ui/chatbox.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

void ProposeScreen::onEnter(const app::ScreenParams& params) {
  errorField_.clear();
  errorText_.clear();
  openSlug_.reset();
  done_ = false;
  if (const auto it = params.find("name"); it != params.end()) {  // dev QA: --screen=codex-propose
    std::snprintf(name_.data(), name_.size(), "%s", it->second.c_str());
  }
  if (params.count("demo") != 0) {
    std::snprintf(vendor_.data(), vendor_.size(), "Candlewick");
    std::snprintf(examine_.data(), examine_.size(), "A tiny note-taking helper that glows when it is unsure.");
    std::snprintf(website_.data(), website_.size(), "http://candlewick.example");
    submitOnEnter_ = true;
  }
}

void ProposeScreen::fieldError(const char* field) {
  if (errorField_ == field && !errorText_.empty()) ui::ShadowText(errorText_, ui::PaletteColor::TextOrange);
}

void ProposeScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##propose", "Propose a familiar"};
  if (submitOnEnter_) {   // dev QA only: shows what a refused proposal looks like
    submitOnEnter_ = false;
    submit(ctx);
  }
  if (ui::PixelButton("Back", {70, 0}) && ctx.nav != nullptr) ctx.nav->back();
  ImGui::SameLine();
  ui::ShadowText("A Codex Keeper reviews every proposal.", ui::PaletteColor::Parchment);

  const float w = ImGui::GetContentRegionAvail().x - 8;
  ImGui::BeginChild("##proposeform", {0, 0});
  ui::ShadowText("Name", ui::PaletteColor::Parchment);
  ImGui::SetNextItemWidth(w);
  ImGui::InputText("##pname", name_.data(), name_.size());
  fieldError("name");

  ui::ShadowText("Vendor", ui::PaletteColor::Parchment);
  ImGui::SetNextItemWidth(w);
  ImGui::InputText("##pvendor", vendor_.data(), vendor_.size());
  fieldError("vendor");

  if (ui::PixelButton(kind_ == domain::AgentKind::Model ? "Kind: Model##pk" : "Kind: Tool##pk", {0, 0})) {
    kind_ = kind_ == domain::AgentKind::Model ? domain::AgentKind::Tool : domain::AgentKind::Model;
  }

  ui::ShadowText("Examine (one line, up to 200 characters)", ui::PaletteColor::Parchment);
  ImGui::SetNextItemWidth(w);
  ImGui::InputText("##pexamine", examine_.data(), examine_.size());
  fieldError("examine");

  ui::ShadowText("About (optional)", ui::PaletteColor::Parchment);
  ImGui::InputTextMultiline("##pabout", about_.data(), about_.size(), {w, 54});
  fieldError("about");

  ui::ShadowText("Website (optional, https only)", ui::PaletteColor::Parchment);
  ImGui::SetNextItemWidth(w);
  ImGui::InputText("##pweb", website_.data(), website_.size());
  fieldError("website");

  ImGui::Spacing();
  if (ui::PixelButton("Submit##propose", {0, 0})) submit(ctx);
  if (!errorText_.empty() && errorField_.empty()) ui::ShadowText(errorText_, ui::PaletteColor::TextOrange);
  if (openSlug_) {
    ImGui::SameLine();
    if (ui::PixelButton("Open it##existing", {0, 0}) && ctx.nav != nullptr) {
      ctx.nav->navigate(app::ScreenId::CodexCard, {{"agent", *openSlug_}});
    }
  }
  ImGui::EndChild();
}

void ProposeScreen::submit(app::FrameCtx& ctx) {
  errorField_.clear();
  errorText_.clear();
  openSlug_.reset();
  if (ctx.services == nullptr) return;

  domain::AgentInput in;
  in.name = name_.data();
  in.vendor = vendor_.data();
  in.kind = kind_;
  in.examine = examine_.data();
  in.about = about_.data();
  if (website_[0] != '\0') in.website = std::string(website_.data());

  auto r = ctx.services->agents.proposeAgent(ctx.userId, std::move(in));
  if (!r) {
    errorField_ = r.error().field;
    errorText_ = r.error().error.message;
    openSlug_ = r.error().existingSlug;
    if (r.error().field.empty()) ui::pushGameMessage(r.error().error.message);
    return;
  }
  name_.fill('\0');
  vendor_.fill('\0');
  examine_.fill('\0');
  about_.fill('\0');
  website_.fill('\0');
  ui::pushGameMessage("Your proposal has been sent to the Codex Keepers.");
  if (ctx.nav != nullptr) ctx.nav->back();
}

}  // namespace as::screens
