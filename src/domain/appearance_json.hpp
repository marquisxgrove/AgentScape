#pragma once

#include <nlohmann/json.hpp>

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

}  // namespace as::domain
