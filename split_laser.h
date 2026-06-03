/*
 * split_laser.h - 硫磺火 + 寄生虫：短促折射分裂激光
 */

#ifndef SPLIT_LASER_H
#define SPLIT_LASER_H

#include "player_stats.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <vector>

struct SplitLaser {
    int   laser_id = 0;
    float x = 0.f;
    float y = 0.f;
    float angle = 0.f;
    float max_length = 130.f;
    float damage = 0.f;
    int   generation = 0;
    int   life_timer = 0;
    bool  is_baby_tear = false;
    int   bounce_count = 0;
    float vx = 0.f;
    float vy = 0.f;
    bool  is_haemolacria_reflect = false;
    bool  pierces_enemies = false;
};

class SplitLaserSystem {
public:
    static constexpr int   k_parasite_enemy_cooldown_frames = 15;
    static constexpr float k_main_damage_ratio     = 0.30f;
    static constexpr float k_hit_damage_ratio      = 0.50f;
    static constexpr float k_length_shrink_ratio   = 0.70f;
    static constexpr float k_gen0_length           =
        static_cast<float>(SCREEN_WIDTH) / 3.f;
    static constexpr int   k_default_life_frames   = 14;
    static constexpr int   k_max_split_generation  = 3;
    static constexpr float k_haemolacria_reflect_length = 1000.f;
    static constexpr int   k_haemolacria_reflect_life_min = 45;
    static constexpr int   k_haemolacria_reflect_life_max = 60;

    static void reset(std::vector<SplitLaser>& split_lasers);

    // 主激光命中：按敌人独立冷却触发初代分裂
    static void try_spawn_from_main_laser_hit(
        std::vector<SplitLaser>& pending_lasers,
        Enemy& enemy,
        float main_laser_damage,
        bool has_parasite);

    // 短激光命中：按敌人独立冷却触发次级分裂
    static void try_spawn_from_split_laser_hit(
        std::vector<SplitLaser>& pending_lasers,
        Enemy& enemy,
        const SplitLaser& parent_laser,
        bool has_parasite);

    static void update(
        std::vector<SplitLaser>& split_lasers,
        std::vector<Enemy>& enemies,
        bool has_parasite,
        const std::function<void(const Enemy&)>& on_enemy_killed);

    static void render(sf::RenderWindow& window,
                       const std::vector<SplitLaser>& split_lasers);

    static void spawn_haemolacria_reflect_beams(
        float origin_x,
        float origin_y,
        float base_damage,
        std::vector<SplitLaser>& pending_lasers);
};

#endif
