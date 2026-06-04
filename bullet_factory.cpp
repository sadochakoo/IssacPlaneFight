#include "bullet_factory.h"
#include "baby_system.h"
#include "brimstone_laser.h"
#include "parasite_bullet.h"
#include "haemolacria.h"
#include "module3_tears.h"
#include <algorithm>
#include <cmath>

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
    const Player& player,
    sf::Vector2f origin,
    const AttackProfile& profile,
    float shot_speed,
    float range_sec,
    bool is_baby_tear)
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
        b.homing = profile.usesHoming() && !is_baby_tear;
        b.homing_strength = profile.homing_strength;
        b.bullet_color = profile.bullet_color;
        setup_player_bullet(b, player, is_baby_tear);
        bullets.push_back(b);
    }
}

static bool parasite_bullet_out_of_bounds(const Bullet& b) {
    return b.life <= 0.f ||
           b.x < -50.f || b.x > 850.f ||
           b.y < -50.f || b.y > 950.f;
}

static void handle_parasite_wall_bounce(
    Bullet& bullet,
    std::vector<Bullet>& pending_bullets)
{
    const float margin = 10.f;
    const float left = margin;
    const float right = static_cast<float>(SCREEN_WIDTH) - margin;
    const float top = margin;

    bool bounced = false;

    if (bullet.x < left) {
        bullet.x = left;
        bullet.vx = std::fabs(bullet.vx);
        bounced = true;
    } else if (bullet.x > right) {
        bullet.x = right;
        bullet.vx = -std::fabs(bullet.vx);
        bounced = true;
    }

    if (bullet.y < top) {
        bullet.y = top;
        bullet.vy = std::fabs(bullet.vy);
        bounced = true;
    }

    if (bounced && bullet.bounce_split_cooldown <= 0) {
        enqueue_parasite_wall_splits(bullet, pending_bullets);
        bullet.bounce_split_cooldown = 8;
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

    if (HaemolacriaSystem::try_fire(player, bullets)) {
        return;
    }

    if (profile.usesBrimstone()) {
        return;
    }

    if (player.fire_cooldown > 0) return;

    player.fire_cooldown = player.stats.tear_rate;
    const float shot_speed = static_cast<float>(player.stats.shot_speed);
    const float range_sec = static_cast<float>(player.stats.range);

    spawn_lane_bullets(
        bullets, player, player.pos, profile, shot_speed, range_sec, false);
    BabySystem::tryFireBullets(player, profile, bullets);
}

void BulletFactory::updateBullets(
    Player& player,
    std::vector<Bullet>& bullets,
    std::vector<SplitLaser>& split_lasers,
    const std::vector<Enemy>& enemies,
    float player_shot_speed,
    float dt)
{
    HaemolacriaSystem::update_haemolacria_orbs(player, bullets, split_lasers, dt);

    std::vector<Bullet> pending_bullets;
    pending_bullets.reserve(128);

    for (size_t i = 0; i < bullets.size(); ) {
        Bullet& b = bullets[i];

        if (b.is_haemolacria_orb || b.is_dead) {
            ++i;
            continue;
        }

        if (b.bounce_split_cooldown > 0) {
            --b.bounce_split_cooldown;
        }

        const bool steer_homing =
            (b.homing || b.module3_godhead) && !b.is_baby_tear && !enemies.empty();
        if (steer_homing) {
            const int target = find_nearest_enemy_index(enemies, b.x, b.y);
            if (target >= 0 && target < static_cast<int>(enemies.size())) {
                float dx = enemies[target].x - b.x;
                float dy = enemies[target].y - b.y;
                const float steer = b.homing_strength * dt;
                if (dx > 5.f) {
                    b.vx += steer;
                } else if (dx < -5.f) {
                    b.vx -= steer;
                }
                if (b.module3_godhead) {
                    if (dy > 5.f) {
                        b.vy += steer * 0.85f;
                    } else if (dy < -5.f) {
                        b.vy -= steer * 0.85f;
                    }
                }

                float max_vx = player_shot_speed * 0.85f;
                float max_vy = player_shot_speed * 0.85f;
                if (b.vx > max_vx) b.vx = max_vx;
                if (b.vx < -max_vx) b.vx = -max_vx;
                if (b.vy > max_vy) b.vy = max_vy;
                if (b.vy < -max_vy) b.vy = -max_vy;
            }
        }

        b.x += b.vx * dt;
        b.y += b.vy * dt;
        b.life -= dt;

        if (b.has_parasite) {
            handle_parasite_wall_bounce(b, pending_bullets);
            if (parasite_bullet_out_of_bounds(b)) {
                bullets.erase(bullets.begin() + i);
                continue;
            }
        } else {
            if (parasite_bullet_out_of_bounds(b)) {
                bullets.erase(bullets.begin() + i);
                continue;
            }
        }

        ++i;
    }

    if (!pending_bullets.empty()) {
        bullets.insert(
            bullets.end(),
            pending_bullets.begin(),
            pending_bullets.end());
    }
}
