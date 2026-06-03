/*
 * ui_system.h - 菜单 / HUD / 结算等纯 UI 渲染（与玩法逻辑解耦）
 */

#ifndef UI_SYSTEM_H
#define UI_SYSTEM_H

#include <SFML/Graphics.hpp>

class Player;

struct HudOverlay {
    int   next_level_threshold = 0;
    int   score_at_level_start = 0;
    bool  wave_pause           = false;
    float wave_pause_timer     = 0.f;
};

class UISystem {
public:
    bool initialize();

    void draw_main_menu(sf::RenderWindow& window);

    void draw_game_over(sf::RenderWindow& window,
                        int score,
                        int player_level,
                        int item_count);

    void draw_hud(sf::RenderWindow& window,
                  const Player& player,
                  int current_wave,
                  int score,
                  const HudOverlay& overlay);

    void draw_player_hp_hearts(sf::RenderWindow& window,
                               const Player& player,
                               float x,
                               float y);

private:
    sf::Font font_;
    bool     font_loaded_ = false;

    bool load_font();
    const sf::Font& font() const;
};

#endif
