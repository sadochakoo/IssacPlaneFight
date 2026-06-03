#include "split_laser.h"
#include <cmath>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

int g_next_laser_id = 1;

int allocate_laser_id() {
    return g_next_laser_id++;
}

float beam_width_for_generation(int generation, bool is_haemolacria_reflect) {
    if (is_haemolacria_reflect) {
        return 34.f;
    }
    if (generation <= 0) {
        return 28.f;
    }
    if (generation == 1) {
        return 20.f;
    }
    if (generation == 2) {
        return 14.f;
    }
    return 9.f;
}

bool enemy_hits_laser_segment(const Enemy& enemy,
                              float ax, float ay,
                              float bx, float by,
                              float half_width)
{
    const float ex = enemy.x;
    const float ey = enemy.y;
    const float dx = bx - ax;
    const float dy = by - ay;
    const float len2 = dx * dx + dy * dy;
    float t = 0.f;
    if (len2 > 1e-6f) {
        t = ((ex - ax) * dx + (ey - ay) * dy) / len2;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
    }
    const float cx = ax + dx * t;
    const float cy = ay + dy * t;
    const float ddx = ex - cx;
    const float ddy = ey - cy;
    const float hit_r = half_width
        + static_cast<float>(std::max(enemy.width, enemy.height)) * 0.45f;
    return ddx * ddx + ddy * ddy <= hit_r * hit_r;
}

void push_y_split_laser(std::vector<SplitLaser>& pending,
                        float origin_x,
                        float origin_y,
                        float angle,
                        float max_length,
                        float damage,
                        int generation,
                        bool is_baby_tear)
{
    SplitLaser laser;
    laser.laser_id = allocate_laser_id();
    laser.x = origin_x;
    laser.y = origin_y;
    laser.angle = angle;
    laser.max_length = max_length;
    laser.damage = damage;
    laser.generation = generation;
    laser.life_timer = SplitLaserSystem::k_default_life_frames;
    laser.is_baby_tear = is_baby_tear;
    laser.bounce_count = 0;
    laser.vx = 0.f;
    laser.vy = 0.f;
    pending.push_back(laser);
}

void update_reflect_laser_motion(SplitLaser& laser, float dt) {
    if (!laser.is_haemolacria_reflect) {
        if (laser.bounce_count <= 0 && std::fabs(laser.vx) < 1.f && std::fabs(laser.vy) < 1.f) {
            return;
        }
    }

    laser.x += laser.vx * dt;
    laser.y += laser.vy * dt;
    laser.angle = std::atan2(laser.vy, laser.vx);

    const float margin = 12.f;
    const float left = margin;
    const float right = static_cast<float>(SCREEN_WIDTH) - margin;
    const float top = margin;

    const float tip_x = laser.x + std::cos(laser.angle) * laser.max_length;
    const float tip_y = laser.y + std::sin(laser.angle) * laser.max_length;

    auto try_bounce = [&](float& vel, bool hit) {
        if (hit && laser.bounce_count > 0) {
            vel = -vel;
            --laser.bounce_count;
        }
    };

    if (laser.x < left) {
        laser.x = left;
        try_bounce(laser.vx, laser.vx < 0.f);
    } else if (laser.x > right) {
        laser.x = right;
        try_bounce(laser.vx, laser.vx > 0.f);
    }

    if (laser.y < top) {
        laser.y = top;
        try_bounce(laser.vy, laser.vy < 0.f);
    }

    if (tip_x < left) {
        try_bounce(laser.vx, laser.vx < 0.f);
    } else if (tip_x > right) {
        try_bounce(laser.vx, laser.vx > 0.f);
    }

    if (tip_y < top) {
        try_bounce(laser.vy, laser.vy < 0.f);
    }

    laser.angle = std::atan2(laser.vy, laser.vx);
}

