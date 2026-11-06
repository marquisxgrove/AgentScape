#pragma once

#include "services/profile_service.hpp"

namespace as::app {

// The service bundle passed to screens through FrameCtx. Owned by App.
struct Services {
  services::ProfileService profile;
};

}  // namespace as::app
