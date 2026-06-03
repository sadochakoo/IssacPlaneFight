#include "ui_system.h"
#include "player_stats.h"
#include "attack_profile.h"

#include <algorithm>
#include <string>

namespace {

constexpr float k_screen_center_x = 400.f;

void center_text_origin(sf::Text& text, bool center_y = false) {
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin(bounds.width / 2.f, center_y ? bounds.height / 2.f : 0.f);
}

} // namespace

bool UISystem::load_font() {
    if (font_.loadFromFile("C:/Windows/Fonts/msyh.ttc")) return true;
    if (font_.loadFromFile("C:/Windows/Fonts/msyhbd.ttc")) return true;
    if (font_.loadFromFile("C:/Windows/Fonts/simhei.ttf")) return true;
    if (font_.loadFromFile("C:/Windows/Fonts/simsun.ttc")) return true;
    if (font_.loadFromFile("C:/Windows/Fonts/arial.ttf")) return true;
    return false;
}

bool UISystem::initialize() {
    font_loaded_ = load_font();
    return font_loaded_;
}

const sf::Font& UISystem::font() const {
    return font_;
}

void UISystem::draw_player_hp_hearts(sf::RenderWindow& window,
                                     const Player& player,
                                     float x,
                                     float y)
{
    for (int i = 0; i < player.stats.max_hp; ++i) {
        sf::CircleShape heart(8.f);
        heart.setPosition(x + static_cast<float>(i) * 20.f, y);

        if (i < static_cast<int>(player.stats.hp)) {
            heart.setFillColor(sf::Color::Red);
        } else {
            heart.setFillColor(sf::Color(80, 20, 20));
        }
        window.draw(heart);
    }
}

void UISystem::draw_main_menu(sf::RenderWindow& window) {
    if (!font_loaded_) {
        return;
    }

    sf::Text title(L"飞机大战 - 以撒版", font_, 48);
    title.setFillColor(sf::Color::White);
    center_text_origin(title);
    title.setPosition(k_screen_center_x, 200.f);
    window.draw(title);

    sf::Text subtitle(L"模仿以撒的结合属性机制", font_, 24);
    subtitle.setFillColor(sf::Color(200, 200, 200));
    center_text_origin(subtitle);
    subtitle.setPosition(k_screen_center_x, 280.f);
    window.draw(subtitle);

    sf::Text controls(L"WASD / 方向键 → 移动", font_, 20);
    controls.setFillColor(sf::Color(180, 180, 180));
    center_text_origin(controls);
    controls.setPosition(k_screen_center_x, 400.f);
    window.draw(controls);

    sf::Text controls2(L"空格 / J → 向上射击", font_, 20);
    controls2.setFillColor(sf::Color(180, 180, 180));
    center_text_origin(controls2);
    controls2.setPosition(k_screen_center_x, 430.f);
    window.draw(controls2);

    sf::Text controls3(L"击杀敌人获得分数，分数达标后升级三选一", font_, 18);
    controls3.setFillColor(sf::Color(150, 150, 150));
    center_text_origin(controls3);
    controls3.setPosition(k_screen_center_x, 480.f);
    window.draw(controls3);

    sf::Text start(L"按 Enter 开始游戏", font_, 28);
    start.setFillColor(sf::Color(255, 255, 100));
    center_text_origin(start);
    start.setPosition(k_screen_center_x, 580.f);
    window.draw(start);
}

void UISystem::draw_game_over(sf::RenderWindow& window,
                              int score,
                              int player_level,
                              int item_count)
{
    if (!font_loaded_) {
        return;
    }

    sf::Text over_text(L"游戏结束", font_, 48);
    over_text.setFillColor(sf::Color(255, 80, 80));
    center_text_origin(over_text);
    over_text.setPosition(k_screen_center_x, 250.f);
    window.draw(over_text);

    sf::Text final_score(L"最终分数: " + std::to_wstring(score), font_, 28);
    final_score.setFillColor(sf::Color::White);
    center_text_origin(final_score);
    final_score.setPosition(k_screen_center_x, 330.f);
    window.draw(final_score);

    sf::Text final_level(L"最高等级: " + std::to_wstring(player_level), font_, 24);
    final_level.setFillColor(sf::Color(255, 255, 150));
    center_text_origin(final_level);
    final_level.setPosition(k_screen_center_x, 370.f);
    window.draw(final_level);

    sf::Text final_items(
        L"收集道具: " + std::to_wstring(item_count) + L" 个", font_, 24);
    final_items.setFillColor(sf::Color(200, 200, 200));
    center_text_origin(final_items);
    final_items.setPosition(k_screen_center_x, 410.f);
    window.draw(final_items);

    sf::Text restart(L"按 Enter 重新开始", font_, 28);
    restart.setFillColor(sf::Color(255, 255, 100));
    center_text_origin(restart);
    restart.setPosition(k_screen_center_x, 500.f);
    window.draw(restart);
}

