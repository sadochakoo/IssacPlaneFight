#include "haemolacria.h"
#include <cmath>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

void spawn_radial_tears(
    const Bullet& ball,
    float shard_speed,
    std::vector<Bullet>& pending_bullets)
{
    constexpr int k_shard_count = 8;

    for (int i = 0; i < k_shard_count; ++i) {
        const float angle =
            static_cast<float>(2.0 * M_PI) * static_cast<float>(i)
            / static_cast<float>(k_shard_count);

        Bullet shard;
        shard.x = ball.x;
        shard.y = ball.y;
        shard.vx = shard_speed * std::cos(angle);
        shard.vy = shard_speed * std::sin(angle);
        shard.life = 2.8f;
        shard.damage = ball.damage;
        shard.has_parasite = false;
        shard.is_baby_tear = false;
        shard.is_haemolacria_shard = true;
        shard.is_haemolacria_orb = false;
        shard.homing = false;
        shard.bullet_color = sf::Color(180, 20, 30, 255);
        shard.generation = 1;
        shard.radius = 7.f;
        pending_bullets.push_back(shard);
    }
}

void update_orb_visual_scale(Bullet& orb) {
    const float t = std::min(1.f, std::max(0.f, orb.arc_progress));
    if (t < 0.5f) {
        orb.visual_scale = 1.0f + t * 1.4f;
    } else {
        orb.visual_scale = 1.7f - (t - 0.5f) * 2.2f;
    }
    if (orb.visual_scale < 0.45f) {
        orb.visual_scale = 0.45f;
    }
}

} // namespace

bool HaemolacriaSystem::overrides_attack(const Player& player) {
    return player.stats.has_haemolacria;
}

bool HaemolacriaSystem::try_fire(Player& player, std::vector<Bullet>& bullets) {
    if (!player.stats.has_haemolacria) {
        return false;
    }
    if (player.fire_cooldown > 0) {
        return false;
    }

    player.fire_cooldown = k_fire_cooldown_frames;

    const float shot_speed = static_cast<float>(player.stats.shot_speed);

    Bullet orb;
    orb.x = player.pos.x;
    orb.y = player.pos.y - 24.f;
    orb.start_y = orb.y;
    orb.target_y = player.pos.y - k_orb_land_offset_y;
    if (orb.target_y < 48.f) {
        orb.target_y = 48.f;
    }
    orb.vx = 0.f;
    orb.vy = -shot_speed * 0.55f;
    orb.damage = static_cast<float>(player.stats.damage);
    orb.life = 999.f;
    orb.is_haemolacria_orb = true;
    orb.is_dead = false;
    orb.arc_progress = 0.f;
    orb.visual_scale = 1.f;
    orb.radius = 16.f;
    orb.bullet_color = sf::Color(120, 0, 20, 240);
    orb.has_parasite = false;
    orb.homing = false;
    bullets.push_back(orb);
    return true;
}

void HaemolacriaSystem::haemolacria_explode(
    const Player& player,
    const Bullet& ball,
    std::vector<Bullet>& pending_bullets,
    std::vector<SplitLaser>& pending_lasers)
{
    if (player.stats.brimstone_level > 0) {
        SplitLaserSystem::spawn_haemolacria_reflect_beams(
            ball.x, ball.y, ball.damage, pending_lasers);
        return;
    }

    const float shard_speed = static_cast<float>(player.stats.shot_speed) * 0.85f;
    spawn_radial_tears(ball, shard_speed, pending_bullets);
}

void HaemolacriaSystem::update_haemolacria_orbs(
    Player& player,
    std::vector<Bullet>& bullets,
    std::vector<SplitLaser>& split_lasers,
    float dt)
{
    std::vector<Bullet> pending_bullets;
    std::vector<SplitLaser> pending_lasers;
    pending_bullets.reserve(16);
    pending_lasers.reserve(12);

    const float shot_speed = static_cast<float>(player.stats.shot_speed);

    for (size_t i = 0; i < bullets.size(); ) {
        Bullet& b = bullets[i];

        if (!b.is_haemolacria_orb || b.is_dead) {
            ++i;
            continue;
        }

        const float travel = std::fabs(b.start_y - b.target_y);
        const float progress_step = (shot_speed * dt) / std::max(travel, 1.f);
        b.arc_progress += progress_step;

        const float t = std::min(1.f, b.arc_progress);
        const float linear = b.start_y + (b.target_y - b.start_y) * t;
        const float bump = travel * 0.38f * 4.f * t * (1.f - t);
        b.y = linear - bump;
        b.x += b.vx * dt;

        update_orb_visual_scale(b);

        const bool reached_land = (b.y <= b.target_y) || (b.arc_progress >= 1.f);
        if (reached_land) {
            b.is_dead = true;
            haemolacria_explode(player, b, pending_bullets, pending_lasers);
            bullets.erase(bullets.begin() + i);
            continue;
        }

        ++i;
    }

    if (!pending_bullets.empty()) {
        bullets.insert(
            bullets.end(),
            pending_bullets.begin(),
            pending_bullets.end());
    }
    if (!pending_lasers.empty()) {
        split_lasers.insert(
            split_lasers.end(),
            pending_lasers.begin(),
            pending_lasers.end());
    }
}

void HaemolacriaSystem::render_orbs(sf::RenderWindow& window,
                                    const std::vector<Bullet>& bullets)
{
    for (const Bullet& b : bullets) {
        if (!b.is_haemolacria_orb || b.is_dead) {
            continue;
        }
        const float draw_radius = b.radius * b.visual_scale;
        sf::CircleShape ball(draw_radius);
        ball.setOrigin(draw_radius, draw_radius);
        ball.setPosition(b.x, b.y);
        ball.setFillColor(sf::Color(120, 0, 20, 240));
        ball.setOutlineThickness(2.f);
        ball.setOutlineColor(sf::Color(200, 30, 40, 200));
        window.draw(ball);
    }
}
