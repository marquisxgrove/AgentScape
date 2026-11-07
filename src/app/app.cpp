#include "app/app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "domain/items.hpp"
#include "domain/xp.hpp"
#include "game/hotspots.hpp"
#include "game/xp_feedback.hpp"
#include "platform/accessibility.hpp"
#include "platform/paths.hpp"
#include "platform/web_url.hpp"
#include "screens/codex/codex_card_screen.hpp"
#include "screens/codex/codex_screen.hpp"
#include "screens/codex/keeper_screen.hpp"
#include "screens/codex/propose_screen.hpp"
#include "screens/graveyard/graveyard_screen.hpp"
#include "screens/hiscores/hiscores_screen.hpp"
#include "screens/codex/proving_screen.hpp"
#include "screens/costumer/costumer_screen.hpp"
#include "screens/login/login_screen.hpp"
#include "screens/quest/quest_detail_screen.hpp"
#include "screens/quest/quest_scroll_screen.hpp"
#include "screens/quest/tavern_screen.hpp"
#include "screens/world/world_screen.hpp"
#include "ui/layout.hpp"
#include "ui/tab_panel.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"
#ifdef AS_ENABLE_DEV_SCREENS
#include "screens/dev/avatar_gallery_screen.hpp"
#include "screens/dev/ui_gallery_screen.hpp"
#endif

