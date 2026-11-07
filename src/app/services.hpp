#pragma once

#include "services/activity_service.hpp"
#include "services/agent_service.hpp"
#include "services/adventurer_service.hpp"
#include "services/codex_service.hpp"
#include "services/hiscores_service.hpp"
#include "services/keeper_service.hpp"
#include "services/note_service.hpp"
#include "services/proving_service.hpp"
#include "services/quest_service.hpp"
#include "services/profile_service.hpp"

namespace as::app {

// The service bundle passed to screens through FrameCtx. Owned by App.
struct Services {
  services::ProfileService profile;
  services::ActivityService activity;
  services::AdventurerService adventurers;
  services::CodexService codex;
  services::NoteService notes;
  services::AgentService agents;
  services::KeeperService keeper;
  services::ProvingService proving;
  services::QuestService quests;
  services::HiscoresService hiscores;
};

}  // namespace as::app
