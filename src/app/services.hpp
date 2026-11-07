#pragma once

#include "services/activity_service.hpp"
#include "services/adventurer_service.hpp"
#include "services/profile_service.hpp"

namespace as::app {

// The service bundle passed to screens through FrameCtx. Owned by App.
struct Services {
  services::ProfileService profile;
  services::ActivityService activity;
  services::AdventurerService adventurers;
};

}  // namespace as::app