namespace as::app {

namespace {
std::chrono::sys_seconds utcNow() {
  return std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
}

void logError(const char* what, const GameError& e) {
  SDL_Log("%s: %s%s%s", what, e.message.c_str(), e.detail.empty() ? "" : " | ", e.detail.c_str());
}

constexpr int SNAPSHOT_FRAME = 3;  // a few frames in, so ImGui has laid everything out
constexpr int SMOKE_FRAMES = 5;

// A borderless, transparent ImGui window covering a frame region. Panels are drawn by widgets.
bool beginRegion(const char* id, const ui::FrameRect& r) {
  ImGui::SetNextWindowPos({static_cast<float>(r.x), static_cast<float>(r.y)});
  ImGui::SetNextWindowSize({static_cast<float>(r.w), static_cast<float>(r.h)});
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus;
  return ImGui::Begin(id, nullptr, kFlags);
}

constexpr ui::FrameRect FULL_FRAME{0, 0, domain::FRAME_W, domain::FRAME_H};
constexpr ui::FrameRect CENTER_DIALOG{232, 186, 300, 110};
}  // namespace

Config configFromProcess(int argc, char** argv) {
  std::vector<std::string_view> args;
  for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
  ConfigSources sources;
  sources.args = args;
  sources.env = [](std::string_view name) -> std::optional<std::string> {
    // SDL_getenv rather than std::getenv: MSVC deprecates the latter (C4996, fatal under /WX).
    const char* v = SDL_getenv(std::string(name).c_str());
    return v != nullptr ? std::optional<std::string>{v} : std::nullopt;
  };
  sources.urlKiroween = platform::urlQueryParam("kiroween");
  sources.buildKiroweenDefault = AS_KIROWEEN_DEFAULT;
  return parseConfig(sources);
}

std::unique_ptr<App> App::create(const Config& config) {
  std::unique_ptr<App> app{new App(config)};
  if (!app->init()) return nullptr;
  return app;
}

bool App::init() {
  for (const auto& w : config_.warnings) SDL_Log("%s", w.c_str());

  constexpr int kStartScale = 2;
  if (!SDL_CreateWindowAndRenderer("AgentScape", domain::FRAME_W * kStartScale, domain::FRAME_H * kStartScale,
                                   SDL_WINDOW_RESIZABLE, &window_, &renderer_)) {
    SDL_Log("Accelerated renderer unavailable (%s); trying software", SDL_GetError());
    if (window_ == nullptr) {
      window_ = SDL_CreateWindow("AgentScape", domain::FRAME_W * kStartScale, domain::FRAME_H * kStartScale,
                                 SDL_WINDOW_RESIZABLE);
    }
    renderer_ = window_ != nullptr ? SDL_CreateRenderer(window_, SDL_SOFTWARE_RENDERER) : nullptr;
    if (renderer_ == nullptr) {
      SDL_Log("Could not create window/renderer: %s", SDL_GetError());
      return false;
    }
  }
  SDL_SetWindowMinimumSize(window_, domain::FRAME_W, domain::FRAME_H);

  frame_ = std::make_unique<render::GameFrame>(renderer_);
  if (!frame_->valid()) return false;

  kiroween_ = domain::isKiroweenActive(config_.kiroween, utcNow());

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
  ImGui_ImplSDLRenderer3_Init(renderer_);

  ui::setContext(&ui_);
  ui_.kiroween = kiroween_;
  ui::applyTheme(kiroween_);
  const auto assets = platform::assetsDir();
  ui_.fonts = ui::loadFonts(assets / "fonts");
  textures_ = std::make_unique<render::TextureCache>(renderer_);
  ui_.skin = ui::loadSkin(*textures_, assets / "ui");
  ui_.textures = textures_.get();

  if (auto avatars = game::AvatarRenderer::create(renderer_, assets / "sprites")) {
    avatars_ = std::move(*avatars);
  } else {
    logError("Avatar renderer", avatars.error());  // the game still runs; avatars just aren't drawn
  }

  updateData();  // desktop opens the store immediately; the web waits for IndexedDB
  return true;
}

App::~App() {
  xpSub_.reset();
  unlockSub_.reset();
  chatbox_.reset();  // unsubscribes from the bus before services/db go away
  services_.reset();
  store_.reset();  // close the database before tearing down the platform layer
  avatars_.reset();
  textures_.reset();
  ui::setContext(nullptr);
  if (ImGui::GetCurrentContext() != nullptr) {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
  frame_.reset();
  if (renderer_ != nullptr) SDL_DestroyRenderer(renderer_);
  if (window_ != nullptr) SDL_DestroyWindow(window_);
}

void App::updateData() {
  if (dataState_ != DataState::WaitingForStorage) return;
  const auto ps = persistence_.state();
  if (ps == platform::Persistence::State::Loading) return;
  inMemoryOnly_ = ps == platform::Persistence::State::Unavailable;
  if (inMemoryOnly_) SDL_Log("Persistent storage unavailable; running with an in-memory database");
  openStore(config_.resetData);
}

void App::openStore(bool resetData) {
  LocalStoreOptions opts;
#ifdef __EMSCRIPTEN__
  opts.journal = db::JournalMode::Delete;
  if (!inMemoryOnly_) opts.dataDir = platform::defaultDataDir();
#else
  opts.dataDir = platform::resolveDataDir(config_.dataDir, platform::defaultDataDir);
#endif
  opts.resetData = resetData;
  opts.reseed = config_.reseed;
  savePath_ = opts.dataDir ? (*opts.dataDir / platform::DB_FILE_NAME).string() : std::string{":memory:"};

  auto opened = openLocalStore(opts, utcNow());
  if (!opened) {
    logError("Local store", opened.error().error);
    loadError_ = opened.error();
    dataState_ = DataState::LoadFailed;
    return;
  }
  store_ = std::move(*opened);
  loadError_.reset();
  store_->db.setAfterCommit([this] { persistence_.requestFlush(); });
  if (!store_->backupPath.empty()) SDL_Log("Old save moved to %s", store_->backupPath.string().c_str());
  if (store_->seeded) SDL_Log("Seeded community snapshot: %d NPCs, %d Agents, %d rating aggregates, %d Notes", store_->seeded->npcsUpserted,
            store_->seeded->agentsUpserted, store_->seeded->statRows, store_->seeded->notesUpserted);
  SDL_Log("Local store ready: %s (schema %d)", savePath_.c_str(), store_->schemaVersion);
  persistence_.requestFlush();  // migrations/seed on a new web save
  services_.emplace(Services{services::ProfileService{store_->db, clock_, rng_},
                             services::ActivityService{store_->db, clock_, bus_},
                             services::AdventurerService{store_->db, clock_, rng_},
                             services::CodexService{store_->db, codexCache_},
                             services::NoteService{store_->db, clock_, rng_, bus_},
                             services::AgentService{store_->db, clock_, rng_, bus_},
                             services::KeeperService{store_->db, clock_, rng_, bus_},
                             services::ProvingService{store_->db, clock_},
                             services::QuestService{store_->db, clock_, rng_, bus_},
                             services::HiscoresService{store_->db, codexCache_}});
  // An idle Guest's content leaves the Codex through the normal delete paths, not by cascade (quest-logs Req 9.2).
  services_->profile.setBeforeGuestDelete([this](const std::string& userId) {
    if (auto r = services_->quests.purgeUser(userId); !r) logError("Purge Guest quest logs", r.error());
    if (auto r = services_->notes.retractAllVotes(userId); !r) logError("Purge Guest note votes", r.error());
  });
  if (config_.repairXp) {
    // total_xp is a cache of the ledger; this recomputes it (quest-logs design: --repair-xp).
    services::XpService repair{store_->db, clock_, rng_, services_->activity};
    if (auto fixed = repair.repairTotals()) {
      SDL_Log("Repaired total XP for %d user(s)", *fixed);
    } else {
      logError("Repair XP", fixed.error());
    }
  }
  xpSub_ = app::Subscription{bus_, bus_.subscribe<app::XpChanged>([this](const app::XpChanged& e) { onXpChanged(e); })};
  unlockSub_ = app::Subscription{bus_, bus_.subscribe<app::ItemUnlocked>([this](const app::ItemUnlocked& e) { onItemUnlocked(e); })};
  chatbox_.emplace(bus_);
  onStoreReady();
  dataState_ = DataState::Ready;
}

void App::onStoreReady() {
  if (auto removed = services_->profile.cleanupGuests(); removed && *removed > 0) {
    SDL_Log("Removed %d stale Guest profile(s)", *removed);
  }
  registerScreens();
  signInRemembered();  // Req 2.1: auto sign-in the remembered user (Player or Guest)
}

void App::signInRemembered() {
  auto remembered = services_->profile.rememberedUser();
  if (!remembered || !*remembered) return;
  auto user = services_->profile.signIn((*remembered)->id);
  if (!user) return;
  adoptUser(user->id, user->settings);
  screens_.navigate(ScreenId::World);
  if (postLoginScreen_) {
    screens_.navigate(*postLoginScreen_, postLoginParams_);
  } else if (auto active = services_->adventurers.getActive(user->id); active && !*active) {
    screens_.navigate(ScreenId::Costumer);  // Req 2.6, as after the login screen: no Adventurer yet
  }
  SDL_Log("Signed in remembered user %s", user->name.c_str());
}

void App::adoptUser(const std::string& userId, const domain::Settings& settings) {
  currentUserId_ = userId;
  applySettings(settings);
  // The Questing level comes from the user's XP (quest-logs Requirement 5.4). A new Guest starts with seed XP, so
  // they begin at level 13; players begin at level 1. The unlock sync below is idempotent and also covers
  // anything that became reachable while the app was closed.
  currentLevel_ = 1;
  totalXp_ = 0;
  isKeeper_ = false;
  effects_.clear();
  if (services_) {
    if (auto keeper = services_->keeper.isKeeper(userId)) isKeeper_ = *keeper;
    if (auto user = services_->profile.getUser(userId)) {
      totalXp_ = user->totalXp;
      currentLevel_ = domain::levelForXp(totalXp_);
    }
    services_->adventurers.syncLevelUnlocks(userId, currentLevel_);
  }
  // Seed the Chatbox: 20 most recent Tavern events, then the welcome line (Req 5.1/5.3).
  if (chatbox_ && services_) {
    if (auto recent = services_->activity.latest(20)) {
      std::vector<domain::ChatMessage> msgs;
      msgs.reserve(recent->size());
      for (const auto& e : *recent) msgs.push_back(services::toChatMessage(e));
      chatbox_->loadInitial(msgs);
    }
    ui::pushGameMessage("Welcome to AgentScape.");
    if (kiroween_) ui::pushGameMessage("A chill runs through Promptmoor. Kiroween has begun!");
  }
}

void App::applySettings(const domain::Settings& settings) {
  settings_ = settings;
  ui_.plainFont = settings.plainFont;
}

SDL_AppResult App::handleEvent(SDL_Event& event) {
  if (event.type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
  // Pointer events are converted to frame pixels before ImGui sees them.
  switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION: {
      const auto p = frame_->toFrame(event.motion.x, event.motion.y);
      event.motion.x = p.x;
      event.motion.y = p.y;
      break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      const auto p = frame_->toFrame(event.button.x, event.button.y);
      event.button.x = p.x;
      event.button.y = p.y;
      break;
    }
    default:
      break;
  }
  ImGui_ImplSDL3_ProcessEvent(&event);
  return SDL_APP_CONTINUE;
}

void App::registerScreens() {
  screens_.registerScreen(ScreenId::Login, [] { return std::make_unique<screens::LoginScreen>(); });
  screens_.registerScreen(ScreenId::World, [] { return std::make_unique<screens::WorldScreen>(); });
  screens_.registerScreen(ScreenId::Costumer, [] { return std::make_unique<screens::CostumerScreen>(); });
  screens_.registerScreen(ScreenId::Codex, [] { return std::make_unique<screens::CodexScreen>(); });
  screens_.registerScreen(ScreenId::Keeper, [] { return std::make_unique<screens::KeeperScreen>(); });
  screens_.registerScreen(ScreenId::Proving, [] { return std::make_unique<screens::ProvingScreen>(); });
  screens_.registerScreen(ScreenId::Graveyard, [] { return std::make_unique<screens::GraveyardScreen>(); });
  screens_.registerScreen(ScreenId::Hiscores, [] { return std::make_unique<screens::HiscoresScreen>(); });
  screens_.registerScreen(ScreenId::Tavern, [] { return std::make_unique<screens::TavernScreen>(); });
  screens_.registerScreen(ScreenId::QuestDetail, [] { return std::make_unique<screens::QuestDetailScreen>(); });
  screens_.registerScreen(ScreenId::QuestScroll, [] { return std::make_unique<screens::QuestScrollScreen>(); });
  screens_.registerScreen(ScreenId::Propose, [] { return std::make_unique<screens::ProposeScreen>(); });
  screens_.registerScreen(ScreenId::CodexCard, [] { return std::make_unique<screens::CodexCardScreen>(); });
#ifdef AS_ENABLE_DEV_SCREENS
  screens_.registerScreen(ScreenId::DevGallery, [] { return std::make_unique<screens::UiGalleryScreen>(); });
  screens_.registerScreen(ScreenId::DevAvatars, [] { return std::make_unique<screens::AvatarGalleryScreen>(); });
#endif
  ScreenId start = ScreenId::Login;
  ScreenParams startParams;
  if (config_.startScreen) {
#ifdef AS_ENABLE_DEV_SCREENS
    if (*config_.startScreen == "ui-gallery") start = ScreenId::DevGallery;
    if (*config_.startScreen == "avatar-gallery") start = ScreenId::DevAvatars;
    for (const char* tab : {"costumer", "costumer-gear", "costumer-adventurers"}) {
      if (*config_.startScreen == tab) {
        postLoginScreen_ = ScreenId::Costumer;
        const std::string name = tab;
        if (name == "costumer-gear") postLoginParams_["tab"] = "gear";
        if (name == "costumer-adventurers") postLoginParams_["tab"] = "adventurers";
      }
    }
    // Dev QA aliases: `--screen=<name>` opens this screen with these parameters right after sign-in.
    struct DevView {
      const char* name;
      ScreenId screen;
      ScreenParams params;
    };
    static const std::vector<DevView> DEV_VIEWS = {
        {"codex", ScreenId::Codex, {}},
        {"codex-table", ScreenId::Codex, {{"view", "table"}}},
        {"codex-search", ScreenId::Codex, {{"q", "lanternfold"}}},
        {"codex-card", ScreenId::CodexCard, {{"agent", "gryphon"}}},
        {"codex-card-debug", ScreenId::CodexCard, {{"agent", "gryphon"}, {"scope", "DEBUGGING"}}},
        {"codex-card-notes", ScreenId::CodexCard, {{"agent", "gryphon"}, {"scroll", "end"}}},
        {"codex-card-compose", ScreenId::CodexCard, {{"agent", "gryphon"}, {"scroll", "end"}, {"compose", "bane"}}},
        {"codex-propose", ScreenId::Propose, {}},
        {"codex-propose-error", ScreenId::Propose, {{"demo", "1"}, {"name", "Gryphon"}}},
        {"codex-keeper", ScreenId::Keeper, {}},
        {"proving", ScreenId::Proving, {{"with", "gryphon,cairn,gryphon-chat"}}},
        {"proving-pick", ScreenId::Proving, {{"with", "gryphon,cairn,gryphon-chat"}, {"pick", "2"}}},
        {"graveyard", ScreenId::Graveyard, {}},
        {"graveyard-list", ScreenId::Graveyard, {{"view", "list"}}},
        {"quest-scroll", ScreenId::QuestScroll, {{"agent", "gryphon"}}},
        {"quest-scroll-rating", ScreenId::QuestScroll, {{"agent", "gryphon"}, {"step", "2"}}},
        {"quest-scroll-story", ScreenId::QuestScroll, {{"agent", "gryphon"}, {"step", "3"}}},
        {"quest-scroll-demo", ScreenId::QuestScroll, {{"agent", "gryphon"}, {"demo", "1"}, {"submit", "1"}}},
        {"hiscores", ScreenId::Hiscores, {}},
        {"hiscores-speed", ScreenId::Hiscores, {{"board", "9"}}},
        {"hiscores-adventurers", ScreenId::Hiscores, {{"tab", "adventurers"}}},
        {"hiscores-search", ScreenId::Hiscores, {{"tab", "adventurers"}, {"q", "nullwyn"}}},
        {"tavern", ScreenId::Tavern, {}},
        {"tavern-helpful", ScreenId::Tavern, {{"sort", "helpful"}}},
        {"tavern-gryphon", ScreenId::Tavern, {{"agent", "gryphon"}}},
        {"quest-detail", ScreenId::QuestDetail, {{"id", "devlog03"}}},   // needs a Quest Log with this id (dev fixture)
    };
    for (const auto& view : DEV_VIEWS) {
      if (*config_.startScreen == view.name) {
        postLoginScreen_ = view.screen;
        postLoginParams_ = view.params;
      }
    }
    if (*config_.startScreen == "avatar-looks") {
      start = ScreenId::DevAvatars;
      startParams["page"] = "looks";
    }
#endif
    if (start == ScreenId::Login && !postLoginScreen_) SDL_Log("Unknown or unavailable --screen=%s", config_.startScreen->c_str());
  }
  screens_.navigate(start, startParams);
}

void App::drawLoading() {
  if (beginRegion("##loading", FULL_FRAME)) {
    ImGui::SetCursorPos({340, 245});
    ui::ShadowText("Loading...");
  }
  ImGui::End();
}

void App::drawLoadFailed() {
  if (beginRegion("##loadfailed", CENTER_DIALOG)) {
    ui::StonePanel panel{"##loadfailed-panel", "AgentScape"};
    ui::ShadowText("Your save could not be loaded.", ui::PaletteColor::TextOrange);   // red text is too low-contrast on stone
    ImGui::Spacing();
    if (loadError_ && loadError_->canStartFresh) {
      if (ui::PixelButton("Start fresh")) {
        dataState_ = DataState::WaitingForStorage;
        openStore(/*resetData=*/true);
      }
      ImGui::SameLine();
    }
    if (ui::PixelButton("Quit")) quitRequested_ = true;
  }
  ImGui::End();
}

ui::XpStatus App::xpStatus() const {
  ui::XpStatus x;
  x.level = currentLevel_;
  x.totalXp = totalXp_;
  x.toNextLevel = currentLevel_ >= domain::MAX_QUESTING_LEVEL ? 0 : domain::xpTable()[static_cast<std::size_t>(currentLevel_) + 1] - totalXp_;
  return x;
}

void App::onXpChanged(const XpChanged& e) {
  if (!currentUserId_ || e.userId != *currentUserId_) return;   // someone else's XP (another device user) is not ours to show
  const auto fb = game::feedbackFor(totalXp_, e.totalXp, currentLevel_, e.newLevel);
  totalXp_ = e.totalXp;
  currentLevel_ = e.newLevel;
  if (fb.gained > 0) effects_.addXpDrop(fb.gained);
  if (fb.levelUp) {
    ui::pushGameMessage(game::levelUpMessage(*fb.levelUp));
    effects_.addFireworks(static_cast<float>(ui::VIEWPORT.w), static_cast<float>(ui::VIEWPORT.h),
                          static_cast<std::uint64_t>(e.totalXp));
  }
}

void App::onItemUnlocked(const ItemUnlocked& e) {
  if (!currentUserId_ || e.userId != *currentUserId_) return;
  if (const auto* item = domain::findItem(e.itemId)) ui::pushGameMessage(game::unlockMessage(item->name));
}

// Floating "+N XP" drops near the tab panel's level line, and level-up fireworks over the viewport. Drawn last, on
// the foreground list, in frame pixels. With Reduce motion they are static (see game::Effects).
void App::drawEffects() {
  ImDrawList* dl = ImGui::GetForegroundDrawList();
  const ImU32 colors[4] = {ui::toU32(ui::PaletteColor::TextYellow), ui::toU32(ui::PaletteColor::TextOrange),
                           ui::toU32(ui::PaletteColor::TextCyan), ui::toU32(ui::PaletteColor::TextGreen)};
  const ImVec2 origin{static_cast<float>(ui::VIEWPORT.x), static_cast<float>(ui::VIEWPORT.y)};
  for (const auto& burst : effects_.bursts()) {
    if (burst.age <= 0.0F) continue;   // staggered: not started
    for (const auto& s : burst.sparks) {
      const float a = s.maxLife > 0 ? std::clamp(s.life / s.maxLife, 0.0F, 1.0F) : 0.0F;
      if (a <= 0.0F) continue;
      const ImU32 c = (colors[s.color % 4] & 0x00FFFFFFU) | (static_cast<ImU32>(a * 255.0F) << 24);
      dl->AddRectFilled({origin.x + s.x, origin.y + s.y}, {origin.x + s.x + 2, origin.y + s.y + 2}, c);
    }
  }
  const ImVec2 base{xpAnchor_.x + 160.0F, xpAnchor_.y};   // right of the level line, clear of the panel title
  float stack = 0;
  for (const auto& d : effects_.drops()) {
    const std::string text = game::dropText(d.amount);
    const ImVec2 at{base.x, base.y - d.rise + stack};   // each extra drop sits one line lower, so none is clipped
    const auto alpha = static_cast<ImU32>(std::clamp(d.alpha, 0.0F, 1.0F) * 255.0F) << 24;
    dl->AddText({at.x + 1, at.y + 1}, 0x00000000U | alpha, text.c_str());
    dl->AddText(at, (ui::toU32(ui::PaletteColor::TextYellow) & 0x00FFFFFFU) | alpha, text.c_str());
    stack += 12.0F;   // several drops at once don't overprint each other
  }
}

void App::drawShell() {
  FrameCtx ctx;
  ctx.dt = static_cast<double>(ImGui::GetIO().DeltaTime);
  ctx.kiroween = kiroween_;
  ctx.reduceMotion = settings_.reduceMotion;
  ctx.services = services_ ? &*services_ : nullptr;
  ctx.nav = &screens_;
  ctx.avatars = avatars_.get();
  ctx.userId = currentUserId_.value_or("");
  ctx.userLevel = currentLevel_;
  ctx.isKeeper = isKeeper_;
  ctx.exportPortrait = [this](const domain::Appearance& a, const domain::Loadout& l, domain::Facing f,
                              const std::string& name) {
    if (!avatars_) return;
    if (SDL_Surface* portrait = avatars_->exportSurface(a, l, f, 4)) {
      pngExporter_.start(window_, portrait, platform::safePngName(name));
      SDL_DestroySurface(portrait);  // the exporter keeps its own copy
    }
  };
  ctx.reduceMotionDefault = platform::prefersReducedMotion();
  ctx.contextRequest = &contextReq_;
  ctx.onSignedIn = [this](const std::string& userId, bool remember) {
    if (auto user = services_->profile.getUser(userId)) {
      adoptUser(userId, user->settings);
    }
    if (remember) services_->profile.rememberUser(userId);
  };

  const ScreenId before = screens_.currentId().value_or(ScreenId::Login);
  // Leaving the current screen ("To town", Escape) is allowed anywhere past the World, except the
  // Costumer before the first Adventurer exists (Req 2.6). Escape is read before the screen draws, so a
  // text box that Escape is about to release still counts as busy, and a context menu Escape is about to
  // close keeps the key to itself.
  bool canLeave = currentUserId_ && before != ScreenId::Login && before != ScreenId::World;
  if (canLeave && before == ScreenId::Costumer && services_) {
    auto active = services_->adventurers.getActive(*currentUserId_);
    canLeave = active && *active;
  }
  const bool escapeBack = canLeave && !ImGui::IsAnyItemActive() && !contextMenu_.isOpen() &&
                          (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                           ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false));
  if (beginRegion("##viewport", ui::VIEWPORT)) {
    if (Screen* screen = screens_.current()) {
      screen->update(ctx);
      screen->draw(ctx);
    }
  }
  ImGui::End();

  // The Tab_Panel (Settings) is shown only once a user is signed in (i.e. not on the login screen).
  if (beginRegion("##tabpanel", ui::TAB_PANEL)) {
    if (before != ScreenId::Login && currentUserId_) {
      const auto result = ui::drawTabPanel(settings_, isKeeper_, xpStatus(), canLeave);
      if (result.toTown) screens_.navigate(ScreenId::World);  // the World is a hub: history resets
      xpAnchor_ = result.xpAnchor;
      if (result.settingsChanged) {
        applySettings(result.settings);
        if (services_) {
          if (auto saved = services_->profile.updateSettings(*currentUserId_, result.settings); !saved) {
            logError("Save settings", saved.error());
          }
        }
      }
      if (result.setKeeper && services_) {
        if (auto r = services_->keeper.setKeeper(*currentUserId_, *result.setKeeper)) {
          isKeeper_ = *result.setKeeper;
          ui::pushGameMessage(isKeeper_ ? "You are now a Codex Keeper." : "You are no longer a Codex Keeper.");
        } else {
          ui::pushGameMessage(r.error().message);
        }
      }
      if (result.logOut) {
        ui::resetTabPanel();
        services_->profile.forgetRemembered();
        currentUserId_.reset();
        applySettings(domain::Settings{});
        screens_.navigate(ScreenId::Login);
      }
    } else {
      ui::StonePanel panel{"##tabs", "AgentScape"};
      ui::ShadowText("Sign in to begin.", ui::PaletteColor::Parchment);
    }
  }
  ImGui::End();

  if (beginRegion("##chatbox", ui::CHATBOX)) {
    if (chatbox_) chatbox_->draw();
  }
  ImGui::End();

  if (escapeBack) screens_.back();

  // A screen asked to open the context menu this frame.
  if (contextReq_.requested) {
    contextMenu_.open(contextReq_.actions, contextReq_.examineText, contextReq_.x, contextReq_.y);
    contextReq_ = app::ContextRequest{};
  }
  if (auto chosen = contextMenu_.draw()) {
    if (chosen->id == "examine") {
      ui::pushGameMessage(chosen->target);  // Req 4.3: Examine prints to the Chatbox
    } else if (Screen* screen = screens_.current()) {
      screen->onContextAction(ctx, *chosen);
    }
  }

  ui::HoverText({static_cast<float>(ui::VIEWPORT.x), static_cast<float>(ui::VIEWPORT.y)});
  effects_.step(static_cast<double>(ImGui::GetIO().DeltaTime), settings_.reduceMotion);
  drawEffects();
}

void App::saveSnapshot() {
  // Reads the 765x503 frame texture (the current render target), not the scaled window.
  SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr);
  if (shot == nullptr) {
    SDL_Log("Snapshot failed: %s", SDL_GetError());
    return;
  }
  if (IMG_SavePNG(shot, config_.snapshotPath->c_str())) {
    SDL_Log("Snapshot saved to %s", config_.snapshotPath->c_str());
  } else {
    SDL_Log("Snapshot could not be written: %s", SDL_GetError());
  }
  SDL_DestroySurface(shot);
}

