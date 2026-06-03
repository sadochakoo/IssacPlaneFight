#include "brimstone_laser.h"
#include "attack_profile.h"
#include "passive_item.h"
#include "split_laser.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

int find_nearest_enemy_at(const std::vector<Enemy>& enemies, float px, float py) {
    float best = 1e18f;
    int idx = -1;
    for (size_t i = 0; i < enemies.size(); ++i) {
        const float dx = enemies[i].x - px;
        const float dy = enemies[i].y - py;
        const float d2 = dx * dx + dy * dy;
        if (d2 < best) {
            best = d2;
            idx = static_cast<int>(i);
        }
    }
    return idx;
}

bool enemy_intersects_beam_rect(const Enemy& e, const sf::FloatRect& beam) {
    const float ex = e.x - static_cast<float>(e.width) * 0.5f;
    const float ey = e.y - static_cast<float>(e.height) * 0.5f;
    return beam.intersects(sf::FloatRect(ex, ey,
        static_cast<float>(e.width),
        static_cast<float>(e.height)));
}

bool enemy_near_segment(const Enemy& e, sf::Vector2f a, sf::Vector2f b, float half_w) {
    const float ex = e.x;
    const float ey = e.y;
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len2 = dx * dx + dy * dy;
    float t = 0.f;
    if (len2 > 1e-6f) {
        t = ((ex - a.x) * dx + (ey - a.y) * dy) / len2;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
    }
    const float cx = a.x + dx * t;
    const float cy = a.y + dy * t;
    const float ddx = ex - cx;
    const float ddy = ey - cy;
    const float hit_r = half_w + static_cast<float>(std::max(e.width, e.height)) * 0.45f;
    return ddx * ddx + ddy * ddy <= hit_r * hit_r;
}

void build_straight_path(std::vector<sf::Vector2f>& path, float lane_x, float origin_y) {
    path.clear();
    float y = origin_y;
    path.emplace_back(lane_x, y);
    while (y > 8.f) {
        y += BrimstoneLaser::kPathStepY;
        path.emplace_back(lane_x, y);
    }
}

void build_curved_path(std::vector<sf::Vector2f>& path, float lane_x, float origin_y,
                       const std::vector<Enemy>& enemies) {
    path.clear();
    float x = lane_x;
    float y = origin_y;
    path.emplace_back(x, y);

    while (y > 8.f) {
        y += BrimstoneLaser::kPathStepY;

        if (!enemies.empty()) {
            const int target = find_nearest_enemy_at(enemies, x, y);
            if (target >= 0) {
                const float ex = enemies[static_cast<size_t>(target)].x;
                const float ey = enemies[static_cast<size_t>(target)].y;
                const float dx = ex - x;
                const float dy = ey - y;
                const float dist = std::sqrt(dx * dx + dy * dy) + 1.f;
                const float weight = std::min(1.f, 420.f / dist);
                x += dx * weight * 0.38f;
            }
        }

        if (x < 12.f) x = 12.f;
        if (x > static_cast<float>(SCREEN_WIDTH) - 12.f)
            x = static_cast<float>(SCREEN_WIDTH) - 12.f;

        path.emplace_back(x, y);
    }
}

void rebuild_laser_paths(Player& player, const std::vector<Enemy>& enemies) {
    const AttackProfile profile = buildAttackProfile(player);
    player.laser_paths.clear();
    player.laser_half_width = profile.laser_half_width;
    player.laser_lane_count = profile.parallel_lanes;
    player.laser_lane_spacing = profile.parallel_spacing;

    const bool curved = profile.usesHoming();

    std::vector<sf::Vector2f> origins;
    origins.push_back(player.pos);
    for (const BabyCompanion& baby : player.babies) {
        origins.push_back(baby.pos);
    }

    for (const sf::Vector2f& origin : origins) {
        for (int lane = 0; lane < profile.parallel_lanes; ++lane) {
            float offset = 0.f;
            if (profile.parallel_lanes > 1) {
                const float mid = (profile.parallel_lanes - 1) * 0.5f;
                offset = (static_cast<float>(lane) - mid) * profile.parallel_spacing;
            }
            const float lane_x = origin.x + offset;

            std::vector<sf::Vector2f> path;
            if (curved) {
                build_curved_path(path, lane_x, origin.y, enemies);
            } else {
                build_straight_path(path, lane_x, origin.y);
            }
            player.laser_paths.push_back(std::move(path));
        }
    }

    if (!player.laser_paths.empty()) {
        player.laser_path = player.laser_paths[0];
    } else {
        player.laser_path.clear();
    }
}

