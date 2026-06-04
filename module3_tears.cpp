#include "module3_tears.h"
#include "item_system.h"

#include <algorithm>
#include <cmath>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

int g_betray_popup_frames = 0;

float dist_xy(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

void push_trail(std::vector<sf::Vector2f>& trail, float x, float y, size_t cap) {
    trail.push_back(sf::Vector2f(x, y));
    if (trail.size() > cap) {
        trail.erase(trail.begin());
    }
}

void draw_blade(sf::RenderTarget& target, float x, float y, float spin, float scale) {
    sf::ConvexShape blade;
    blade.setPointCount(3);
    const float s = 7.f * scale;
    blade.setPoint(0, sf::Vector2f(0.f, -s * 1.4f));
    blade.setPoint(1, sf::Vector2f(s * 0.55f, s * 0.7f));
    blade.setPoint(2, sf::Vector2f(-s * 0.55f, s * 0.7f));
    blade.setOrigin(0.f, 0.f);
    blade.setPosition(x, y);
    blade.setRotation(spin * 57.2958f);
    blade.setFillColor(sf::Color(220, 230, 245, 240));
    blade.setOutlineColor(sf::Color(180, 190, 210));
    blade.setOutlineThickness(1.f);
    target.draw(blade);
}

void draw_heart(sf::RenderTarget& target, float x, float y, float size, sf::Color c) {
    sf::CircleShape l(size * 0.45f);
    sf::CircleShape r(size * 0.45f);
    l.setOrigin(size * 0.45f, size * 0.45f);
    r.setOrigin(size * 0.45f, size * 0.45f);
    l.setPosition(x - size * 0.35f, y - size * 0.15f);
    r.setPosition(x + size * 0.35f, y - size * 0.15f);
    l.setFillColor(c);
    r.setFillColor(c);
    target.draw(l);
    target.draw(r);
    sf::ConvexShape tip;
    tip.setPointCount(3);
    tip.setPoint(0, sf::Vector2f(0.f, size * 0.9f));
    tip.setPoint(1, sf::Vector2f(-size, -size * 0.1f));
    tip.setPoint(2, sf::Vector2f(size, -size * 0.1f));
    tip.setOrigin(0.f, 0.f);
    tip.setPosition(x, y);
    tip.setFillColor(c);
    target.draw(tip);
}

} // namespace