void enqueue_generation_zero_splits(std::vector<SplitLaser>& pending,
                                    float hit_x,
                                    float hit_y,
                                    float split_damage)
{
    const float up = -static_cast<float>(M_PI) * 0.5f;
    const float spread = static_cast<float>(M_PI) * 0.25f;
    const float angles[2] = { up - spread, up + spread };

    for (float angle : angles) {
        push_y_split_laser(
            pending,
            hit_x,
            hit_y,
            angle,
            SplitLaserSystem::k_gen0_length,
            split_damage,
            0,
            false);
    }
}

void enqueue_child_splits(std::vector<SplitLaser>& pending,
                          float hit_x,
                          float hit_y,
                          const SplitLaser& parent)
{
    const int child_generation = parent.generation + 1;
    if (child_generation >= SplitLaserSystem::k_max_split_generation) {
        return;
    }

    const float child_damage = parent.damage * SplitLaserSystem::k_hit_damage_ratio;
    if (child_damage < 0.5f) {
        return;
    }

    const float child_length = parent.max_length * SplitLaserSystem::k_length_shrink_ratio;
    const float up = -static_cast<float>(M_PI) * 0.5f;
    const float spread = static_cast<float>(M_PI) * 0.22f;
    const float angles[2] = { up - spread, up + spread };

    for (float angle : angles) {
        push_y_split_laser(
            pending,
            hit_x,
            hit_y,
            angle,
            child_length,
            child_damage,
            child_generation,
            parent.is_baby_tear);
    }
}

void draw_flat_split_beam(sf::RenderWindow& window, const SplitLaser& laser)
{
    const float width = beam_width_for_generation(
        laser.generation, laser.is_haemolacria_reflect);
    const float length = laser.max_length;

    sf::RectangleShape core(sf::Vector2f(width, length));
    core.setOrigin(width * 0.5f, 0.f);
    core.setPosition(laser.x, laser.y);
    core.setRotation(laser.angle * 180.f / static_cast<float>(M_PI) - 90.f);
    core.setFillColor(sf::Color(210, 35, 35, 215));
    window.draw(core);

    sf::RectangleShape glow(sf::Vector2f(width + 8.f, length));
    glow.setOrigin((width + 8.f) * 0.5f, 0.f);
    glow.setPosition(laser.x, laser.y);
    glow.setRotation(laser.angle * 180.f / static_cast<float>(M_PI) - 90.f);
    glow.setFillColor(sf::Color(255, 70, 50, 75));
    window.draw(glow);
}

} // namespace

void SplitLaserSystem::spawn_haemolacria_reflect_beams(
    float origin_x,
    float origin_y,
    float base_damage,
    std::vector<SplitLaser>& pending_lasers)
{
    const int beam_count = 5 + (std::rand() % 2);
    const float move_speed = 280.f;
    const int life_span = k_haemolacria_reflect_life_min
        + (std::rand() % (k_haemolacria_reflect_life_max
                          - k_haemolacria_reflect_life_min + 1));

    for (int i = 0; i < beam_count; ++i) {
        const float angle =
            static_cast<float>(2.0 * M_PI) * static_cast<float>(i)
            / static_cast<float>(beam_count);

        SplitLaser laser;
        laser.laser_id = allocate_laser_id();
        laser.x = origin_x;
        laser.y = origin_y;
        laser.angle = angle;
        laser.max_length = k_haemolacria_reflect_length;
        laser.damage = base_damage * 0.45f;
        laser.generation = 0;
        laser.life_timer = life_span;
        laser.is_baby_tear = false;
        laser.is_haemolacria_reflect = true;
        laser.pierces_enemies = true;
        laser.bounce_count = 2;
        laser.vx = std::cos(angle) * move_speed;
        laser.vy = std::sin(angle) * move_speed;
        pending_lasers.push_back(laser);
    }
}

void SplitLaserSystem::reset(std::vector<SplitLaser>& split_lasers) {
    split_lasers.clear();
    g_next_laser_id = 1;
}

void SplitLaserSystem::try_spawn_from_main_laser_hit(
    std::vector<SplitLaser>& pending_lasers,
    Enemy& enemy,
    float main_laser_damage,
    bool has_parasite)
{
    if (!has_parasite) return;
    if (enemy.parasite_split_cooldown > 0) return;

    const float split_damage = main_laser_damage * k_main_damage_ratio;
    if (split_damage < 0.5f) return;

    enqueue_generation_zero_splits(
        pending_lasers, enemy.x, enemy.y, split_damage);
    enemy.parasite_split_cooldown = k_parasite_enemy_cooldown_frames;
}