SDL_AppResult App::iterate() {
  updateData();
  persistence_.tick();
  switch (pngExporter_.poll()) {
    case platform::ExportOutcome::Saved: ui::pushGameMessage("Your portrait has been saved."); break;
    case platform::ExportOutcome::Failed: ui::pushGameMessage("<col=orange>Your portrait could not be saved.</col>"); break;
    case platform::ExportOutcome::Cancelled:
    case platform::ExportOutcome::None: break;  // cancelling does nothing (Req 8.2)
  }
  frame_->updateLayout(window_);

  screens_.applyPending();

  ImGui_ImplSDLRenderer3_NewFrame();
  if (!fontTextureNearest_) {
    // The backend creates the font atlas with linear filtering; every texture must be nearest.
    if (auto* fontTex = reinterpret_cast<SDL_Texture*>(static_cast<std::uintptr_t>(ImGui::GetIO().Fonts->TexID))) {
      SDL_SetTextureScaleMode(fontTex, SDL_SCALEMODE_NEAREST);
      fontTextureNearest_ = true;
    }
  }
  ImGui_ImplSDL3_NewFrame();
  // On Windows, macOS and X11 the SDL3 backend also feeds the OS mouse position every frame while the
  // window is focused (its "global mouse state" fallback), in raw window pixels. That bypasses the
  // conversion in handleEvent, so hovers flicker and clicks miss whenever the frame is scaled. Re-feed
  // the current position in frame pixels; ImGui applies the last position event queued in a frame.
  if (SDL_GetMouseFocus() == window_) {
    float mx = 0.0F;
    float my = 0.0F;
    SDL_GetMouseState(&mx, &my);
    const auto p = frame_->toFrame(mx, my);
    ImGui::GetIO().AddMousePosEvent(p.x, p.y);
  }
  ImGui::GetIO().DisplaySize = {static_cast<float>(domain::FRAME_W), static_cast<float>(domain::FRAME_H)};
  ImGui::GetIO().DisplayFramebufferScale = {1.0F, 1.0F};
  ImGui::NewFrame();
  ui::beginFrame();
  ImGui::PushFont(ui::activeFonts().body);
  ImGui::GetBackgroundDrawList()->AddRectFilled(
      {0, 0}, {static_cast<float>(domain::FRAME_W), static_cast<float>(domain::FRAME_H)},
      ui::toU32(ui::PaletteColor::StoneDark));
  switch (dataState_) {
    case DataState::WaitingForStorage:
      drawLoading();
      break;
    case DataState::LoadFailed:
      drawLoadFailed();
      break;
    case DataState::Ready:
      drawShell();
      break;
  }
  ImGui::PopFont();
  ImGui::Render();

  frame_->beginFrame();
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
  if (avatars_) avatars_->endFrame();
  if (config_.snapshotPath && framesRendered_ + 1 == SNAPSHOT_FRAME) saveSnapshot();
  if (config_.exportAvatarPath && framesRendered_ == 0) {
    // Dev QA: prove the 4x transparent portrait export path without a dialog.
    bool ok = false;
    if (avatars_) {
      domain::Appearance look;
      look.hair = 2;
      look.colors = {3, 5, 4, 2, 1};
      if (SDL_Surface* portrait = avatars_->exportSurface(look, domain::defaultLoadout(), domain::Facing::S, 4)) {
        ok = platform::writePng(portrait, *config_.exportAvatarPath);
        SDL_Log("Exported %dx%d portrait to %s: %s", portrait->w, portrait->h, config_.exportAvatarPath->c_str(),
                ok ? "ok" : "FAILED");
        SDL_DestroySurface(portrait);
      }
    }
    if (!ok) return SDL_APP_FAILURE;
  }
  frame_->present();

  ++framesRendered_;
  if (quitRequested_) return SDL_APP_SUCCESS;

  if (config_.smokePathDir) {
    if (framesRendered_ >= 2 && runPathStep()) return demoFailed_ ? SDL_APP_FAILURE : SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
  }

  if (config_.smokeCodexDir) {
    if (framesRendered_ >= 2 && runCodexStep()) return demoFailed_ ? SDL_APP_FAILURE : SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
  }

  if (config_.smokeCostumerDir) {
    if (framesRendered_ >= 2 && runCostumerStep()) return demoFailed_ ? SDL_APP_FAILURE : SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
  }

  if (config_.smokeDemoDir) {
    // One demo step per frame, so each step's screen is what the previous frame rendered.
    if (framesRendered_ >= 2 && runDemoStep()) {
      return demoFailed_ ? SDL_APP_FAILURE : SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
  }

  if (config_.headlessSmoke && framesRendered_ >= SMOKE_FRAMES) {
    // Smoke runs must fail loudly if storage didn't come up.
    return dataState_ == DataState::Ready ? SDL_APP_SUCCESS : SDL_APP_FAILURE;
  }
  return SDL_APP_CONTINUE;
}

// The documented demo path (testing.md), end to end, in the real app:
//   guest login -> World -> Costumer: create an Adventurer, claim and equip the Kiroween gear -> Codex card ->
//   write a Quest Log through the real quest scroll -> "+250 XP" and the level-up -> Hiscores.
// Phase 2 is a SEPARATE process on the same database (what a page reload is on the web): everything must still be
// there. It saves a snapshot of every screen. It drives screens by navigation and services by calls; it does not
// simulate mouse or keyboard input.
bool App::runPathStep() {
  const auto fail = [&](const char* why) {
    SDL_Log("SMOKE FAIL: %s", why);
    demoFailed_ = true;
    return true;
  };
  const auto shoot = [&](const char* file) {
    SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr);
    if (shot == nullptr) return;
    const std::string path = *config_.smokePathDir + "/" + file;
    if (!IMG_SavePNG(shot, path.c_str())) SDL_Log("Snapshot write failed: %s", SDL_GetError());
    SDL_DestroySurface(shot);
  };
  if (dataState_ != DataState::Ready || !services_) return fail("store not ready");
  auto& svc = *services_;
  const auto geared = [](const services::Adventurer& a) {
    const auto head = a.loadout.find(domain::Slot::Head);
    const auto weapon = a.loadout.find(domain::Slot::Weapon);
    return head != a.loadout.end() && head->second == "jack_mask" && weapon != a.loadout.end() && weapon->second == "reapers_scythe";
  };

  if (config_.smokePhase == 1) {
    switch (demoStep_++) {
      case 0: {   // the login screen, then "Play as Guest"
        if (screens_.currentId() != ScreenId::Login) return fail("did not start on the login screen");
        shoot("p1-login.png");
        auto guest = svc.profile.createGuest();
        if (!guest) return fail("createGuest failed");
        svc.profile.signIn(guest->id);
        adoptUser(guest->id, guest->settings);
        if (currentLevel_ != 13) return fail("a Guest should start at Questing level 13");
        store_->db.prepare("INSERT OR REPLACE INTO app_state(key, value) VALUES ('smoke_user_id', ?)").bind(1, guest->id).run();
        screens_.navigate(ScreenId::World);
        return false;
      }
      case 1:
        if (screens_.currentId() != ScreenId::World) return fail("did not reach the World");
        shoot("p2-world.png");
        screens_.navigate(ScreenId::Costumer);
        return false;
      case 2: {   // no Adventurer yet: the Costumer asks for a name; create one and dress it in the Kiroween gear
        if (screens_.currentId() != ScreenId::Costumer) return fail("did not reach the Costumer");
        shoot("p3-costumer-create.png");
        const std::string uid = *currentUserId_;
        auto created = svc.adventurers.createAdventurer(uid, "Wanderer");
        if (!created || !created->isActive) return fail("createAdventurer failed or not active");
        if (!svc.adventurers.claimKiroweenItem(uid, "jack_mask", /*kiroweenActive=*/true, currentLevel_)) return fail("claim mask");
        if (!svc.adventurers.claimKiroweenItem(uid, "reapers_scythe", true, currentLevel_)) return fail("claim scythe");
        auto owned = svc.adventurers.ownedItems(uid);
        if (!owned) return fail("ownedItems failed");
        auto gear = domain::equip(created->loadout, "jack_mask", *owned);
        if (gear) gear = domain::equip(*gear, "reapers_scythe", *owned);
        if (!gear) return fail("equip failed");
        if (!svc.adventurers.saveAdventurer(uid, created->id, created->appearance, *gear)) return fail("save failed");
        screens_.navigate(ScreenId::World);
        screens_.navigate(ScreenId::Costumer);
        return false;
      }
      case 3: {
        auto active = svc.adventurers.getActive(*currentUserId_);
        if (!active || !*active || !geared(**active)) return fail("the Kiroween gear is not equipped");
        shoot("p4-costumer-equipped.png");
        screens_.navigate(ScreenId::World);
        screens_.navigate(ScreenId::Codex, {{"q", "gryphon"}});
        return false;
      }
      case 4:   // search the Codex, open a familiar's card
        if (screens_.currentId() != ScreenId::Codex) return fail("did not reach the Codex");
        shoot("p5-codex.png");
        screens_.navigate(ScreenId::CodexCard, {{"agent", "gryphon"}});
        return false;
      case 5:
        if (screens_.currentId() != ScreenId::CodexCard) return fail("did not reach the examine card");
        shoot("p6-codex-card.png");
        screens_.navigate(ScreenId::QuestScroll, {{"agent", "gryphon"}, {"demo", "1"}, {"step", "3"}});   // the story step, filled in
        return false;
      case 6:
        if (screens_.currentId() != ScreenId::QuestScroll) return fail("did not reach the quest scroll");
        shoot("p7-quest-scroll.png");
        screens_.navigate(ScreenId::QuestScroll, {{"agent", "gryphon"}, {"demo", "1"}, {"submit", "1"}});   // and saved
        return false;
      case 7:   // saved on its first frame; one more frame so the effects are drawn
        return false;
      case 8: {
        const std::string uid = *currentUserId_;
        auto logId = svc.quests.findOwn(uid, "seed:agent:gryphon", "dev");
        if (!logId || !*logId) return fail("the quest scroll did not save a Quest Log");
        if (totalXp_ != domain::guestSeedXp() + 250) return fail("the Quest Log should have paid exactly 250 XP");
        if (currentLevel_ != 14) return fail("a Guest's first Quest Log should take it from level 13 to 14");
        if (effects_.drops().empty() || effects_.bursts().empty()) return fail("expected the +250 XP drop and the level-up fireworks");
        shoot("p8-level-up.png");
        screens_.navigate(ScreenId::Hiscores, {{"tab", "adventurers"}, {"q", "Wanderer"}});
        return false;
      }
      case 9: {
        if (screens_.currentId() != ScreenId::Hiscores) return fail("did not reach the Hiscores");
        auto board = svc.hiscores.adventurers(0, "Wanderer");
        if (!board || !board->match) return fail("the Hiscores should list the new Adventurer");
        const auto& row = board->rows[*board->match];
        if (row.xp != totalXp_ || row.level != 14) return fail("the Hiscores row should show the new XP and level 14");
        shoot("p9-hiscores.png");
        SDL_Log("SMOKE OK: demo path phase 1 (level 14, rank %d)", row.rank);
        return true;
      }
      default: return true;
    }
  }

  // Phase 2: a brand-new process on the same database, as after a page reload.
  switch (demoStep_++) {
    case 0: {
      auto st = store_->db.prepare("SELECT value FROM app_state WHERE key = 'smoke_user_id'");
      if (!st.step()) return fail("phase 1 left no user behind");
      const std::string uid = st.columnText(0);
      auto user = svc.profile.getUser(uid);
      if (!user) return fail("the Guest from phase 1 is gone");
      adoptUser(uid, user->settings);
      if (totalXp_ != domain::guestSeedXp() + 250 || currentLevel_ != 14) return fail("the XP and level did not survive the restart");
      auto active = svc.adventurers.getActive(uid);
      if (!active || !*active || !geared(**active)) return fail("the equipped gear did not survive the restart");
      auto logId = svc.quests.findOwn(uid, "seed:agent:gryphon", "dev");
      if (!logId || !*logId) return fail("the Quest Log did not survive the restart");
      auto board = svc.hiscores.adventurers(0, "Wanderer");
      if (!board || !board->match || board->rows[*board->match].level != 14) return fail("the Hiscores did not keep the Adventurer");
      screens_.navigate(ScreenId::World);
      screens_.navigate(ScreenId::Tavern);
      return false;
    }
    case 1:
      if (screens_.currentId() != ScreenId::Tavern) return fail("did not reach the Tavern after the restart");
      shoot("p10-restored-tavern.png");
      SDL_Log("SMOKE OK: demo path phase 2 (everything survived a restart)");
      return true;
    default: return true;
  }
}