void damage_enemies_in_beam_rect(
    float center_x,
    float top_y,
    float width,
    std::vector<Enemy>& enemies,
    int damage,
    std::vector<SplitLaser>& pending_split_lasers,
    bool has_parasite,
    const std::function<void(const Enemy&)>& on_kill)
{
    const float half = width * 0.5f;
    const sf::FloatRect beam(center_x - half, 0.f, width, top_y);

    for (size_t ei = 0; ei < enemies.size(); ) {
        if (!enemy_intersects_beam_rect(enemies[ei], beam)) {
            ++ei;
            continue;
        }

        SplitLaserSystem::try_spawn_from_main_laser_hit(
            pending_split_lasers,
            enemies[ei],
            static_cast<float>(damage),
            has_parasite);

        enemies[ei].hp -= damage;
        if (enemies[ei].hp <= 0) {
            Enemy dead = enemies[ei];
            enemies.erase(enemies.begin() + ei);
            if (on_kill) on_kill(dead);
        } else {
            ++ei;
        }
    }
}

void damage_enemies_along_path(
    const std::vector<sf::Vector2f>& path,
    std::vector<Enemy>& enemies,
    int damage,
    float half_width,
    std::vector<SplitLaser>& pending_split_lasers,
    bool has_parasite,
    const std::function<void(const Enemy&)>& on_kill)
{
    if (path.size() < 2) return;

    for (size_t ei = 0; ei < enemies.size(); ) {
        bool hit = false;
        for (size_t s = 1; s < path.size(); ++s) {
            if (enemy_near_segment(enemies[ei], path[s - 1], path[s], half_width)) {
                hit = true;
                break;
            }
        }
        if (!hit) {
            ++ei;
            continue;
        }

        SplitLaserSystem::try_spawn_from_main_laser_hit(
            pending_split_lasers,
            enemies[ei],
            static_cast<float>(damage),
            has_parasite);

        enemies[ei].hp -= damage;
        if (enemies[ei].hp <= 0) {
            Enemy dead = enemies[ei];
            enemies.erase(enemies.begin() + ei);
            if (on_kill) on_kill(dead);
        } else {
            ++ei;
        }
    }
}

void draw_charge_arc(sf::RenderWindow& window, const Player& player) {
    if (player.charge_timer <= 0) return;

    const float ratio = static_cast<float>(player.charge_timer)
                      / static_cast<float>(player.max_charge);
    if (ratio <= 0.f) return;

    const sf::Vector2f center(player.pos.x, player.pos.y - 36.f);
    const float radius = 26.f;
    const int seg_count = static_cast<int>(48.f * ratio);
    if (seg_count < 2) return;

    sf::VertexArray arc(sf::LineStrip, static_cast<std::size_t>(seg_count + 1));
    const float start = -static_cast<float>(M_PI) * 0.5f;
    const float sweep = ratio * static_cast<float>(M_PI) * 2.f;

    for (int i = 0; i <= seg_count; ++i) {
        const float t = start + sweep * (static_cast<float>(i) / static_cast<float>(seg_count));
        arc[static_cast<std::size_t>(i)].position =
            center + sf::Vector2f(std::cos(t) * radius, std::sin(t) * radius);
        arc[static_cast<std::size_t>(i)].color = sf::Color(60, 255, 100, 230);
    }
    window.draw(arc);
}

void draw_triangle_strip_beam(sf::RenderWindow& window,
                              const std::vector<sf::Vector2f>& path,
                              float half_width)
{
    if (path.size() < 2) return;

    sf::VertexArray strip(sf::TriangleStrip);
    strip.resize(path.size() * 2);

    for (size_t i = 0; i < path.size(); ++i) {
        sf::Vector2f tangent(0.f, -1.f);
        if (i + 1 < path.size()) {
            tangent = path[i + 1] - path[i];
        } else if (i > 0) {
            tangent = path[i] - path[i - 1];
        }
        const float len = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
        if (len > 1e-4f) {
            tangent /= len;
        }
        sf::Vector2f perp(-tangent.y, tangent.x);

        const sf::Vector2f p = path[i];
        strip[i * 2].position     = p + perp * half_width;
        strip[i * 2 + 1].position = p - perp * half_width;
        strip[i * 2].color     = sf::Color(255, 50, 40, 210);
        strip[i * 2 + 1].color = sf::Color(40, 0, 0, 200);
    }

    window.draw(strip);
}

