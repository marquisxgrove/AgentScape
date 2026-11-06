#include "ui/context_menu_model.hpp"

namespace as::ui {

void ContextMenuModel::open(std::vector<domain::MenuAction> items, float x, float y) {
  items_ = std::move(items);
  selected_ = 0;
  x_ = x;
  y_ = y;
  open_ = !items_.empty();
}

void ContextMenuModel::close() {
  open_ = false;
  items_.clear();
  selected_ = 0;
}

bool ContextMenuModel::isCancel(std::size_t index) const {
  return index < items_.size() && items_[index].id == "cancel";
}

void ContextMenuModel::moveUp() {
  if (!open_ || items_.empty()) return;
  selected_ = selected_ == 0 ? items_.size() - 1 : selected_ - 1;
}

void ContextMenuModel::moveDown() {
  if (!open_ || items_.empty()) return;
  selected_ = selected_ + 1 >= items_.size() ? 0 : selected_ + 1;
}

void ContextMenuModel::moveTo(std::size_t index) {
  if (open_ && index < items_.size()) selected_ = index;
}

std::optional<domain::MenuAction> ContextMenuModel::confirm() {
  if (!open_) return std::nullopt;
  return confirmIndex(selected_);
}

std::optional<domain::MenuAction> ContextMenuModel::confirmIndex(std::size_t index) {
  if (!open_ || index >= items_.size()) return std::nullopt;
  const auto action = items_[index];
  const bool cancel = isCancel(index);
  close();
  if (cancel) return std::nullopt;
  return action;
}

}  // namespace as::ui
