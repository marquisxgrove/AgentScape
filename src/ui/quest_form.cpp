#include "ui/quest_form.hpp"

#include <algorithm>

namespace as::ui {

namespace {
std::vector<std::string>& side(domain::QuestLogInput& in, bool pro) { return pro ? in.pros : in.cons; }
const std::vector<std::string>& side(const domain::QuestLogInput& in, bool pro) { return pro ? in.pros : in.cons; }
}  // namespace

void QuestForm::begin(const std::string& agentId) {
  *this = QuestForm{};
  input_.agentId = agentId;
  input_.questType = domain::QuestType::Coding;
  input_.overall = 0;   // not chosen yet: the form insists on an explicit rating
}

void QuestForm::edit(const std::string& questLogId, const domain::QuestLogInput& saved) {
  *this = QuestForm{};
  input_ = saved;
  editingId_ = questLogId;
  step_ = QuestStep::Rating;   // the Agent and version are already settled
}

void QuestForm::setAgent(const std::string& agentId) {
  if (!editing()) input_.agentId = agentId;
}

void QuestForm::cycleQuestType() {
  const auto index = static_cast<std::size_t>(input_.questType);
  input_.questType = domain::QUEST_TYPES[(index + 1) % domain::QUEST_TYPES.size()];
}

void QuestForm::setOverall(int stars) { input_.overall = std::clamp(stars, 0, 5); }

void QuestForm::setSkill(domain::Skill skill, int stars) {
  if (stars <= 0) {
    input_.skills.erase(skill);
  } else {
    input_.skills[skill] = std::min(stars, 5);
  }
}

int QuestForm::skillStars(domain::Skill skill) const {
  const auto it = input_.skills.find(skill);
  return it == input_.skills.end() ? 0 : it->second;
}

bool QuestForm::hasTag(bool pro, const std::string& tag) const {
  const auto& tags = side(input_, pro);
  return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

bool QuestForm::toggleTag(bool pro, const std::string& tag) {
  auto& tags = side(input_, pro);
  const auto it = std::find(tags.begin(), tags.end(), tag);
  if (it != tags.end()) {
    tags.erase(it);
    return true;
  }
  if (tags.size() >= domain::MAX_TAGS_PER_SIDE) return false;
  tags.push_back(tag);
  return true;
}

QuestStep QuestForm::stepOfField(const std::string& field) {
  if (field == "overall" || field == "skills") return QuestStep::Rating;
  if (field == "title" || field == "body" || field == "pros" || field == "cons") return QuestStep::Story;
  return QuestStep::Where;   // agent, version
}

std::vector<domain::FieldError> QuestForm::errorsFor(QuestStep forStep) const {
  std::vector<domain::FieldError> out;
  const auto result = domain::validateQuestLogInput(input_);
  if (result) return out;
  for (const auto& e : result.error()) {
    if (stepOfField(e.field) == forStep) out.push_back(e);
  }
  return out;
}

bool QuestForm::next() {
  if (step_ == QuestStep::Story || !errorsFor(step_).empty()) return false;
  step_ = static_cast<QuestStep>(static_cast<int>(step_) + 1);
  return true;
}

void QuestForm::back() {
  // An existing log starts at the rating step; the "where" step is not part of editing it.
  const int floor = editing() ? static_cast<int>(QuestStep::Rating) : static_cast<int>(QuestStep::Where);
  if (static_cast<int>(step_) > floor) step_ = static_cast<QuestStep>(static_cast<int>(step_) - 1);
}

bool QuestForm::ready() const { return domain::validateQuestLogInput(input_).has_value(); }

std::optional<QuestStep> QuestForm::firstBadStep() const {
  for (const auto s : {QuestStep::Where, QuestStep::Rating, QuestStep::Story}) {
    if (!errorsFor(s).empty()) return s;
  }
  return std::nullopt;
}

}  // namespace as::ui
