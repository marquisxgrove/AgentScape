#pragma once

#include <string>
#include <tl/expected.hpp>

namespace as {

enum class GameErrorCode {
  InvalidInput,
  NotFound,
  RateLimited,
  StorageFailed,
  Forbidden,
  Internal,
};

struct GameError {
  GameErrorCode code = GameErrorCode::Internal;
  std::string message;  // game-voice text, safe to show the player
};

template <typename T, typename E = GameError>
using Result = tl::expected<T, E>;

inline tl::unexpected<GameError> makeError(GameErrorCode code, std::string message) {
  return tl::unexpected<GameError>(GameError{code, std::move(message)});
}

}  // namespace as
