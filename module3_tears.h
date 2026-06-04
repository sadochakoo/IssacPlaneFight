/*
 * module3_tears.h - 模块三：苹果刀片 / 背叛 / 神性 / 玻璃碎片 玩家弹幕
 */

#ifndef MODULE3_TEARS_H
#define MODULE3_TEARS_H

#include "player_stats.h"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

class Player;

namespace item_ui {
struct pickup_toast;
}

std::vector<item_ui::pickup_toast>& item_pickup_toast_queue();

namespace module3 {

struct DamagePopup {
    float     x = 0.f;
    float     y = 0.f;
    float     vy = -55.f;
    int       damage = 0;
    int       frames = 0;
    sf::Color color = sf::Color::White;
};

inline constexpr int   k_betrayal_duration_frames = 600; // 10s @60fps
inline constexpr float k_godhead_aura_radius      = 60.f;
inline constexpr int   k_godhead_aura_interval    = 6;
inline constexpr float k_godhead_aura_damage_ratio  = 0.30f;
inline constexpr int   k_apple_shred_damage       = 5;
inline constexpr float k_glass_max_dist           = 300.f;

void configure_player_bullet(Bullet& bullet, const Player& player);

void update_player_bullets(
    const Player&           player,
    std::vector<Bullet>&    bullets,
    std::vector<Enemy>&     enemies,
    float                   dt,
    int                     frame_count,
    std::vector<DamagePopup>& popups);

int compute_tear_damage(
    const Bullet& bullet,
    int           player_damage_fallback,
    float         hit_cx,
    float         hit_cy);

bool can_apple_razor_split(const Bullet& bullet);
void enqueue_apple_razor_splits(
    const Bullet&          parent,
    const Player&          player,
    std::vector<Bullet>&   pending);

void on_apple_razor_hit(float x, float y);
void on_glass_hit(float x, float y, float dist);
void on_godhead_direct_hit(float x, float y);
void on_betrayal_applied();

float glass_distance_multiplier(float dist_px);

void render_player_bullet(
    sf::RenderTarget&   target,
    const Bullet&       bullet,
    int                 frame_count);

void tick_damage_popups(std::vector<DamagePopup>& popups, float dt);
void render_damage_popups(sf::RenderTarget& target, const std::vector<DamagePopup>& popups);

void draw_pickup_toast_center(
    sf::RenderTarget&     target,
    const sf::Font&       font,
    bool                  font_ok,
    const std::wstring&   text,
    int                   frames_remaining);

void draw_status_icons(
    sf::RenderTarget& target,
    const sf::Font&   font,
    bool              font_ok,
    const Player&     player,
    float             x,
    float             y);

void draw_betray_popup(
    sf::RenderTarget& target,
    const sf::Font&   font,
    bool              font_ok,
    int               frames_remaining);

int betray_popup_frames();
void trigger_betray_popup();
void tick_betray_popup();

const wchar_t* pickup_message_for_extension(const char* item_id);

/** 仅开启一种模块三泪弹并弹出对应拾取提示（调试用） */
void enable_single_item_test(Player& player, const char* item_id);

} // namespace module3

#endif