namespace module3 {

void configure_player_bullet(Bullet& bullet, const Player& player) {
    bullet.module3_trail.clear();
    const auto& ext = player.stats_ext;
    bullet.module3_apple_razor   = ext.has_apple;
    bullet.module3_betrayal_tear = ext.has_betrayal;
    bullet.module3_godhead       = ext.has_godhead;
    bullet.module3_glass         = ext.has_glass_shard;
    bullet.module3_apple_shred   = false;
    bullet.module3_spin        = 0.f;
    bullet.module3_spawn_x      = bullet.x;
    bullet.module3_spawn_y      = bullet.y;
    bullet.module3_godhead_aura_cd = 0;
    bullet.module3_prism_flash  = 0;

    if (bullet.module3_apple_razor && !bullet.module3_apple_shred) {
        bullet.bullet_color = sf::Color(210, 220, 235);
        bullet.radius       = std::max(bullet.radius, 7.f);
    } else if (bullet.module3_betrayal_tear) {
        bullet.bullet_color = sf::Color(200, 80, 220);
    } else if (bullet.module3_godhead) {
        bullet.bullet_color = sf::Color(255, 220, 80);
        bullet.homing       = true;
        bullet.homing_strength = std::max(bullet.homing_strength, 520.f);
    } else if (bullet.module3_glass) {
        bullet.bullet_color = sf::Color(160, 240, 255, 190);
        bullet.radius       = std::max(bullet.radius, 7.f);
    }
}

float glass_distance_multiplier(float dist_px) {
    const float d = std::max(0.f, std::min(k_glass_max_dist, dist_px));
    if (d <= 50.f) {
        return 1.f - d * 0.0066f; // 100% -> ~67%
    }
    if (d <= 150.f) {
        return 0.67f - (d - 50.f) * 0.0027f; // ~67% -> ~40%
    }
    return std::max(0.25f, 0.40f - (d - 150.f) * 0.001f);
}

int compute_tear_damage(
    const Bullet& bullet,
    int           player_damage_fallback,
    float         hit_cx,
    float         hit_cy)
{
    float dmg = bullet.damage;
    if (dmg <= 0.f) {
        dmg = static_cast<float>(player_damage_fallback);
    }
    if (bullet.module3_glass && !bullet.module3_apple_shred) {
        const float dist = dist_xy(
            bullet.module3_spawn_x, bullet.module3_spawn_y, hit_cx, hit_cy);
        dmg *= glass_distance_multiplier(dist);
    }
    return std::max(1, static_cast<int>(std::ceil(dmg)));
}

bool can_apple_razor_split(const Bullet& bullet) {
    return bullet.module3_apple_razor
        && !bullet.module3_apple_shred
        && bullet.generation == 0
        && !bullet.is_haemolacria_shard
        && !bullet.is_haemolacria_orb;
}

void enqueue_apple_razor_splits(
    const Bullet&          parent,
    const Player&          player,
    std::vector<Bullet>&   pending)
{
    if (!can_apple_razor_split(parent)) {
        return;
    }
    const float speed = 300.f;
    for (int i = 0; i < 8; ++i) {
        const float ang = static_cast<float>(i) * static_cast<float>(M_PI) * 0.25f;
        Bullet child;
        child.x   = parent.x;
        child.y   = parent.y;
        child.vx  = speed * std::sin(ang);
        child.vy  = -speed * std::cos(ang);
        child.life = 0.85f;
        child.damage = static_cast<float>(k_apple_shred_damage);
        child.generation = 1;
        child.module3_apple_razor = true;
        child.module3_apple_shred = true;
        child.module3_spin = ang * 57.2958f;
        child.bullet_color = sf::Color(230, 235, 250, 220);
        child.radius = 4.5f;
        child.has_parasite = false;
        child.homing = false;
        child.module3_spawn_x = parent.x;
        child.module3_spawn_y = parent.y;
        (void)player;
        pending.push_back(child);
    }
}

static void apply_godhead_aura(
    Bullet&                   bullet,
    std::vector<Enemy>&       enemies,
    std::vector<DamagePopup>& popups)
{
    if (!bullet.module3_godhead || bullet.is_dead) {
        return;
    }
    if (bullet.module3_godhead_aura_cd > 0) {
        --bullet.module3_godhead_aura_cd;
        return;
    }
    bullet.module3_godhead_aura_cd = k_godhead_aura_interval;

    const int aura_dmg = std::max(
        1,
        static_cast<int>(std::ceil(bullet.damage * k_godhead_aura_damage_ratio)));

    for (Enemy& enemy : enemies) {
        if (enemy.hp <= 0) {
            continue;
        }
        const float d = dist_xy(bullet.x, bullet.y, enemy.x, enemy.y);
        if (d > k_godhead_aura_radius) {
            continue;
        }
        enemy.hp -= aura_dmg;
        DamagePopup pop;
        pop.x      = enemy.x;
        pop.y      = enemy.y - 28.f;
        pop.damage = aura_dmg;
        pop.frames = 22;
        pop.color  = sf::Color(255, 220, 80);
        popups.push_back(pop);
    }
}

void update_player_bullets(
    const Player&             player,
    std::vector<Bullet>&      bullets,
    std::vector<Enemy>&       enemies,
    float                     dt,
    int                       frame_count,
    std::vector<DamagePopup>& popups)
{
    (void)player;
    (void)frame_count;
    for (Bullet& b : bullets) {
        if (b.is_haemolacria_orb || b.is_dead) {
            continue;
        }
        if (b.module3_apple_razor || b.module3_apple_shred) {
            b.module3_spin += dt * 720.f;
        }
        if (b.module3_betrayal_tear) {
            push_trail(b.module3_trail, b.x, b.y, 14);
        }
        if (b.module3_glass) {
            if (b.module3_prism_flash > 0) {
                --b.module3_prism_flash;
            } else {
                b.module3_prism_flash = 10;
            }
        }
        if (b.module3_godhead) {
            apply_godhead_aura(b, enemies, popups);
        }
    }
}

void on_apple_razor_hit(float x, float y) {
    for (int i = 0; i < 8; ++i) {
        const float ang = static_cast<float>(i) * static_cast<float>(M_PI) * 0.25f;
        // particles spawned from main via spawn_particles callback - use module2 style
        (void)ang;
    }
    (void)x;
    (void)y;
}

void on_glass_hit(float /*x*/, float /*y*/, float /*dist*/) {}
void on_godhead_direct_hit(float /*x*/, float /*y*/) {}

void on_betrayal_applied() {
    trigger_betray_popup();
}

int betray_popup_frames() {
    return g_betray_popup_frames;
}

void trigger_betray_popup() {
    g_betray_popup_frames = 75;
}

void tick_betray_popup() {
    if (g_betray_popup_frames > 0) {
        --g_betray_popup_frames;
    }
}

void render_player_bullet(
    sf::RenderTarget& target,
    const Bullet&     bullet,
    int               frame_count)
{
    if (bullet.is_haemolacria_orb || bullet.is_dead) {
        return;
    }

    if (bullet.module3_godhead) {
        const float pulse = 1.f + 0.08f * std::sin(frame_count * 0.2f);
        sf::CircleShape aura(k_godhead_aura_radius * pulse);
        aura.setOrigin(aura.getRadius(), aura.getRadius());
        aura.setPosition(bullet.x, bullet.y);
        aura.setFillColor(sf::Color(255, 230, 120, 28));
        aura.setOutlineColor(sf::Color(255, 240, 180, 70));
        aura.setOutlineThickness(1.5f);
        target.draw(aura);

        sf::CircleShape halo(bullet.radius + 5.f);
        halo.setOrigin(halo.getRadius(), halo.getRadius());
        halo.setPosition(bullet.x, bullet.y);
        halo.setFillColor(sf::Color(255, 240, 150, 50));
        target.draw(halo);

        sf::CircleShape core(bullet.radius);
        core.setOrigin(core.getRadius(), core.getRadius());
        core.setPosition(bullet.x, bullet.y);
        core.setFillColor(sf::Color(255, 220, 60, 240));
        core.setOutlineColor(sf::Color(255, 255, 200));
        core.setOutlineThickness(1.5f);
        target.draw(core);
        return;
    }

    if (bullet.module3_betrayal_tear) {
        for (size_t i = 1; i < bullet.module3_trail.size(); ++i) {
            const float t = static_cast<float>(i)
                / static_cast<float>(bullet.module3_trail.size());
            sf::Vertex seg[] = {
                sf::Vertex(
                    bullet.module3_trail[i - 1],
                    sf::Color(255, 120, 200, static_cast<sf::Uint8>(80 * t))),
                sf::Vertex(
                    bullet.module3_trail[i],
                    sf::Color(255, 180, 220, 0)),
            };
            target.draw(seg, 2, sf::Lines);
        }
        draw_heart(target, bullet.x, bullet.y, 6.f, sf::Color(220, 60, 180));
        for (int p = 0; p < 3; ++p) {
            const float ang = bullet.module3_spin * 0.02f + static_cast<float>(p) * 2.1f;
            draw_heart(
                target,
                bullet.x - std::cos(ang) * 10.f,
                bullet.y - std::sin(ang) * 8.f - 6.f,
                2.5f,
                sf::Color(255, 150, 210, 140));
        }
        return;
    }

    if (bullet.module3_apple_razor || bullet.module3_apple_shred) {
        draw_blade(
            target,
            bullet.x,
            bullet.y,
            bullet.module3_spin,
            bullet.module3_apple_shred ? 0.65f : 1.f);
        return;
    }

    if (bullet.module3_glass) {
        sf::ConvexShape shard;
        shard.setPointCount(4);
        const float hw = bullet.radius * 0.85f;
        shard.setPoint(0, sf::Vector2f(-hw, 0.f));
        shard.setPoint(1, sf::Vector2f(0.f, -hw * 1.2f));
        shard.setPoint(2, sf::Vector2f(hw, 0.f));
        shard.setPoint(3, sf::Vector2f(0.f, hw));
        shard.setOrigin(0.f, 0.f);
        shard.setPosition(bullet.x, bullet.y);
        shard.setRotation(bullet.module3_spin);
        shard.setFillColor(sf::Color(180, 240, 255, 170));
        shard.setOutlineColor(sf::Color(255, 255, 255, 200));
        shard.setOutlineThickness(1.f);
        target.draw(shard);
        if (bullet.module3_prism_flash >= 8) {
            sf::CircleShape prism(3.f);
            prism.setOrigin(3.f, 3.f);
            prism.setPosition(bullet.x + hw * 0.6f, bullet.y - hw * 0.5f);
            prism.setFillColor(sf::Color(255, 100, 255, 180));
            target.draw(prism);
        }
        return;
    }
}

void tick_damage_popups(std::vector<DamagePopup>& popups, float dt) {
    for (auto& p : popups) {
        p.y += p.vy * dt;
        --p.frames;
    }
    popups.erase(
        std::remove_if(
            popups.begin(),
            popups.end(),
            [](const DamagePopup& p) { return p.frames <= 0; }),
        popups.end());
}

void render_damage_popups(
    sf::RenderTarget&              target,
    const std::vector<DamagePopup>& popups)
{
    static sf::Font font;
    static bool     tried = false;
    static bool     ok    = false;
    if (!tried) {
        tried = true;
        ok = font.loadFromFile("C:/Windows/Fonts/arial.ttf")
             || font.loadFromFile("C:/Windows/Fonts/msyh.ttc");
    }
    for (const DamagePopup& p : popups) {
        if (!ok) {
            sf::CircleShape dot(3.f);
            dot.setOrigin(3.f, 3.f);
            dot.setPosition(p.x, p.y);
            dot.setFillColor(p.color);
            target.draw(dot);
            continue;
        }
        const unsigned size = (p.color.r > 200) ? 16u : (p.color.g > 200 ? 13u : 11u);
        sf::Text text(std::to_string(p.damage), font, size);
        text.setFillColor(p.color);
        text.setOutlineColor(sf::Color(0, 0, 0, 120));
        text.setOutlineThickness(1.f);
        const sf::FloatRect b = text.getLocalBounds();
        text.setOrigin(b.width * 0.5f, b.height);
        text.setPosition(p.x, p.y);
        target.draw(text);
    }
}

void draw_pickup_toast_center(
    sf::RenderTarget&   target,
    const sf::Font&     font,
    bool                font_ok,
    const std::wstring& text,
    int                 frames_remaining)
{
    if (frames_remaining <= 0 || text.empty()) {
        return;
    }
    const float alpha = std::min(1.f, frames_remaining / 40.f);
    sf::RectangleShape panel(sf::Vector2f(560.f, 44.f));
    panel.setOrigin(280.f, 22.f);
    panel.setPosition(400.f, 420.f);
    panel.setFillColor(sf::Color(30, 20, 50, static_cast<sf::Uint8>(200 * alpha)));
    panel.setOutlineColor(sf::Color(200, 180, 255, static_cast<sf::Uint8>(220 * alpha)));
    panel.setOutlineThickness(2.f);
    target.draw(panel);

    if (!font_ok) {
        return;
    }
    sf::Text label(text, font, 20);
    label.setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(255 * alpha)));
    label.setOutlineColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(100 * alpha)));
    label.setOutlineThickness(1.f);
    const sf::FloatRect b = label.getLocalBounds();
    label.setOrigin(b.width * 0.5f, b.height * 0.5f);
    label.setPosition(400.f, 420.f);
    target.draw(label);
}

