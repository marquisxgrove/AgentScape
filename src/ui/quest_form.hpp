#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/ratings.hpp"

namespace as::ui {

enum class QuestStep { Where = 1, Rating = 2, Story = 3 };

// The quest scroll's working copy (quest-logs Requirement 1.1): three steps, pure and without ImGui, so the rules
// the screen relies on are unit-tested. Every edit goes through the same domain validator the service re-runs on
// Save, so the form can't build something the service would refuse for a form reason.
class QuestForm {
 public:
  // Starts a fresh form about one Agent (empty `agentId` = not chosen yet). Quest Type starts as Coding.
  void begin(const std::string& agentId);
  // Opens an existing log for editing. The Agent and version identify it and can no longer be changed.
  void edit(const std::string& questLogId, const domain::QuestLogInput& saved);

  bool editing() const { return !editingId_.empty(); }
  const std::string& editingId() const { return editingId_; }
  QuestStep step() const { return step_; }

  const domain::QuestLogInput& input() const { return input_; }
  domain::QuestLogInput& mutableInput() { return input_; }   // text fields are edited in place

  // --- Step 1 ---
  void setAgent(const std::string& agentId);
  void setQuestType(domain::QuestType type) { input_.questType = type; }
  void cycleQuestType();

  // --- Step 2 ---
  void setOverall(int stars);
  // 0 clears the Skill's rating (not every Skill has to be rated); 1..5 sets it.
  void setSkill(domain::Skill skill, int stars);
  int skillStars(domain::Skill skill) const;   // 0 = not rated

  // --- Step 3 ---
  // Adds or removes a tag. Adding a fourth to one side is refused (returns false and changes nothing).
  bool toggleTag(bool pro, const std::string& tag);
  bool hasTag(bool pro, const std::string& tag) const;
  void setRecommend(bool yes) { input_.recommend = yes; }

  // --- Moving between steps ---
  // Problems with the fields of one step, from the same validator the service uses.
  std::vector<domain::FieldError> errorsFor(QuestStep step) const;
  // Which step a field belongs to (so a refused Save can jump to the first step with a problem).
  static QuestStep stepOfField(const std::string& field);

  // Moves forward only when the current step has no problems. Returns whether it moved.
  bool next();
  void back();
  void goTo(QuestStep step) { step_ = step; }

  // Everything is fine: ready to submit.
  bool ready() const;
  // The first step that has a problem, or nullopt.
  std::optional<QuestStep> firstBadStep() const;

 private:
  domain::QuestLogInput input_;
  std::string editingId_;
  QuestStep step_ = QuestStep::Where;
};

}  // namespace as::ui
