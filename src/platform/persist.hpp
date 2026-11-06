#pragma once

#include "platform/flush_scheduler.hpp"

namespace as::platform {

// Keeps the database file durable across page reloads in the web build.
//  - Web: mounts IDBFS at /persist and loads it (FS.syncfs(true)) before the database opens;
//    requestFlush() schedules a debounced FS.syncfs(false); the page's `pagehide` also flushes.
//  - Desktop: files are already durable, so everything is a no-op and state() is Ready at once.
class Persistence {
 public:
  enum class State {
    Loading,      // web: waiting for IndexedDB to load into /persist
    Ready,        // the data dir is usable and durable
    Unavailable,  // web: IndexedDB missing or failed (private mode?) -> run in memory
  };

  Persistence();
  State state();  // polls the async load on the web

  // Call after every committed write transaction.
  void requestFlush();
  // Call once per frame; performs a flush when one is due.
  void tick();

 private:
  FlushScheduler scheduler_;
  State state_ = State::Ready;
};

}  // namespace as::platform