void draw_status_icons(
    sf::RenderTarget& target,
    const sf::Font&   font,
    bool              font_ok,
    const Player&     player,
    float             x,
    float             y)
{
    float cx = x;
    const auto& ext = player.stats_ext;

    auto draw_icon_label = [&](const wchar_t* glyph, sf::Color c) {
        if (font_ok) {
            sf::Text t(std::wstring(1, glyph[0]), font, 16);
            t.setFillColor(c);
            t.setPosition(cx, y);
            target.draw(t);
        } else {
            sf::CircleShape dot(6.f);
            dot.setOrigin(6.f, 6.f);
            dot.setPosition(cx + 6.f, y + 8.f);
            dot.setFillColor(c);
            target.draw(dot);
        }
        cx += 22.f;
    };

    if (ext.has_apple) {
        draw_icon_label(L"🔪", sf::Color(220, 230, 245));
    }
    if (ext.has_betrayal) {
        draw_icon_label(L"💜", sf::Color(200, 100, 255));
    }
    if (ext.has_godhead) {
        draw_icon_label(L"👁", sf::Color(255, 220, 80));
    }
    if (ext.has_glass_shard) {
        draw_icon_label(L"💎", sf::Color(120, 240, 255));
    }
}

void draw_betray_popup(
    sf::RenderTarget& target,
    const sf::Font&   font,
    bool              font_ok,
    int               frames_remaining)
{
    if (frames_remaining <= 0) {
        return;
    }
    const float a = std::min(1.f, frames_remaining / 30.f);
    if (font_ok) {
        sf::Text t(L"倒戈!", font, 26);
        t.setFillColor(sf::Color(255, 120, 200, static_cast<sf::Uint8>(255 * a)));
        t.setOutlineColor(sf::Color(80, 0, 80, static_cast<sf::Uint8>(180 * a)));
        t.setOutlineThickness(1.f);
        const sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(b.width * 0.5f, b.height * 0.5f);
        t.setPosition(400.f, 360.f);
        target.draw(t);
    }
}

