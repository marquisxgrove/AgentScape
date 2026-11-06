#include "app/app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>

namespace as::app {

Config parseConfig(int argc, char** argv) {
  Config cfg;
  std::optional<std::string_view> cli;
  std::string cliStorage;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    constexpr std::string_view kKiroween = "--kiroween=";
    if (arg.substr(0, kKiroween.size()) == kKiroween) {
      cliStorage = std::string(arg.substr(kKiroween.size()));
      cli = cliStorage;
    } else if (arg == "--reseed") {
      cfg.reseed = true;
    } else if (arg == "--reset-data") {
      cfg.resetData = true;
    } else if (arg == "--smoke") {
      cfg.headlessSmoke = true;
    }
  }
  const char* envRaw = std::getenv("KIROWEEN_MODE");
  std::optional<std::string_view> env = envRaw != nullptr ? std::optional<std::string_view>{envRaw} : std::nullopt;
  const std::array<std::optional<std::string_view>, 3> sources{cli, env, std::string_view{AS_KIROWEEN_DEFAULT}};
  cfg.kiroween = domain::resolveKiroweenSetting(sources);
  return cfg;
}

std::unique_ptr<App> App::create(const Config& config) {
  std::unique_ptr<App> app{new App(config)};
  if (!app->init()) {
    return nullptr;
  }
  return app;
}

bool App::init() {
  constexpr int kStartScale = 2;
  const auto flags = SDL_WINDOW_RESIZABLE;
  if (!SDL_CreateWindowAndRenderer("AgentScape", domain::FRAME_W * kStartScale, domain::FRAME_H * kStartScale, flags,
                                   &window_, &renderer_)) {
    SDL_Log("Could not create window/renderer: %s", SDL_GetError());
    return false;
  }
  SDL_SetWindowMinimumSize(window_, domain::FRAME_W, domain::FRAME_H);

  frame_ = std::make_unique<render::GameFrame>(renderer_);
  if (!frame_->valid()) {
    return false;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  ImGuiStyle& style = ImGui::GetStyle();
  style.AntiAliasedLines = false;
  style.AntiAliasedLinesUseTex = false;
  style.AntiAliasedFill = false;
  style.WindowRounding = style.FrameRounding = style.ChildRounding = style.PopupRounding = 0.0F;
  style.ScrollbarRounding = style.GrabRounding = style.TabRounding = 0.0F;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
  ImGui_ImplSDLRenderer3_Init(renderer_);

  kiroween_ = domain::isKiroweenActive(config_.kiroween, std::chrono::floor<std::chrono::seconds>(
                                                              std::chrono::system_clock::now()));
  return true;
}

App::~App() {
  if (ImGui::GetCurrentContext() != nullptr) {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
  frame_.reset();
  if (renderer_ != nullptr) SDL_DestroyRenderer(renderer_);
  if (window_ != nullptr) SDL_DestroyWindow(window_);
}

SDL_AppResult App::handleEvent(SDL_Event& event) {
  if (event.type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }
  // Pointer events are converted to frame pixels before ImGui sees them (design: "Handles queued SDL events").
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

void App::drawShell() {
  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize({static_cast<float>(domain::FRAME_W), static_cast<float>(domain::FRAME_H)});
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
  if (ImGui::Begin("##frame", nullptr, kFlags)) {
    ImGui::TextColored({1.0F, 0.85F, 0.2F, 1.0F}, "Welcome to AgentScape");
    ImGui::TextUnformatted("Rate your familiars, brave adventurer.");
    ImGui::Separator();
    ImGui::Text("Frame %dx%d at %dx scale", domain::FRAME_W, domain::FRAME_H, frame_->layout().scale);
    ImGui::TextUnformatted(kiroween_ ? "Kiroween mode: ON" : "Kiroween mode: off");
  }
  ImGui::End();
}

SDL_AppResult App::iterate() {
  frame_->updateLayout(window_);

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::GetIO().DisplaySize = {static_cast<float>(domain::FRAME_W), static_cast<float>(domain::FRAME_H)};
  ImGui::GetIO().DisplayFramebufferScale = {1.0F, 1.0F};
  ImGui::NewFrame();
  drawShell();
  ImGui::Render();

  frame_->beginFrame();
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
  frame_->present();

  ++framesRendered_;
  if (config_.headlessSmoke && framesRendered_ >= 5) {
    return SDL_APP_SUCCESS;
  }
  return SDL_APP_CONTINUE;
}

}  // namespace as::app
