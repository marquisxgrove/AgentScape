#include "app/app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <SDL3_image/SDL_image.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "game/hotspots.hpp"
#include "platform/accessibility.hpp"
#include "platform/paths.hpp"
#include "platform/web_url.hpp"
#include "screens/codex/codex_card_screen.hpp"
#include "screens/codex/codex_screen.hpp"
#include "screens/codex/keeper_screen.hpp"
#include "screens/codex/propose_screen.hpp"
#include "screens/graveyard/graveyard_screen.hpp"
#include "screens/codex/proving_screen.hpp"
#include "screens/costumer/costumer_screen.hpp"
#include "screens/login/login_screen.hpp"
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
                             services::ProvingService{store_->db, clock_}});
  chatbox_.emplace(bus_);
  onStoreReady();
  dataState_ = DataState::Ready;
}

void App::onStoreReady() {
  if (auto removed = services_->profile.cleanupGuests(); removed && *removed > 0) {
    SDL_Log("Removed %d stale Guest profile(s)", *removed);
  }
  registerScreens();
  signInRemembered();  // Req 2.1: auto sign-in a remembered (non-Guest) user
}

void App::signInRemembered() {
  auto remembered = services_->profile.rememberedUser();
  if (!remembered || !*remembered) return;
  auto user = services_->profile.signIn((*remembered)->id);
  if (!user) return;
  adoptUser(user->id, user->settings);
  screens_.navigate(ScreenId::World);
  if (postLoginScreen_) screens_.navigate(*postLoginScreen_, postLoginParams_);
  SDL_Log("Signed in remembered user %s", user->name.c_str());
}

void App::adoptUser(const std::string& userId, const domain::Settings& settings) {
  currentUserId_ = userId;
  applySettings(settings);
  // Placeholder until the quest-logs spec supplies XP: Guests start at Questing level 13 (quest-logs
  // Req 9.1), everyone else at 1. The unlock sync below is the same call the level-up flow will make.
  currentLevel_ = 1;
  isKeeper_ = false;
  if (services_) {
    if (auto keeper = services_->keeper.isKeeper(userId)) isKeeper_ = *keeper;
    if (auto user = services_->profile.getUser(userId); user && user->kind == services::UserKind::Guest) {
      currentLevel_ = 13;
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
    for (const char* name : {"codex", "codex-table", "codex-search", "codex-card", "codex-card-debug", "codex-card-notes", "codex-card-compose", "codex-propose", "codex-propose-error", "codex-keeper", "proving", "proving-pick", "graveyard", "graveyard-list"}) {
      if (*config_.startScreen == name) {
        const std::string n = name;
        postLoginScreen_ = n.rfind("codex-card", 0) == 0 ? ScreenId::CodexCard : (n.rfind("codex-propose", 0) == 0 ? ScreenId::Propose : (n == "codex-keeper" ? ScreenId::Keeper : (n.rfind("proving", 0) == 0 ? ScreenId::Proving : (n.rfind("graveyard", 0) == 0 ? ScreenId::Graveyard : ScreenId::Codex))));
        if (n == "graveyard-list") postLoginParams_["view"] = "list";
        if (n.rfind("proving", 0) == 0) postLoginParams_["with"] = "gryphon,cairn,gryphon-chat";
        if (n == "proving-pick") postLoginParams_["pick"] = "2";
        if (n == "codex-propose-error") postLoginParams_["demo"] = "1";
        if (n == "codex-propose-error") postLoginParams_["name"] = "Gryphon";
        if (n == "codex-table") postLoginParams_["view"] = "table";
        if (n == "codex-search") postLoginParams_["q"] = "lanternfold";
        if (n.rfind("codex-card", 0) == 0) postLoginParams_["agent"] = "gryphon";
        if (n == "codex-card-debug") postLoginParams_["scope"] = "DEBUGGING";
        if (n == "codex-card-notes" || n == "codex-card-compose") postLoginParams_["scroll"] = "end";
        if (n == "codex-card-compose") postLoginParams_["compose"] = "bane";
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
    ui::ShadowText("Your save could not be loaded.", ui::PaletteColor::TextRed);
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
      const auto result = ui::drawTabPanel(settings_, isKeeper_);
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
    case platform::ExportOutcome::Failed: ui::pushGameMessage("<col=red>Your portrait could not be saved.</col>"); break;
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
      SDL_Log("SMOKE OK: codex flow completed (%zu tombstones)", stones->size());
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
        auto created = svc.createAdventurer(uid, "Mossbeard");
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
      if ((*active)->name != "Mossbeard") return fail("wrong Adventurer after restart");
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
