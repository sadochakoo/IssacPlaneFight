/*
 * brimstone_laser.h - 硫磺火蓄力/激光（锚定玩家，支持弯勺曲线融合）
 */

#ifndef BRIMSTONE_LASER_H
#define BRIMSTONE_LASER_H

#include "player_stats.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <vector>

class BrimstoneLaser {
public:
    static constexpr int   kDefaultMaxCharge      = 60;
    static constexpr int   kDefaultLaserDuration = 30;
    static constexpr float kLaserHalfWidth       = 15.f;
    static constexpr float kPathStepY            = -20.f;

    static bool hasBrimstone(const Player& player);
    static bool hasHomingSynergy(const Player& player);
    static bool hasDoubleSynergy(const Player& player);

    // 长按蓄力：按住 +1 帧，松开满蓄触发激光
    static void updateChargeInput(Player& player, bool fire_pressed);

    // 每帧：递减激光计时、重建路径、矩形/路径碰撞伤害
    static void updateLaser(
        Player& player,
        std::vector<Enemy>& enemies,
        int damage,
        const std::function<void(const Enemy&)>& on_enemy_killed);

    // 蓄力圆环 + 光柱（直线 Rectangle 或 TriangleStrip 曲线）
    static void render(sf::RenderWindow& window, const Player& player);

    static void reset(Player& player);
};

#endif
