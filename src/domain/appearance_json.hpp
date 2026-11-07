#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string_view>

#include "domain/appearance.hpp"

namespace as::domain {

inline void to_json(nlohmann::json& j, const Colors& c) {
  j = nlohmann::json{{"hair", c.hair}, {"torso", c.torso}, {"legs", c.legs}, {"feet", c.feet}, {"skin", c.skin}};
}

inline void from_json(const nlohmann::json& j, Colors& c) {
  j.at("hair").get_to(c.hair);
  j.at("torso").get_to(c.torso);
  j.at("legs").get_to(c.legs);
  j.at("feet").get_to(c.feet);
  j.at("skin").get_to(c.skin);
}

inline void to_json(nlohmann::json& j, const Appearance& a) {
  j = nlohmann::json{{"bodyType", a.bodyType == BodyType::A ? "A" : "B"},
                     {"hair", a.hair},
                     {"facialHair", a.facialHair},
                     {"torso", a.torso},
                     {"arms", a.arms},
                     {"legs", a.legs},
                     {"colors", a.colors}};
}

inline void from_json(const nlohmann::json& j, Appearance& a) {
  a.bodyType = j.at("bodyType").get<std::string>() == "B" ? BodyType::B : BodyType::A;
  j.at("hair").get_to(a.hair);
  j.at("facialHair").get_to(a.facialHair);
  j.at("torso").get_to(a.torso);
  j.at("arms").get_to(a.arms);
  j.at("legs").get_to(a.legs);
  j.at("colors").get_to(a.colors);
}

// Non-throwing parse for stored data: nullopt if the text isn't valid JSON, a field is missing or
// has the wrong type. Does NOT range-check; callers also run validateAppearance.
inline std::optional<Appearance> parseAppearance(std::string_view text) {
  const auto j = nlohmann::json::parse(text.begin(), text.end(), nullptr, /*allow_exceptions=*/false);
  if (!j.is_object()) return std::nullopt;
  const auto intField = [&](const nlohmann::json& obj, const char* key, int& out) {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_number_integer()) return false;
    out = it->get<int>();
    return true;
  };
  Appearance a;
  const auto body = j.find("bodyType");
  if (body == j.end() || !body->is_string()) return std::nullopt;
  a.bodyType = body->get<std::string>() == "B" ? BodyType::B : BodyType::A;
  const auto colors = j.find("colors");
  if (colors == j.end() || !colors->is_object()) return std::nullopt;
  const bool ok = intField(j, "hair", a.hair) && intField(j, "facialHair", a.facialHair) &&
                  intField(j, "torso", a.torso) && intField(j, "arms", a.arms) && intField(j, "legs", a.legs) &&
                  intField(*colors, "hair", a.colors.hair) && intField(*colors, "torso", a.colors.torso) &&
                  intField(*colors, "legs", a.colors.legs) && intField(*colors, "feet", a.colors.feet) &&
                  intField(*colors, "skin", a.colors.skin);
  if (!ok) return std::nullopt;
  return a;
}

}  // namespace as::domain
