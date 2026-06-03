/*
 * attack_profile.h - 由 PlayerStats 层数推导出的攻击形态（融合结果）
 */

#ifndef ATTACK_PROFILE_H
#define ATTACK_PROFILE_H

#include "player_stats.h"
#include <SFML/Graphics.hpp>

struct AttackProfile {
    int   brimstone_level = 0;
    int   tracking_level  = 0;
    int   baby_count      = 0;
    int   parallel_lanes  = 1;
    float parallel_spacing = 22.f;

    float laser_half_width = 10.f;   // 总宽 = half * 2；双层硫磺火 → 20 half → 宽 40
    float homing_strength  = 380.f;
    sf::Color bullet_color = sf::Color(100, 200, 255);

    bool usesBrimstone() const { return brimstone_level > 0; }
    bool usesHoming() const { return tracking_level > 0; }
};

// 判定树：主武器形态 → 额外弹道 → 追踪附加
AttackProfile buildAttackProfile(const Player& player);

#endif
