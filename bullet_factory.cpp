#include "bullet_factory.h"
#include "baby_system.h"
#include "brimstone_laser.h"
#include <algorithm>

static int find_nearest_enemy_index(const std::vector<Enemy>& enemies, float bx, float by) {
    float min_dist = 1e9f;
    int nearest = -1;
    for (size_t i = 0; i < enemies.size(); ++i) {
        float dx = enemies[i].x - bx;
        float dy = enemies[i].y - by;
        float dist = dx * dx + dy * dy;
        if (dist < min_dist) {
            min_dist = dist;
            nearest = static_cast<int>(i);
        }
    }
    return nearest;
}

static void spawn_lane_bullets(
    std::vector<Bullet>& bullets,
    sf::Vector2f origin,
    const AttackProfile& profile,
    float shot_speed,
    float range_sec)
{
    for (int lane = 0; lane < profile.parallel_lanes; ++lane) {
        float offset = 0.f;
        if (profile.parallel_lanes > 1) {
            const float mid = (profile.parallel_lanes - 1) * 0.5f;
            offset = (static_cast<float>(lane) - mid) * profile.parallel_spacing;
        }

        Bullet b;
        b.x = origin.x + offset;
        b.y = origin.y - 20.f;
        b.vx = 0.f;
        b.vy = -shot_speed;
        b.life = range_sec;
        b.homing = profile.usesHoming();
        b.homing_strength = profile.homing_strength;
        b.bullet_color = profile.bullet_color;
        bullets.push_back(b);
    }
}

void BulletFactory::tryFire(
    Player& player,
    const AttackProfile& profile,
    std::vector<Bullet>& bullets,
    bool fire_pressed,
    float dt)
{
    (void)fire_pressed;
    (void)dt;

    // 主武器形态：硫磺火走 BrimstoneLaser，此处只处理普通弹幕
    if (profile.usesBrimstone()) {
        return;
    }

    if (player.fire_cooldown > 0) return;

    player.fire_cooldown = player.stats.tear_rate;
    const float shot_speed = static_cast<float>(player.stats.shot_speed);
    const float range_sec = static_cast<float>(player.stats.range);

    spawn_lane_bullets(bullets, player.pos, profile, shot_speed, range_sec);
    BabySystem::tryFireBullets(player, profile, bullets);
}

void BulletFactory::updateBullets(
    std::vector<Bullet>& bullets,
    const std::vector<Enemy>& enemies,
    float player_shot_speed,
    float dt)
{
    for (size_t i = 0; i < bullets.size(); ) {
        if (bullets[i].homing && !enemies.empty()) {
            const int target = find_nearest_enemy_index(enemies, bullets[i].x, bullets[i].y);
            if (target >= 0 && target < static_cast<int>(enemies.size())) {
                float dx = enemies[target].x - bullets[i].x;
                float steer = bullets[i].homing_strength * dt;
                if (dx > 5.f) bullets[i].vx += steer;
                else if (dx < -5.f) bullets[i].vx -= steer;

                float max_vx = player_shot_speed * 0.85f;
                if (bullets[i].vx > max_vx) bullets[i].vx = max_vx;
                if (bullets[i].vx < -max_vx) bullets[i].vx = -max_vx;
            }
        }

        bullets[i].x += bullets[i].vx * dt;
        bullets[i].y += bullets[i].vy * dt;
        bullets[i].life -= dt;

        if (bullets[i].life <= 0.f ||
            bullets[i].x < -50.f || bullets[i].x > 850.f ||
            bullets[i].y < -50.f || bullets[i].y > 950.f) {
            bullets.erase(bullets.begin() + i);
        } else {
            ++i;
        }
    }
}
