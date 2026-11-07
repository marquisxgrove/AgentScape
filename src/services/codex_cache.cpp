#include "services/codex_cache.hpp"

namespace as::services {

CodexCache::CodexCache(app::EventBus& bus) {
  ratings_ = app::Subscription{bus, bus.subscribe<app::RatingsChanged>([this](const app::RatingsChanged&) { invalidate(); })};
  agents_ = app::Subscription{bus, bus.subscribe<app::AgentChanged>([this](const app::AgentChanged&) { invalidate(); })};
  notes_ = app::Subscription{bus, bus.subscribe<app::NotesChanged>([this](const app::NotesChanged&) { invalidate(); })};
}

}  // namespace as::services
