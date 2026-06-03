#include "weapon_synergy_resolver.h"
#include "passive_item.h"
#include <algorithm>

bool WeaponSynergyResolver::has(const std::vector<std::string>& items, const char* id) {
    return std::find(items.begin(), items.end(), id) != items.end();
}

static WeaponProfile makeBrimstoneHomingLaser() {
    WeaponProfile p;
    p.mode = WeaponMode::BrimstoneLaserHoming;
    p.use_laser = false;
    p.laser_homing = true;
    return p;
}

static WeaponProfile makeBrimstoneDoubleLaser() {
    WeaponProfile p;
    p.mode = WeaponMode::BrimstoneLaserDouble;
    p.use_laser = false;
    p.parallel_shots = 2;
    p.parallel_spacing = 28.f;
    return p;
}

static WeaponProfile makeBrimstoneLaser() {
    WeaponProfile p;
    p.mode = WeaponMode::BrimstoneLaser;
    p.use_laser = false;
    return p;
}

static WeaponProfile makeDoubleHomingBullets() {
    WeaponProfile p;
    p.mode = WeaponMode::HomingUp;
    p.parallel_shots = 2;
    p.bullet_homing = true;
    p.homing_strength = 420.f;
    p.bullet_color = sf::Color(220, 100, 255);
    return p;
}

static WeaponProfile makeDoubleBullets() {
    WeaponProfile p;
    p.mode = WeaponMode::DoubleUp;
    p.parallel_shots = 2;
    return p;
}

static WeaponProfile makeHomingBullet() {
    WeaponProfile p;
    p.mode = WeaponMode::HomingUp;
    p.bullet_homing = true;
    p.homing_strength = 380.f;
    p.bullet_color = sf::Color(200, 80, 255);
    return p;
}

std::vector<WeaponSynergyResolver::SynergyRule> WeaponSynergyResolver::buildRuleTable() {
    return {
        { [](const std::vector<std::string>& c) {
              return has(c, ItemIds::BRIMSTONE) && has(c, ItemIds::SPOON_BENDER);
          }, makeBrimstoneHomingLaser() },
        { [](const std::vector<std::string>& c) {
              return has(c, ItemIds::BRIMSTONE) && has(c, ItemIds::TWENTY_TWENTY);
          }, makeBrimstoneDoubleLaser() },
        { [](const std::vector<std::string>& c) { return has(c, ItemIds::BRIMSTONE); },
          makeBrimstoneLaser() },
        { [](const std::vector<std::string>& c) {
              return has(c, ItemIds::TWENTY_TWENTY) && has(c, ItemIds::SPOON_BENDER);
          }, makeDoubleHomingBullets() },
        { [](const std::vector<std::string>& c) { return has(c, ItemIds::TWENTY_TWENTY); },
          makeDoubleBullets() },
        { [](const std::vector<std::string>& c) { return has(c, ItemIds::SPOON_BENDER); },
          makeHomingBullet() },
    };
}

WeaponProfile WeaponSynergyResolver::resolve(const std::vector<std::string>& collected) const {
    for (const auto& rule : buildRuleTable()) {
        if (rule.match(collected)) {
            return rule.profile;
        }
    }
    WeaponProfile normal;
    normal.mode = WeaponMode::NormalUp;
    return normal;
}