// The agent-codex flow, one step per frame so each step's screen is what the previous frame rendered:
//   guest -> Codex (searching "lanternfold" finds 2) -> examine card -> the same card in Debugging (narrower)
//   -> write a Boon, another user votes, you can't vote on your own -> Proving Grounds remembers 2 Agents
//   -> propose, a Keeper approves, the Codex grows -> Graveyard (Kiroween) holds 1 to 13 tombstones.
// It drives the real screens by navigation and the real services by calls. It does NOT simulate mouse or
// keyboard input, so it proves that each screen loads, renders and persists what the services give it.
bool App::runCodexStep() {
  const auto fail = [&](const char* why) {
    SDL_Log("SMOKE FAIL: %s", why);
    demoFailed_ = true;
    return true;
  };
  const auto shoot = [&](const char* file) {
    SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr);
    if (shot == nullptr) return;
    const std::string path = *config_.smokeCodexDir + "/" + file;
    if (!IMG_SavePNG(shot, path.c_str())) SDL_Log("Snapshot write failed: %s", SDL_GetError());
    SDL_DestroySurface(shot);
  };
  if (dataState_ != DataState::Ready || !services_) return fail("store not ready");
  auto& svc = *services_;
  const std::string me = currentUserId_.value_or("");
  using domain::QuestType;

  switch (demoStep_++) {
    case 0: {
      auto guest = svc.profile.createGuest();
      if (!guest) return fail("createGuest failed");
      svc.profile.signIn(guest->id);
      adoptUser(guest->id, guest->settings);
      screens_.navigate(ScreenId::World);
      screens_.navigate(ScreenId::Codex, {{"q", "lanternfold"}});
      return false;
    }
    case 1: {
      if (screens_.currentId() != ScreenId::Codex) return fail("did not reach the Codex");
      domain::ListQuery q;
      q.text = "lanternfold";
      auto page = svc.codex.list(q, std::nullopt);
      if (!page || page->total != 2) return fail("searching 'lanternfold' should find 2 familiars");
      shoot("x1-codex-search.png");
      screens_.navigate(ScreenId::CodexCard, {{"agent", "gryphon"}});
      return false;
    }
    case 2: {
      if (screens_.currentId() != ScreenId::CodexCard) return fail("did not reach the examine card");
      auto card = svc.codex.card("gryphon", std::nullopt);
      if (!card || !*card || (*card)->levels.overallCount == 0) return fail("Gryphon's card has no Quest Logs");
      shoot("x2-card.png");
      screens_.navigate(ScreenId::CodexCard, {{"agent", "gryphon"}, {"scope", "DEBUGGING"}});
      return false;
    }
    case 3: {
      if (screens_.currentId() != ScreenId::CodexCard) return fail("did not reach the Debugging card");
      auto all = svc.codex.card("gryphon", std::nullopt);
      auto dbg = svc.codex.card("gryphon", QuestType::Debugging);
      if (!all || !dbg || !*all || !*dbg) return fail("card lookup failed");
      if ((*dbg)->levels.overallCount >= (*all)->levels.overallCount) return fail("Debugging should narrow the Quest Logs");
      shoot("x3-card-debugging.png");

      // Boons and votes.
      auto other = svc.profile.createGuest();
      if (!other) return fail("second guest failed");
      services::NewNote note;
      note.agentId = (*all)->agent.id;
      note.kind = services::NoteKind::Boon;
      note.text = "Smoke test remark: tidy, readable answers.";
      auto id = svc.notes.createNote(me, note);
      if (!id) return fail("createNote failed");
      if (svc.notes.voteOnNote(me, *id, domain::VoteValue::Agree)) return fail("voting on your own Note was allowed");
      if (!svc.notes.voteOnNote(other->id, *id, domain::VoteValue::Agree)) return fail("another user's vote failed");
      auto boons = svc.codex.notes((*all)->agent.id, services::NoteKind::Boon);
      if (!boons) return fail("reading Boons failed");
      const auto mine = std::find_if(boons->begin(), boons->end(), [&](const services::NoteView& n) { return n.id == *id; });
      if (mine == boons->end() || mine->agree != 1) return fail("the new Boon should show with one agree vote");

      screens_.navigate(ScreenId::Proving, {{"add", "gryphon"}, {"scope", "DEBUGGING"}});
      return false;
    }
    case 4:
      if (screens_.currentId() != ScreenId::Proving) return fail("did not reach the Proving Grounds");
      screens_.navigate(ScreenId::Proving, {{"add", "cairn"}});   // a fresh visit restores Gryphon + Debugging
      return false;
    case 5: {
      if (screens_.currentId() != ScreenId::Proving) return fail("did not return to the Proving Grounds");
      auto saved = svc.proving.restore(me);
      if (!saved || saved->slugs != std::vector<std::string>{"gryphon", "cairn"}) return fail("the comparison was not remembered");
      if (saved->questType != std::optional{QuestType::Debugging}) return fail("the Quest Type was not remembered");
      auto cols = svc.codex.compareColumns(saved->slugs, saved->questType);
      if (!cols || cols->size() != 2) return fail("compare should have 2 columns");
      shoot("x4-proving.png");

      // Propose, approve, and the Codex grows.
      domain::AgentInput in;
      in.name = "Smoke Familiar";
      in.vendor = "Candlewick";
      in.examine = "A made-up familiar proposed by the smoke test.";
      auto proposed = svc.agents.proposeAgent(me, in);
      if (!proposed) return fail("proposeAgent failed");
      auto before = svc.codex.list({}, std::nullopt);
      if (!before || before->total != 14) return fail("a pending proposal must not be in the Codex");
      if (!svc.keeper.setKeeper(me, true)) return fail("setKeeper failed");
      if (!svc.keeper.moderateAgent(me, *proposed, true)) return fail("approving the proposal failed");
      auto after = svc.codex.list({}, std::nullopt);
      if (!after || after->total != 15) return fail("the approved familiar should join the Codex");
      svc.keeper.setKeeper(me, false);

      screens_.navigate(ScreenId::Graveyard);
      return false;
    }
    case 6: {
      if (screens_.currentId() != ScreenId::Graveyard) return fail("did not reach the Graveyard");
      auto stones = svc.codex.tombstones();
      if (!stones || stones->empty() || stones->size() > domain::MAX_TOMBSTONES) {
        return fail("the Graveyard should hold 1 to 13 tombstones");
      }
      shoot("x5-graveyard.png");
      smokeTombstones_ = stones->size();

      // Write a Quest Log through the real screen: a Guest (level 13) with an Adventurer, the form prefilled by the
      // dev "demo" switch and saved on its first frame. The XP, the level-up and the effects come from the events.
      if (!svc.adventurers.createAdventurer(me, "Smokey")) return fail("createAdventurer failed");
      // The Guest started at level 13 (seed XP); the Note (+50) and the approved proposal (+500) earlier in this flow
      // have raised it since, so remember where it stands now and check the Quest Log adds exactly 250 on top.
      if (totalXp_ != domain::guestSeedXp() + 50 + 500) return fail("the Guest's XP should be its seed plus a Note and an approval");
      if (currentLevel_ != domain::levelForXp(totalXp_)) return fail("the level should follow the XP");
      smokeXpBefore_ = totalXp_;
      smokeLevelBefore_ = currentLevel_;
      screens_.navigate(ScreenId::QuestScroll, {{"agent", "gryphon"}, {"demo", "1"}, {"submit", "1"}});
      return false;
    }
    case 7:   // the screen loaded and saved on its first draw; wait one more frame for the effects to be drawn
      return false;
    case 8: {
      auto logs = svc.quests.findOwn(me, "seed:agent:gryphon", "dev");
      if (!logs || !*logs) return fail("the quest scroll did not save a Quest Log");
      auto record = svc.quests.get(**logs);
      if (!record || !*record || (*record)->input.title != "A solid helper") return fail("the saved Quest Log is wrong");
      if (totalXp_ != smokeXpBefore_ + 250) return fail("the Quest Log should have paid exactly 250 XP");
      if (currentLevel_ != domain::levelForXp(totalXp_)) return fail("the level should follow the XP");
      if (currentLevel_ <= smokeLevelBefore_) return fail("250 XP should take this Guest up a level");
      if (effects_.drops().empty()) return fail("expected a floating +250 XP drop");
      if (effects_.bursts().empty()) return fail("expected level-up fireworks");
      if (screens_.currentId() == ScreenId::QuestScroll) return fail("the scroll should close after saving");
      shoot("x6-level-up.png");
      screens_.navigate(ScreenId::Tavern);
      return false;
    }
    case 9: {   // the Tavern board shows the new Quest Log; open it
      if (screens_.currentId() != ScreenId::Tavern) return fail("did not reach the Tavern");
      services::QuestListQuery q;
      q.agentId = std::string("seed:agent:gryphon");
      auto page = svc.quests.list(q, me);
      if (!page || page->logs.empty()) return fail("the Tavern board should hold the new Quest Log");
      const auto mine = std::find_if(page->logs.begin(), page->logs.end(), [&](const services::QuestLogRecord& l) { return l.authorId == me; });
      if (mine == page->logs.end()) return fail("the board should list my own Quest Log");
      auto top = svc.quests.topHelpful("seed:agent:gryphon", 5);
      if (!top || top->empty()) return fail("Gryphon's card should list its Quest Logs");
      smokeQuestLogId_ = mine->id;
      shoot("x7-tavern.png");
      screens_.navigate(ScreenId::QuestDetail, {{"id", mine->id}});
      return false;
    }
    case 10: {  // the detail screen is up; another adventurer marks it helpful and the author is paid
      if (screens_.currentId() != ScreenId::QuestDetail) return fail("did not reach the Quest Log detail");
      shoot("x8-quest-detail.png");
      auto other = svc.profile.createGuest();
      if (!other) return fail("second guest failed");
      const auto before = totalXp_;
      auto voted = svc.quests.toggleHelpful(other->id, smokeQuestLogId_);
      if (!voted || !*voted) return fail("another adventurer's Helpful vote failed");
      if (totalXp_ != before + 25) return fail("a Helpful vote should have paid the author 25 XP");
      if (svc.quests.toggleHelpful(me, smokeQuestLogId_)) return fail("voting on your own Quest Log was allowed");
      screens_.navigate(ScreenId::Hiscores, {{"tab", "adventurers"}, {"q", "Smokey"}});
      return false;
    }
    case 11: {  // the Hiscores know about the Guest, at the rank its XP deserves
      if (screens_.currentId() != ScreenId::Hiscores) return fail("did not reach the Hiscores");
      auto board = svc.hiscores.adventurers(0, "Smokey");
      if (!board || !board->match) return fail("the Hiscores should list the Guest's Adventurer by name");
      const auto& row = board->rows[*board->match];
      if (row.xp != totalXp_ || row.level != currentLevel_) return fail("the Hiscores row should match the Guest's XP and level");
      int above = 0;
      for (const auto& r : board->rows) above += r.xp > row.xp ? 1 : 0;
      if (row.rank != 1 + above) return fail("the Guest's rank should be 1 plus the people with more XP");
      auto agents = svc.hiscores.agents({}, 0, "");
      if (!agents || agents->rows.empty() || agents->rows.front().rank != 1) return fail("the Agent board should start at rank 1");
      shoot("x9-hiscores.png");
      SDL_Log("SMOKE OK: codex flow completed (%zu tombstones, level %d, rank %d)", smokeTombstones_, currentLevel_, row.rank);
      return true;
    }
    default:
      return true;
  }
}

