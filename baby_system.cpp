#include "baby_system.h"
#include "parasite_bullet.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

void spawn_lane_bullets(
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

} // namespace

void BabySystem::syncCount(Player& player) {
    const int want = std::max(0, player.stats.baby_count);
    while (static_cast<int>(player.babies.size()) < want) {
        BabyCompanion baby;
        const float n = static_cast<float>(player.babies.size() + 1);
        baby.orbit_angle = (n - 1.f) * 2.f * static_cast<float>(M_PI) / static_cast<float>(want + 1);
        player.babies.push_back(baby);
    }
    while (static_cast<int>(player.babies.size()) > want) {
        player.babies.pop_back();
    }
}

void BabySystem::updateOrbit(Player& player, float dt) {
    syncCount(player);
    if (player.babies.empty()) return;

    const float radius = 52.f;
    const float spin = 0.8f;
    const int count = static_cast<int>(player.babies.size());

    for (int i = 0; i < count; ++i) {
        BabyCompanion& baby = player.babies[static_cast<size_t>(i)];
        baby.orbit_angle += spin * dt;
        const float base = 2.f * static_cast<float>(M_PI) * static_cast<float>(i)
                         / static_cast<float>(count);
        const float a = base + baby.orbit_angle;
        baby.pos.x = player.pos.x + std::cos(a) * radius;
        baby.pos.y = player.pos.y + std::sin(a) * radius * 0.55f;
    }
}

void BabySystem::tryFireBullets(
    Player& player,
    const AttackProfile& profile,
    std::vector<Bullet>& bullets)
{
    if (profile.usesBrimstone()) return;
    if (player.stats.baby_count <= 0) return;

    const float shot_speed = static_cast<float>(player.stats.shot_speed);
    const float range_sec = static_cast<float>(player.stats.range);

    for (const BabyCompanion& baby : player.babies) {
        spawn_lane_bullets(
            bullets, player, baby.pos, profile, shot_speed, range_sec, true);
    }
}

void BabySystem::render(sf::RenderWindow& window, const Player& player) {
    for (const BabyCompanion& baby : player.babies) {
        sf::CircleShape body(10.f);
        body.setOrigin(10.f, 10.f);
        body.setPosition(baby.pos);
        body.setFillColor(sf::Color(255, 200, 120, 220));
        body.setOutlineThickness(1.f);
        body.setOutlineColor(sf::Color(255, 240, 200));
        window.draw(body);
    }
}

void BabySystem::reset(Player& player) {
    player.babies.clear();
}
