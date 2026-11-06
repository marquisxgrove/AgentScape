#include "platform/persist.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "platform/paths.hpp"

namespace as::platform {

#ifdef __EMSCRIPTEN__

// JS-side state on Module: asPersist = 0 loading, 1 ready, 2 unavailable.
// asSyncing/asSyncAgain serialise syncfs calls (overlapping IDBFS syncs are not allowed).
Persistence::Persistence() {
  state_ = State::Loading;
  // clang-format off
  EM_ASM({
    Module.asPersist = 0;
    Module.asSyncing = false;
    Module.asSyncAgain = false;
    Module.asFlush = function() {
      if (Module.asPersist !== 1) return;
      if (Module.asSyncing) { Module.asSyncAgain = true; return; }
      Module.asSyncing = true;
      FS.syncfs(false, function(err) {
        Module.asSyncing = false;
        if (err) console.warn('AgentScape: saving to IndexedDB failed', err);
        if (Module.asSyncAgain) { Module.asSyncAgain = false; Module.asFlush(); }
      });
    };
    try {
      if (typeof indexedDB === 'undefined' || indexedDB === null) throw new Error('no IndexedDB');
      FS.mkdir(UTF8ToString($0));
      FS.mount(IDBFS, {}, UTF8ToString($0));
      FS.syncfs(true, function(err) {
        if (err) { console.warn('AgentScape: loading from IndexedDB failed', err); Module.asPersist = 2; return; }
        Module.asPersist = 1;
        window.addEventListener('pagehide', function() { Module.asFlush(); });
        document.addEventListener('visibilitychange', function() {
          if (document.visibilityState === 'hidden') Module.asFlush();
        });
      });
    } catch (e) {
      console.warn('AgentScape: IndexedDB unavailable', e);
      Module.asPersist = 2;
    }
  }, WEB_PERSIST_DIR);
  // clang-format on
}

Persistence::State Persistence::state() {
  if (state_ == State::Loading) {
    const int s = EM_ASM_INT({ return Module.asPersist | 0; });
    if (s == 1) state_ = State::Ready;
    if (s == 2) state_ = State::Unavailable;
  }
  return state_;
}

void Persistence::requestFlush() {
  if (state_ == State::Ready) scheduler_.request(FlushScheduler::Clock::now());
}

void Persistence::tick() {
  if (scheduler_.poll(FlushScheduler::Clock::now())) {
    EM_ASM({ Module.asFlush(); });
  }
}

#else  // desktop

Persistence::Persistence() = default;
Persistence::State Persistence::state() { return state_; }
void Persistence::requestFlush() {}
void Persistence::tick() {}

#endif

}  // namespace as::platform