bool App::runCostumerStep() {
  const auto fail = [&](const char* why) {
    SDL_Log("SMOKE FAIL: %s", why);
    demoFailed_ = true;
    return true;
  };
  const auto shoot = [&](const char* file) {
    SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr);
    if (shot == nullptr) return;
    const std::string path = *config_.smokeCostumerDir + "/" + file;
    if (!IMG_SavePNG(shot, path.c_str())) SDL_Log("Snapshot write failed: %s", SDL_GetError());
    SDL_DestroySurface(shot);
  };
  if (dataState_ != DataState::Ready || !services_) return fail("store not ready");
  auto& svc = services_->adventurers;
  const auto isEquipped = [](const services::Adventurer& a) {
    const auto head = a.loadout.find(domain::Slot::Head);
    const auto weapon = a.loadout.find(domain::Slot::Weapon);
    return head != a.loadout.end() && head->second == "jack_mask" && weapon != a.loadout.end() &&
           weapon->second == "reapers_scythe";
  };

  if (config_.smokePhase == 1) {
    switch (demoStep_++) {
      case 0: {  // a fresh Guest signs in (level 13) and, having no Adventurer, lands in the Costumer
        auto guest = services_->profile.createGuest();
        if (!guest) return fail("createGuest failed");
        services_->profile.signIn(guest->id);
        adoptUser(guest->id, guest->settings);
        if (currentLevel_ != 13) return fail("a Guest should start at Questing level 13");
        store_->db.prepare("INSERT OR REPLACE INTO app_state(key, value) VALUES ('smoke_user_id', ?)")
            .bind(1, guest->id)
            .run();
        screens_.navigate(ScreenId::World);
        screens_.navigate(ScreenId::Costumer);
        return false;
      }
      case 1: {
        if (screens_.currentId() != ScreenId::Costumer) return fail("did not reach the Costumer");
        shoot("c1-create.png");  // the "name your first Adventurer" form
        const std::string uid = *currentUserId_;
        auto created = svc.createAdventurer(uid, "Wanderer");
        if (!created || !created->isActive) return fail("createAdventurer failed or not active");
        // Claim the two Kiroween items (the Scythe needs level 13) and equip them through the domain rules.
        if (!svc.claimKiroweenItem(uid, "jack_mask", /*kiroweenActive=*/true, currentLevel_)) return fail("claim mask");
        if (!svc.claimKiroweenItem(uid, "reapers_scythe", true, currentLevel_)) return fail("claim scythe");
        auto owned = svc.ownedItems(uid);
        if (!owned) return fail("ownedItems failed");
        auto gear = domain::equip(created->loadout, "jack_mask", *owned);
        if (!gear) return fail("equip mask");
        gear = domain::equip(*gear, "reapers_scythe", *owned);
        if (!gear) return fail("equip scythe");
        if (!svc.saveAdventurer(uid, created->id, created->appearance, *gear)) return fail("save failed");
        screens_.navigate(ScreenId::World);   // re-enter so the Costumer reloads from the database
        screens_.navigate(ScreenId::Costumer);
        return false;
      }
      case 2: {
        auto active = svc.getActive(*currentUserId_);
        if (!active || !*active || !isEquipped(**active)) return fail("the saved gear is not equipped");
        shoot("c2-equipped.png");
        SDL_Log("SMOKE OK: phase 1 (created, claimed, equipped, saved)");
        return true;
      }
      default: return true;
    }
  }

  // Phase 2: a brand-new process on the same database.
  switch (demoStep_++) {
    case 0: {
      auto st = store_->db.prepare("SELECT value FROM app_state WHERE key = 'smoke_user_id'");
      if (!st.step()) return fail("phase 1 left no user behind");
      const std::string uid = st.columnText(0);
      auto user = services_->profile.getUser(uid);
      if (!user) return fail("the Guest from phase 1 is gone");
      adoptUser(uid, user->settings);
      screens_.navigate(ScreenId::World);
      screens_.navigate(ScreenId::Costumer);
      return false;
    }
    case 1: {
      auto active = svc.getActive(*currentUserId_);
      if (!active || !*active) return fail("the Adventurer did not survive the restart");
      if ((*active)->name != "Wanderer") return fail("wrong Adventurer after restart");
      if (!isEquipped(**active)) return fail("the gear did not survive the restart");
      if (screens_.currentId() != ScreenId::Costumer) return fail("not on the Costumer");
      shoot("c3-restored.png");
      SDL_Log("SMOKE OK: phase 2 (still equipped after restart)");
      return true;
    }
    default: return true;
  }
}

