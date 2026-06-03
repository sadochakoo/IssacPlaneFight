/*
 * haemolacria.h - 泪血症血球抛射、落地爆炸与硫磺火融合
 */

#ifndef HAEMOLACRIA_H
#define HAEMOLACRIA_H

#include "player_stats.h"
#include "split_laser.h"
#include <SFML/Graphics.hpp>
#include <vector>

class HaemolacriaSystem {
public:
    static constexpr float k_damage_multiplier       = 3.0f;
    /** 拾取后 stats.tear_rate 下限（帧），与开火硬编码冷却分开 */
    static constexpr int   k_tear_rate_after_pickup  = 60;
    /** 泪血症开火硬编码冷却（1.5s @ 60fps） */
    static constexpr int   k_fire_cooldown_frames    = 90;
    static constexpr float k_orb_land_offset_y       = 300.f;

    static bool overrides_attack(const Player& player);

    static bool try_fire(Player& player, std::vector<Bullet>& bullets);

    static void update_haemolacria_orbs(
        Player& player,
        std::vector<Bullet>& bullets,
        std::vector<SplitLaser>& split_lasers,
        float dt);

    static void render_orbs(sf::RenderWindow& window,
                            const std::vector<Bullet>& bullets);

    static void haemolacria_explode(
        const Player& player,
        const Bullet& ball,
        std::vector<Bullet>& pending_bullets,
        std::vector<SplitLaser>& pending_lasers);
};

#endif