void draw_straight_beam(sf::RenderWindow& window,
                        float lane_x,
                        float bottom_y,
                        float width)
{
    const float height = bottom_y;
    if (height <= 2.f) return;

    sf::RectangleShape core(sf::Vector2f(width, height));
    core.setOrigin(width * 0.5f, height);
    core.setPosition(lane_x, bottom_y);
    core.setFillColor(sf::Color(255, 40, 40, 200));
    window.draw(core);

    sf::RectangleShape glow(sf::Vector2f(width + 12.f, height));
    glow.setOrigin((width + 12.f) * 0.5f, height);
    glow.setPosition(lane_x, bottom_y);
    glow.setFillColor(sf::Color(255, 90, 60, 70));
    window.draw(glow);
}

} // namespace

bool BrimstoneLaser::hasBrimstone(const Player& player) {
    return player.stats.brimstone_level > 0;
}

bool BrimstoneLaser::hasHomingSynergy(const Player& player) {
    return buildAttackProfile(player).usesHoming();
}

bool BrimstoneLaser::hasDoubleSynergy(const Player& player) {
    return buildAttackProfile(player).parallel_lanes > 1;
}

void BrimstoneLaser::updateChargeInput(Player& player, bool fire_pressed) {
    if (player.stats.brimstone_level <= 0) {
        player.charge_timer = 0;
        player.prev_fire_pressed = false;
        return;
    }

    if (fire_pressed) {
        if (player.charge_timer < player.max_charge) {
            ++player.charge_timer;
        }
    } else {
        if (player.prev_fire_pressed) {
            if (player.charge_timer >= player.max_charge) {
                const AttackProfile profile = buildAttackProfile(player);
                player.laser_duration_timer = player.laser_duration_max;
                player.laser_damage_cooldown = 0;
                player.laser_lane_count = profile.parallel_lanes;
                player.laser_lane_spacing = profile.parallel_spacing;
                player.laser_half_width = profile.laser_half_width;
            }
            player.charge_timer = 0;
        }
    }

    player.prev_fire_pressed = fire_pressed;
}

void BrimstoneLaser::updateLaser(
    Player& player,
    std::vector<Enemy>& enemies,
    int damage,
    std::vector<SplitLaser>& split_lasers,
    const std::function<void(const Enemy&)>& on_enemy_killed)
{
    if (player.laser_duration_timer <= 0) {
        player.laser_path.clear();
        player.laser_paths.clear();
        return;
    }

    --player.laser_duration_timer;

    rebuild_laser_paths(player, enemies);

    if (player.laser_damage_cooldown > 0) {
        --player.laser_damage_cooldown;
        return;
    }
    player.laser_damage_cooldown = 4;

    const AttackProfile profile = buildAttackProfile(player);
    const bool curved = profile.usesHoming();
    const float half_w = player.laser_half_width;
    const bool has_parasite = player.stats.has_parasite;

    std::vector<SplitLaser> pending_split_lasers;
    pending_split_lasers.reserve(16);

    if (curved) {
        for (const auto& path : player.laser_paths) {
            damage_enemies_along_path(
                path, enemies, damage, half_w,
                pending_split_lasers, has_parasite,
                on_enemy_killed);
        }
    } else {
        for (const auto& path : player.laser_paths) {
            if (path.empty()) continue;
            damage_enemies_in_beam_rect(
                path[0].x,
                path[0].y,
                half_w * 2.f,
                enemies,
                damage,
                pending_split_lasers,
                has_parasite,
                on_enemy_killed);
        }
    }

    if (!pending_split_lasers.empty()) {
        split_lasers.insert(
            split_lasers.end(),
            pending_split_lasers.begin(),
            pending_split_lasers.end());
    }
}

void BrimstoneLaser::render(sf::RenderWindow& window, const Player& player) {
    draw_charge_arc(window, player);

    if (player.laser_duration_timer <= 0) return;

    const AttackProfile profile = buildAttackProfile(player);
    const bool curved = profile.usesHoming();
    const float beam_w = player.laser_half_width * 2.f;

    if (curved) {
        for (const auto& path : player.laser_paths) {
            draw_triangle_strip_beam(window, path, player.laser_half_width);
        }
    } else {
        for (const auto& path : player.laser_paths) {
            if (path.empty()) continue;
            draw_straight_beam(window, path[0].x, path[0].y, beam_w);
        }
    }
}

void BrimstoneLaser::reset(Player& player) {
    player.charge_timer = 0;
    player.laser_duration_timer = 0;
    player.laser_damage_cooldown = 0;
    player.prev_fire_pressed = false;
    player.laser_path.clear();
    player.laser_paths.clear();
    player.laser_lane_count = 1;
    player.laser_half_width = 10.f;
}