void UISystem::draw_hud(sf::RenderWindow& window,
                        const Player& player,
                        int current_wave,
                        int score,
                        const HudOverlay& overlay)
{
    if (!font_loaded_) {
        draw_player_hp_hearts(window, player, 12.f, 52.f);
        return;
    }

    if (overlay.wave_pause && overlay.wave_pause_timer > 0.f) {
        sf::RectangleShape banner_bg(sf::Vector2f(500.f, 60.f));
        banner_bg.setOrigin(250.f, 30.f);
        banner_bg.setPosition(k_screen_center_x, 450.f);
        banner_bg.setFillColor(sf::Color(0, 0, 0, 160));
        window.draw(banner_bg);

        std::wstring wave_msg;
        if (overlay.wave_pause_timer > 2.0f) {
            wave_msg = L"第 " + std::to_wstring(current_wave) + L" 层已清除！";
        } else {
            wave_msg = L"第 " + std::to_wstring(current_wave)
                     + L" 层 - 一大波敌人来袭！";
        }

        sf::Text wave_text(wave_msg, font_, 26);
        wave_text.setFillColor(sf::Color(255, 220, 100));
        center_text_origin(wave_text, true);
        wave_text.setPosition(k_screen_center_x, 440.f);
        window.draw(wave_text);

        sf::Text count_text(
            L"下一波倒计时: "
                + std::to_wstring(static_cast<int>(overlay.wave_pause_timer + 0.99f))
                + L" 秒",
            font_, 16);
        count_text.setFillColor(sf::Color(200, 200, 200));
        center_text_origin(count_text);
        count_text.setPosition(k_screen_center_x, 462.f);
        window.draw(count_text);
    }

    draw_player_hp_hearts(window, player, 12.f, 52.f);

    sf::Text score_text(L"分数: " + std::to_wstring(score), font_, 18);
    score_text.setFillColor(sf::Color::White);
    score_text.setPosition(12.f, 12.f);
    window.draw(score_text);

    sf::Text level_text(L"等级: " + std::to_wstring(current_wave), font_, 18);
    level_text.setFillColor(sf::Color(255, 255, 100));
    level_text.setPosition(12.f, 32.f);
    window.draw(level_text);

    const float level_needed = static_cast<float>(
        overlay.next_level_threshold - overlay.score_at_level_start);
    float level_progress = static_cast<float>(
        score - overlay.score_at_level_start);
    float progress = level_needed > 0.f ? level_progress / level_needed : 0.f;
    if (progress > 1.f) progress = 1.f;
    if (progress < 0.f) progress = 0.f;

    sf::Text next_text(
        L"下一级: " + std::to_wstring(overlay.next_level_threshold), font_, 14);
    next_text.setFillColor(sf::Color(150, 150, 150));
    next_text.setPosition(12.f, 75.f);
    window.draw(next_text);

    sf::RectangleShape prog_bar(sf::Vector2f(150.f, 6.f));
    prog_bar.setPosition(12.f, 95.f);
    prog_bar.setFillColor(sf::Color(40, 40, 40));
    window.draw(prog_bar);

    sf::RectangleShape prog_fill(sf::Vector2f(150.f * progress, 6.f));
    prog_fill.setPosition(12.f, 95.f);
    prog_fill.setFillColor(sf::Color(255, 200, 50));
    window.draw(prog_fill);

    sf::Text items_text(L"道具: " + std::to_wstring(player.item_count), font_, 14);
    items_text.setFillColor(sf::Color(200, 200, 200));
    items_text.setPosition(12.f, 108.f);
    window.draw(items_text);

    std::wstring held_str = L"层数: ";
    if (player.stats.brimstone_level > 0)
        held_str += L"硫磺" + std::to_wstring(player.stats.brimstone_level) + L" ";
    if (player.stats.tracking_level > 0)
        held_str += L"弯勺" + std::to_wstring(player.stats.tracking_level) + L" ";
    if (player.stats.extra_bullets > 0)
        held_str += L"20/20+" + std::to_wstring(player.stats.extra_bullets) + L" ";
    if (player.stats.baby_count > 0)
        held_str += L"宝宝" + std::to_wstring(player.stats.baby_count) + L" ";
    if (player.stats.has_parasite)
        held_str += L"寄生虫 ";
    if (player.stats.has_haemolacria)
        held_str += L"泪血症 ";

    const AttackProfile hud_profile = buildAttackProfile(player);
    if (hud_profile.usesBrimstone()) {
        held_str += L"[激光";
        if (hud_profile.brimstone_level >= 2) held_str += L"粗";
        if (hud_profile.usesHoming()) held_str += L"+追踪";
        if (hud_profile.parallel_lanes > 1) held_str += L"+多道";
        held_str += L"]";
    } else if (hud_profile.usesHoming() || hud_profile.parallel_lanes > 1) {
        held_str += L"[";
        if (hud_profile.parallel_lanes > 1) held_str += L"多弹道";
        if (hud_profile.usesHoming()) held_str += L"+追踪";
        held_str += L"]";
    }

    sf::Text held_text(held_str, font_, 14);
    held_text.setFillColor(sf::Color(255, 220, 100));
    held_text.setPosition(12.f, 870.f);
    window.draw(held_text);

    const std::wstring dmg_w = std::to_wstring(player.stats.damage);
    sf::Text dmg_text(
        L"伤害: " + dmg_w.substr(0, std::min<size_t>(4, dmg_w.size())),
        font_, 14);
    dmg_text.setFillColor(sf::Color(255, 150, 100));
    dmg_text.setPosition(12.f, 126.f);
    window.draw(dmg_text);

    sf::RectangleShape exp_bar_bg(sf::Vector2f(200.f, 10.f));
    exp_bar_bg.setPosition(300.f, 10.f);
    exp_bar_bg.setFillColor(sf::Color(40, 40, 40));
    window.draw(exp_bar_bg);

    float fill_ratio = level_progress / (level_needed > 0.f ? level_needed : 1.f);
    if (fill_ratio > 1.f) fill_ratio = 1.f;
    if (fill_ratio < 0.f) fill_ratio = 0.f;

    sf::RectangleShape exp_bar_fill(sf::Vector2f(200.f * fill_ratio, 10.f));
    exp_bar_fill.setPosition(300.f, 10.f);
    exp_bar_fill.setFillColor(sf::Color(255, 200, 0));
    window.draw(exp_bar_fill);
}
