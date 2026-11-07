#include "platform/png_export.hpp"

#include <SDL3_image/SDL_image.h>

#include <atomic>
#include <mutex>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace as::platform {

struct PngExporter::State {
  std::mutex mutex;
  SDL_Surface* surface = nullptr;      // owned copy; freed when the export finishes
  std::atomic<bool> busy{false};
  ExportOutcome outcome = ExportOutcome::None;  // guarded by mutex

  ~State() {
    if (surface != nullptr) SDL_DestroySurface(surface);
  }
  void finish(ExportOutcome result) {
    std::lock_guard<std::mutex> lock(mutex);
    if (surface != nullptr) {
      SDL_DestroySurface(surface);
      surface = nullptr;
    }
    outcome = result;
    busy = false;
  }
};

bool writePng(SDL_Surface* surface, const std::string& path) {
  if (surface == nullptr) return false;
  if (!IMG_SavePNG(surface, path.c_str())) {
    SDL_Log("PNG export failed (%s): %s", path.c_str(), SDL_GetError());
    return false;
  }
  return true;
}

std::string safePngName(const std::string& name) {
  std::string out;
  for (const char c : name) {
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
      continue;
    }
    out += c;
  }
  if (out.empty()) out = "adventurer";
  return out + ".png";
}

PngExporter::PngExporter() : state_(std::make_shared<State>()) {}
PngExporter::~PngExporter() = default;

bool PngExporter::busy() const { return state_->busy.load(); }

ExportOutcome PngExporter::poll() {
  std::lock_guard<std::mutex> lock(state_->mutex);
  const auto result = state_->outcome;
  state_->outcome = ExportOutcome::None;
  return result;
}

#ifdef __EMSCRIPTEN__

void PngExporter::start(SDL_Window* /*window*/, SDL_Surface* surface, const std::string& suggestedName) {
  if (state_->busy.exchange(true)) return;
  // Write to MEMFS, then hand the bytes to the browser as a download.
  const std::string path = "/tmp/" + suggestedName;
  if (!writePng(surface, path)) {
    state_->finish(ExportOutcome::Failed);
    return;
  }
  // clang-format off
  EM_ASM({
    var name = UTF8ToString($0);
    var data = FS.readFile('/tmp/' + name);
    var blob = new Blob([data], {type: 'image/png'});
    var a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = name;
    document.body.appendChild(a);
    a.click();
    setTimeout(function() { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
    FS.unlink('/tmp/' + name);
  }, suggestedName.c_str());
  // clang-format on
  state_->finish(ExportOutcome::Saved);
}

#else

namespace {
struct DialogRequest {
  std::shared_ptr<PngExporter::State> state;
};

void SDLCALL onDialogDone(void* userdata, const char* const* fileList, int /*filter*/) {
  std::unique_ptr<DialogRequest> req{static_cast<DialogRequest*>(userdata)};
  auto& state = *req->state;
  if (fileList == nullptr) {  // the dialog could not be shown (no display, no portal...)
    SDL_Log("Save dialog failed: %s", SDL_GetError());
    state.finish(ExportOutcome::Failed);
    return;
  }
  if (fileList[0] == nullptr) {  // cancelled
    state.finish(ExportOutcome::Cancelled);
    return;
  }
  SDL_Surface* surface = nullptr;
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    surface = state.surface;
  }
  state.finish(writePng(surface, fileList[0]) ? ExportOutcome::Saved : ExportOutcome::Failed);
}
}  // namespace

void PngExporter::start(SDL_Window* window, SDL_Surface* surface, const std::string& suggestedName) {
  if (surface == nullptr || state_->busy.exchange(true)) return;
  {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->surface = SDL_DuplicateSurface(surface);
    state_->outcome = ExportOutcome::None;
  }
  if (state_->surface == nullptr) {
    state_->finish(ExportOutcome::Failed);
    return;
  }
  static const SDL_DialogFileFilter filters[] = {{"PNG image", "png"}};
  auto* req = new DialogRequest{state_};  // freed by onDialogDone
  SDL_ShowSaveFileDialog(onDialogDone, req, window, filters, 1, suggestedName.c_str());
}

#endif

}  // namespace as::platform