void SplitLaserSystem::try_spawn_from_split_laser_hit(
    std::vector<SplitLaser>& pending_lasers,
    Enemy& enemy,
    const SplitLaser& parent_laser,
    bool has_parasite)
{
    if (!has_parasite) return;
    if (parent_laser.is_baby_tear) return;
    if (enemy.parasite_split_cooldown > 0) return;
    if (parent_laser.generation >= k_max_split_generation) return;

    enqueue_child_splits(pending_lasers, enemy.x, enemy.y, parent_laser);
    enemy.parasite_split_cooldown = k_parasite_enemy_cooldown_frames;
}

void SplitLaserSystem::update(
    std::vector<SplitLaser>& split_lasers,
    std::vector<Enemy>& enemies,
    bool has_parasite,
    const std::function<void(const Enemy&)>& on_enemy_killed)
{
    std::vector<SplitLaser> pending_lasers;
    pending_lasers.reserve(32);

    for (size_t i = 0; i < split_lasers.size(); ) {
        SplitLaser& laser = split_lasers[i];

        if (laser.is_haemolacria_reflect || laser.bounce_count > 0) {
            update_reflect_laser_motion(laser, FRAME_TIME);
        }

        --laser.life_timer;
        if (laser.life_timer <= 0) {
            split_lasers.erase(split_lasers.begin() + i);
        } else {
            ++i;
        }
    }

    for (size_t li = 0; li < split_lasers.size(); ++li) {
        const SplitLaser& laser = split_lasers[li];
        const float ax = laser.x;
        const float ay = laser.y;
        const float bx = ax + std::cos(laser.angle) * laser.max_length;
        const float by = ay + std::sin(laser.angle) * laser.max_length;
        const float half_w = beam_width_for_generation(
            laser.generation, laser.is_haemolacria_reflect) * 0.5f;

        if (laser.pierces_enemies) {
            for (size_t ei = 0; ei < enemies.size(); ) {
                if (!enemy_hits_laser_segment(enemies[ei], ax, ay, bx, by, half_w)) {
                    ++ei;
                    continue;
                }

                const int hit_dmg = std::max(
                    1,
                    static_cast<int>(std::ceil(laser.damage * 0.14f)));

                enemies[ei].hp -= hit_dmg;
                if (enemies[ei].hp <= 0) {
                    Enemy dead = enemies[ei];
                    enemies.erase(enemies.begin() + ei);
                    if (on_enemy_killed) {
                        on_enemy_killed(dead);
                    }
                } else {
                    ++ei;
                }
            }
            continue;
        }

        for (size_t ei = 0; ei < enemies.size(); ) {
            if (!enemy_hits_laser_segment(enemies[ei], ax, ay, bx, by, half_w)) {
                ++ei;
                continue;
            }

            const int hit_dmg = std::max(
                1,
                static_cast<int>(std::ceil(laser.damage * k_hit_damage_ratio)));

            try_spawn_from_split_laser_hit(
                pending_lasers, enemies[ei], laser, has_parasite);

            enemies[ei].hp -= hit_dmg;
            if (enemies[ei].hp <= 0) {
                Enemy dead = enemies[ei];
                enemies.erase(enemies.begin() + ei);
                if (on_enemy_killed) {
                    on_enemy_killed(dead);
                }
            } else {
                ++ei;
            }
        }
    }

    if (!pending_lasers.empty()) {
        split_lasers.insert(
            split_lasers.end(),
            pending_lasers.begin(),
            pending_lasers.end());
    }
}

void SplitLaserSystem::render(sf::RenderWindow& window,
                              const std::vector<SplitLaser>& split_lasers)
{
    for (const SplitLaser& laser : split_lasers) {
        draw_flat_split_beam(window, laser);
    }
}