const wchar_t* pickup_message_for_extension(const char* item_id) {
    if (!item_id) {
        return L"";
    }
    const std::string id(item_id);
    if (id == "apple" || id == "apple_razor") {
        return L"获得苹果刀片-子弹命中后分裂";
    }
    if (id == "betrayal") {
        return L"获得背叛-子弹使敌人倒戈";
    }
    if (id == "godhead") {
        return L"获得神性- 追踪光环持续伤害";
    }
    if (id == "glass_shard") {
        return L"获得玻璃碎片- 距离越近伤害越高";
    }
    return nullptr;
}

void enable_single_item_test(Player& player, const char* item_id) {
    player.stats_ext.has_apple       = false;
    player.stats_ext.has_betrayal   = false;
    player.stats_ext.has_godhead     = false;
    player.stats_ext.has_glass_shard = false;

    if (!item_id) {
        return;
    }
    const std::string id(item_id);
    if (id == "apple" || id == "apple_razor") {
        player.stats_ext.has_apple = true;
    } else if (id == "betrayal") {
        player.stats_ext.has_betrayal = true;
    } else if (id == "godhead") {
        player.stats_ext.has_godhead = true;
        if (player.stats.tracking_level < 1) {
            ++player.stats.tracking_level;
        }
    } else if (id == "glass_shard") {
        player.stats_ext.has_glass_shard = true;
    }

    if (const wchar_t* msg = pickup_message_for_extension(item_id)) {
        item_ui::push_pickup_toast(
            item_pickup_toast_queue(), item_id, msg);
    }
}

} // namespace module3
