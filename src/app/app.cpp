#include "app/app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <chrono>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "platform/paths.hpp"
#include "platform/web_url.hpp"

namespace as::app {

namespace {
std::chrono::sys_seconds utcNow() {
  return std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
}

void logError(const char* what, const GameError& e) {
  SDL_Log("%s: %s%s%s", what, e.message.c_str(), e.detail.empty() ? "" : " | ", e.detail.c_str());
}

constexpr ImVec4 YELLOW{1.0F, 0.85F, 0.2F, 1.0F};
constexpr ImVec4 RED{0.95F, 0.25F, 0.2F, 1.0F};

bool beginFullFrameWindow(const char* id) {
  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize({static_cast<float>(domain::FRAME_W), static_cast<float>(domain::FRAME_H)});
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
  return ImGui::Begin(id, nullptr, kFlags);
}
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

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.AntiAliasedLines = false;
  style.AntiAliasedLinesUseTex = false;
  style.AntiAliasedFill = false;
  style.WindowRounding = style.FrameRounding = style.ChildRounding = style.PopupRounding = 0.0F;
  style.ScrollbarRounding = style.GrabRounding = style.TabRounding = 0.0F;
  ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
  ImGui_ImplSDLRenderer3_Init(renderer_);

  kiroween_ = domain::isKiroweenActive(config_.kiroween, utcNow());
  updateData();  // desktop opens the store immediately; the web waits for IndexedDB
  return true;
}

App::~App() {
  store_.reset();  // close the database before tearing down the platform layer
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
  dataState_ = DataState::Ready;
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

void App::drawLoading() {
  if (beginFullFrameWindow("##loading")) {
    ImGui::SetCursorPos({330, 240});
    ImGui::TextColored(YELLOW, "Loading...");
  }
  ImGui::End();
}

void App::drawLoadFailed() {
  if (beginFullFrameWindow("##loadfailed")) {
    ImGui::SetCursorPos({250, 200});
    ImGui::TextColored(RED, "Your save could not be loaded.");
    ImGui::SetCursorPosX(250);
    if (loadError_ && loadError_->canStartFresh) {
      if (ImGui::Button("Start fresh")) {
        dataState_ = DataState::WaitingForStorage;
        openStore(/*resetData=*/true);
      }
      ImGui::SameLine();
    }
    if (ImGui::Button("Quit")) quitRequested_ = true;
  }
  ImGui::End();
}

void App::drawShell() {
  if (beginFullFrameWindow("##frame")) {
    ImGui::TextColored(YELLOW, "Welcome to AgentScape");
    ImGui::TextUnformatted("Rate your familiars, brave adventurer.");
    ImGui::Separator();
    ImGui::Text("Frame %dx%d at %dx scale", domain::FRAME_W, domain::FRAME_H, frame_->layout().scale);
    ImGui::TextUnformatted(kiroween_ ? "Kiroween mode: ON" : "Kiroween mode: off");
    ImGui::TextUnformatted("Save:");
    ImGui::SameLine();
    ImGui::TextUnformatted(savePath_.c_str());  // user-controlled path: never a format string
    if (inMemoryOnly_) {
      ImGui::TextColored(RED, "Your progress can't be saved in this browser (private mode?).");
    }
  }
  ImGui::End();
}

SDL_AppResult App::iterate() {
  updateData();
  persistence_.tick();
  frame_->updateLayout(window_);

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::GetIO().DisplaySize = {static_cast<float>(domain::FRAME_W), static_cast<float>(domain::FRAME_H)};
  ImGui::GetIO().DisplayFramebufferScale = {1.0F, 1.0F};
  ImGui::NewFrame();
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
  ImGui::Render();

  frame_->beginFrame();
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
  frame_->present();

  ++framesRendered_;
  if (quitRequested_) return SDL_APP_SUCCESS;
  if (config_.headlessSmoke && framesRendered_ >= 5) {
    // Smoke runs must fail loudly if storage didn't come up.
    return dataState_ == DataState::Ready ? SDL_APP_SUCCESS : SDL_APP_FAILURE;
  }
  return SDL_APP_CONTINUE;
}

}  // namespace as::app
