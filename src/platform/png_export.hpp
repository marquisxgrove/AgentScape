#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <string>

namespace as::platform {

// Saves a transparent PNG (character-creator Requirement 8). Desktop: a native save dialog. Web: a
// browser download. Results arrive asynchronously, so call poll() once per frame.
enum class ExportOutcome {
  None,       // nothing finished this frame
  Saved,      // the file was written (or the download started)
  Cancelled,  // the user dismissed the dialog: do nothing
  Failed,     // writing failed: print "Your portrait could not be saved."
};

class PngExporter {
 public:
  struct State;  // shared with the async dialog callback
  PngExporter();
  ~PngExporter();
  PngExporter(const PngExporter&) = delete;
  PngExporter& operator=(const PngExporter&) = delete;

  // Takes a copy of the surface, so the caller may free its own immediately. `suggestedName` is the
  // default file name, e.g. "Mossbeard.png". Ignored if an export is already in progress.
  void start(SDL_Window* window, SDL_Surface* surface, const std::string& suggestedName);
  bool busy() const;
  ExportOutcome poll();

 private:
  std::shared_ptr<State> state_;
};

// Writes the surface to `path` as PNG. Returns false on failure. (No dialog; used by tests and dev flags.)
bool writePng(SDL_Surface* surface, const std::string& path);

// Strips characters that are unsafe in file names ("Kiro Fan" -> "Kiro Fan.png", never a path separator).
std::string safePngName(const std::string& adventurerName);

}  // namespace as::platform
