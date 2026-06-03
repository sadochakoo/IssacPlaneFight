/*
 * bullet_factory.h - 根据 AttackProfile 生成普通弹幕
 */

#ifndef BULLET_FACTORY_H
#define BULLET_FACTORY_H

#include "attack_profile.h"
#include "player_stats.h"
#include "split_laser.h"
#include <vector>

class BulletFactory {
public:
    static void tryFire(
        Player& player,
        const AttackProfile& profile,
        std::vector<Bullet>& bullets,
        bool fire_pressed,
        float dt);

    static void updateBullets(
        Player& player,
        std::vector<Bullet>& bullets,
        std::vector<SplitLaser>& split_lasers,
        const std::vector<Enemy>& enemies,
        float player_shot_speed,
        float dt);
};

#endif
