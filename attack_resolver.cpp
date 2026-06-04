#include "attack_profile.h"
#include <algorithm>

AttackProfile buildAttackProfile(const Player& player) {
    AttackProfile profile;
    const PlayerStats& s = player.stats;

    // ── 1. 主武器形态：硫磺火 vs 普通弹幕 ──
    profile.brimstone_level = std::max(0, s.brimstone_level);
    if (profile.usesBrimstone()) {
        profile.laser_half_width =
            (profile.brimstone_level >= 2) ? 20.f : 10.f;
    }

    // ── 2. 额外弹道（20/20 等）：对激光与普通弹均生效 ──
    profile.parallel_lanes = 1 + std::max(0, s.extra_bullets);
    profile.parallel_spacing =
        profile.usesBrimstone() ? 28.f : 22.f;

    // ── 3. 追踪叠加（弯勺）：对激光与普通弹均生效 ──
    profile.tracking_level = std::max(0, s.tracking_level);
    if (player.stats_ext.has_godhead) {
        profile.tracking_level = std::max(1, profile.tracking_level);
        profile.homing_strength = 520.f;
        profile.bullet_color    = sf::Color(255, 220, 80);
    } else if (profile.usesHoming()) {
        profile.homing_strength =
            360.f + 40.f * static_cast<float>(profile.tracking_level - 1);
        profile.bullet_color = sf::Color(200, 80, 255);
    }

    profile.baby_count = std::max(0, s.baby_count);
    return profile;
}
