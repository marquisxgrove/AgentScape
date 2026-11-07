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
#include "screens/login/login_screen.hpp"
#include "screens/world/world_screen.hpp"
#include "ui/layout.hpp"
#include "ui/tab_panel.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"
#ifdef AS_ENABLE_DEV_SCREENS
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
    const char* v = std::getenv(std::string(name).c_str());
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

  updateData();  // desktop opens the store immediately; the web waits for IndexedDB
  return true;
}

App::~App() {
  chatbox_.reset();  // unsubscribes from the bus before services/db go away
  services_.reset();
  store_.reset();  // close the database before tearing down the platform layer
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
  if (store_->seeded) SDL_Log("Seeded community snapshot: %d NPC adventurers", store_->seeded->npcsUpserted);
  SDL_Log("Local store ready: %s (schema %d)", savePath_.c_str(), store_->schemaVersion);
  persistence_.requestFlush();  // migrations/seed on a new web save
  services_.emplace(Services{services::ProfileService{store_->db, clock_, rng_},
                             services::ActivityService{store_->db, clock_, bus_}});
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
  SDL_Log("Signed in remembered user %s", user->name.c_str());
}

void App::adoptUser(const std::string& userId, const domain::Settings& settings) {
  currentUserId_ = userId;
  applySettings(settings);
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
#ifdef AS_ENABLE_DEV_SCREENS
  screens_.registerScreen(ScreenId::DevGallery, [] { return std::make_unique<screens::UiGalleryScreen>(); });
#endif
  ScreenId start = ScreenId::Login;
  if (config_.startScreen) {
#ifdef AS_ENABLE_DEV_SCREENS
    if (*config_.startScreen == "ui-gallery") start = ScreenId::DevGallery;
#endif
    if (start == ScreenId::Login) SDL_Log("Unknown or unavailable --screen=%s", config_.startScreen->c_str());
  }
  screens_.navigate(start);
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
      const auto result = ui::drawTabPanel(settings_);
      if (result.settingsChanged) {
        applySettings(result.settings);
        if (services_) {
          if (auto saved = services_->profile.updateSettings(*currentUserId_, result.settings); !saved) {
            logError("Save settings", saved.error());
          }
        }
      }
      if (result.logOut) {
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
    } else {
      // Other default actions map to a screen navigation (e.g. "enter" -> target screen).
      SDL_Log("Context action: %s -> %s", chosen->id.c_str(), chosen->target.c_str());
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
  frame_->present();

  ++framesRendered_;
  if (quitRequested_) return SDL_APP_SUCCESS;

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
