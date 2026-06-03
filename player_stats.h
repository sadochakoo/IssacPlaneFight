/*
 * player_stats.h - 玩家属性、实体数据、Player 类
 */

#ifndef PLAYER_STATS_H
#define PLAYER_STATS_H

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

class Item;

// ==================== PlayerStats ====================
struct PlayerStats {
    double hp         = 3.0;    // 当前红血 (HP)
    int    max_hp     = 3;      // 最大红血容器
    double damage     = 3.50;   // 伤害值 (Damage)
    int    tear_rate  = 12;     // 射速 / 发射间隔帧数 (TearRate，越小越快)
    double shot_speed = 600.0;  // 弹速 (ShotSpeed)
    double range      = 1.2;    // 射程，子弹存活时间秒 (Range)
    double speed      = 320.0;  // 移速 (Speed)

    // 道具叠加层数（替代 bool has_xxx / tracking 开关）
    int brimstone_level = 0;
    int tracking_level  = 0;
    int baby_count      = 0;
    int extra_bullets   = 0;
    bool has_parasite   = false;
    bool has_haemolacria = false;
};

// ==================== 敌人类型 ====================
enum EnemyType {
    ENEMY_NORMAL       = 0,
    ENEMY_DOUBLE_SHOOT = 1,
    ENEMY_TRACKING     = 2,
    ENEMY_ELITE        = 3,
    ENEMY_BLOOM        = 4,
    ENEMY_SPIRAL       = 5,
    ENEMY_SPIRAL_ELITE = 6,
    ENEMY_TYPE_COUNT   = 7
};

// ==================== 子弹 ====================
struct Bullet {
    float x = 0.f, y = 0.f;
    float vx = 0.f, vy = 0.f;
    float life = 0.f;
    bool  homing = false;
    float homing_strength = 0.f;
    sf::Color bullet_color = sf::Color(100, 200, 255);
    bool  has_parasite = false;
    bool  is_baby_tear = false;
    int   generation = 0;
    float damage = 0.f;
    float radius = 8.f;
    int   bounce_split_cooldown = 0;
    bool  is_haemolacria_shard = false;
    bool  is_haemolacria_orb = false;
    bool  is_dead = false;
    float target_y = 0.f;
    float start_y = 0.f;
    float arc_progress = 0.f;
    float visual_scale = 1.f;

    Bullet() {
        update_radius_from_generation();
    }

    void update_radius_from_generation() {
        if (generation == 0) {
            radius = 8.0f;
        } else if (generation == 1) {
            radius = 7.0f;
        } else if (generation == 2) {
            radius = 5.5f;
        } else if (generation == 3) {
            radius = 4.0f;
        } else {
            radius = 3.5f;
        }
    }
};

// ==================== 其它实体 ====================
struct PowerUp {
    float x = 0.f, y = 0.f;
    int   item_index = 0;
    bool  active = true;
};

struct Enemy {
    float x = 0.f, y = 0.f;
    float vx = 0.f, vy = 0.f;
    int   hp = 0, max_hp = 0;
    int   width = 30, height = 30;
    int   score = 25;
    sf::Color color = sf::Color::Red;
    int   enemy_type = ENEMY_NORMAL;
    float shoot_timer = 0.f;
    float spiral_angle = 0.f;
    int   parasite_split_cooldown = 0;
};

struct Particle {
    float x = 0.f, y = 0.f;
    float vx = 0.f, vy = 0.f;
    float life = 0.f, max_life = 0.f;
    sf::Color color = sf::Color::White;
};

struct BabyCompanion {
    float orbit_angle = 0.f;
    sf::Vector2f pos;
};

// ==================== Player ====================
class Player {
public:
    sf::Vector2f pos;
    PlayerStats  stats;
    PlayerStats  base_stats;

    int  fire_cooldown = 0;
    int  shield_timer = 0;
    int  invisible_timer = 0;
    int  item_count = 0;

    std::vector<std::string> collected_items;
    std::vector<BabyCompanion> babies;

    // 硫磺火状态机（帧计数）
    int  charge_timer = 0;
    int  max_charge = 60;
    int  laser_duration_timer = 0;
    int  laser_duration_max = 30;
    int  laser_damage_cooldown = 0;
    bool prev_fire_pressed = false;

    int   laser_lane_count = 1;
    float laser_lane_spacing = 28.f;
    float laser_half_width = 10.f;

    // 每帧重建的激光路径（弯勺融合用 TriangleStrip）
    std::vector<sf::Vector2f> laser_path;
    std::vector<std::vector<sf::Vector2f>> laser_paths;

    Player() {
        pos = sf::Vector2f(400.f, 700.f);
        base_stats = stats;
        fire_cooldown = 0;
        max_charge = 60;
        laser_duration_max = 30;
    }

    bool hasItem(const std::string& id) const {
        return std::find(collected_items.begin(), collected_items.end(), id) != collected_items.end();
    }

    void applyItem(Item* item);

    void clampStats() {
        if (stats.hp > static_cast<double>(stats.max_hp))
            stats.hp = static_cast<double>(stats.max_hp);
        if (stats.hp < 0.0) stats.hp = 0.0;
        if (stats.max_hp > 12) stats.max_hp = 12;
        if (stats.max_hp < 1) stats.max_hp = 1;
        if (stats.tear_rate < 2) stats.tear_rate = 2;
        if (stats.tear_rate > 90) stats.tear_rate = 90;
        if (stats.speed < 150.0) stats.speed = 150.0;
        if (stats.speed > 800.0) stats.speed = 800.0;
        if (stats.range < 0.3) stats.range = 0.3;
        if (stats.range > 3.0) stats.range = 3.0;
    }

    void update_timers() {
        if (invisible_timer > 0) invisible_timer--;
        if (shield_timer > 0) shield_timer--;
    }

    int get_damage() const {
        return static_cast<int>(std::ceil(stats.damage));
    }

    bool is_alive() const { return stats.hp > 0.0; }

    bool take_damage() {
        if (shield_timer != 0) {
            shield_timer = 0;
            return true;
        }
        if (invisible_timer > 0) return true;
        stats.hp -= 1.0;
        return stats.hp > 0.0;
    }
};

const int   SCREEN_WIDTH  = 800;
const int   SCREEN_HEIGHT = 900;
const float FRAME_TIME    = 1.f / 60.f;

#endif