void App::snapshotTo(const std::string& file) {
  SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr);
  if (shot == nullptr) {
    SDL_Log("Demo snapshot failed: %s", SDL_GetError());
    return;
  }
  const std::string path = *config_.smokeDemoDir + "/" + file;
  if (!IMG_SavePNG(shot, path.c_str())) SDL_Log("Demo snapshot write failed: %s", SDL_GetError());
  SDL_DestroySurface(shot);
}

bool App::runDemoStep() {
  // The demo path (game-shell task 11): guest login -> World -> open the Tavern context menu -> Enter.
  const auto fail = [&](const char* why) {
    SDL_Log("SMOKE FAIL: %s", why);
    demoFailed_ = true;
    return true;
  };
  switch (demoStep_++) {
    case 0:
      if (dataState_ != DataState::Ready) return fail("store not ready at login");
      if (screens_.currentId() != ScreenId::Login) return fail("did not start on the login screen");
      snapshotTo("01-login.png");
      return false;
    case 1: {
      // "Play as Guest"
      auto guest = services_->profile.createGuest();
      if (!guest) return fail("createGuest failed");
      auto signedIn = services_->profile.signIn(guest->id);
      if (!signedIn) return fail("guest signIn failed");
      adoptUser(signedIn->id, signedIn->settings);
      screens_.navigate(ScreenId::World);
      return false;
    }
    case 2:
      if (screens_.currentId() != ScreenId::World) return fail("did not reach the World screen");
      snapshotTo("02-world.png");
      // Open the Tavern hotspot's context menu.
      for (const auto* h : game::visibleHotspots(kiroween_)) {
        if (h->id == "tavern") {
          std::vector<domain::MenuAction> actions{
              {std::string(h->id), std::string(h->defaultAction) + " " + std::string(h->name), std::string(h->id), true}};
          contextReq_.open(actions, std::string(h->examine), 200, 120);
        }
      }
      return false;
    case 3:
      if (!contextMenu_.isOpen()) return fail("context menu did not open on the Tavern");
      snapshotTo("03-context-menu.png");
      // Confirm the default action ("Enter Tavern").
      if (auto chosen = contextMenu_.confirmSelected(); !chosen || chosen->id != "tavern") {
        return fail("confirming the menu did not yield the Tavern action");
      }
      ui::pushGameMessage("Enter Tavern...");
      return false;
    case 4:
      snapshotTo("04-entered.png");
      SDL_Log("SMOKE OK: demo path completed");
      return true;
    default:
      return true;
  }
}

}  // namespace as::app
