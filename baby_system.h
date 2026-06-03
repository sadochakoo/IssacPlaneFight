/*
 * baby_system.h - 宝宝/分身：复制玩家当前攻击形态
 */

#ifndef BABY_SYSTEM_H
#define BABY_SYSTEM_H

#include "attack_profile.h"
#include "player_stats.h"
#include <SFML/Graphics.hpp>
#include <vector>

class BabySystem {
public:
    static void syncCount(Player& player);
    static void updateOrbit(Player& player, float dt);

    static void tryFireBullets(
        Player& player,
        const AttackProfile& profile,
        std::vector<Bullet>& bullets);

    static void render(sf::RenderWindow& window, const Player& player);
    static void reset(Player& player);
};

#endif
