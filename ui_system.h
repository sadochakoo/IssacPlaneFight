/*
 * ui_system.h - 菜单 / HUD / 宝箱 / 选道具 / 背景 / 角色动画（纯渲染与 UI 状态）
 *
 * 主程接入要点（第二部分）：
 * 1. 分数达标升级时：ui.spawn_item_chest()，保持 PLAYING，不要立刻 LEVEL_UP
 * 2. 每帧 PLAYING：ui.update_item_chest(dt, player.pos, 16.f)；若返回 true → ui.begin_item_pick(...) + LEVEL_UP
 * 3. LEVEL_UP：ui.update_item_pick(window) / draw_item_pick；选中后 applyItem
 * 4. MENU → draw_waiting_screen；新增 CHARACTER_SELECT 状态 → draw/update_character_select
 * 5. 战斗中：draw_room_background(floor)；draw_isaac_player 替代 CircleShape 机体
 */

#ifndef UI_SYSTEM_H
#define UI_SYSTEM_H

#include <SFML/Graphics.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class Player;
struct Bullet;

// ==================== 角色（仅 UI / 视觉；玩法角色类由主程扩展） ====================
enum class CharacterId {
    Isaac = 0,
    // 预留：Cain, Judas, ...
    Count
};

struct CharacterVisualProfile {
    CharacterId id;
    const char* display_name_w; // 窄字符展示名（UI 内转 wstring）
    const char* anim_path_pattern; // 如 "gfx/player/%d.png"
    int           anim_frame_count;
};

// ==================== 道具宝箱（位置由 UI 更新，主程只读碰撞结果） ====================
struct ItemChestVisual {
    bool  active  = false;
    bool  opened  = false;
    float x       = 400.f;
    float y       = -64.f;
    float vy      = 48.f;
    float half_w  = 22.f;
    float half_h  = 18.f;
};

// ==================== 三选一选项（含等级徽章） ====================
struct ItemPickOption {
    int registry_index = 0;
    int item_level     = 0; // 0~4
};

struct HudOverlay {
    int   next_level_threshold = 0;
    int   score_at_level_start = 0;
    bool  wave_pause           = false;
    float wave_pause_timer     = 0.f;
};

class UISystem {
public:
    bool initialize();

    // ---------- 流程界面 ----------
    void draw_waiting_screen(sf::RenderWindow& window);
    /** @return 选中的 CharacterId 枚举值，未确认返回 -1 */
    int  update_character_select(sf::RenderWindow& window);
    void draw_character_select(sf::RenderWindow& window);

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

    // ---------- 关卡背景 & 玩家精灵 ----------
    /** 旋转 90° + cover 铺满窗口；floor_level 映射 room-1..5，6+ 随机 */
    void draw_room_background(sf::RenderWindow& window, int floor_level);
    void draw_isaac_player(sf::RenderWindow& window, float x, float y, float anim_time_sec);

    /**
     * 关卡背景验收（PLAYING 时每帧在 draw_room_background 内自动调用，主程无需改）：
     * - F1~F5 或 Shift+1~5：直接显示 room-1 ~ room-5（数字键避免与主程 1~6 道具用例冲突）
     * - PageUp / PageDown：下一间 / 上一间
     * - Ctrl + ← / →：同上（避免与纯方向键移动冲突）
     * - F6：恢复按 floor_level 自动选图
     */
    int debug_room_override() const;

    // ---------- 道具宝箱 ----------
    void spawn_item_chest(float center_x = 400.f);
    /** 下落 + 检测与玩家圆碰撞；触碰后 opened=true 并返回 true */
    bool update_item_chest(float dt, const sf::Vector2f& player_pos, float player_radius);
    void draw_item_chest(sf::RenderWindow& window);
    bool has_active_chest() const;
    sf::FloatRect item_chest_bounds() const;

    // ---------- 选道具面板（替代 LevelUpPanel 绘制） ----------
    void begin_item_pick(const std::vector<int>& registry_indices, const Player& player);
    int  update_item_pick(sf::RenderWindow& window);
    void draw_item_pick(sf::RenderWindow& window);
    bool is_item_pick_active() const;

    /** 根据玩家当前层数/持有情况推断展示等级（0~4） */
    static int item_display_level(int registry_index, const Player& player);

    static const CharacterVisualProfile& profile_for(CharacterId id);

private:
    sf::Font font_;
    bool     font_loaded_ = false;

    ItemChestVisual chest_;
    std::vector<ItemPickOption> pick_options_;

    sf::Texture tex_chest_closed_;
    sf::Texture tex_chest_open_;
    sf::Texture tex_waiting_;
    sf::Texture tex_char_select_;
    sf::Texture room_textures_[5];
    sf::Texture isaac_frames_[6];
    std::unordered_map<int, sf::Texture> item_icon_by_registry_;

    bool chest_tex_loaded_   = false;
    bool flow_tex_loaded_    = false;
    bool room_tex_loaded_    = false;
    bool isaac_tex_loaded_   = false;

    int random_room_for_floor6_ = 1;
    /** -1 = 跟随 floor_level；1~5 = 强制 room-N */
    int debug_room_override_ = -1;

    bool load_font();
    bool load_flow_textures();
    bool load_chest_textures();
    bool load_room_textures();
    bool load_isaac_frames();
    bool load_item_icon(int registry_index);

    const sf::Font& font() const;
    void draw_sprite_fit(sf::RenderWindow& window,
                         const sf::Texture& tex,
                         const sf::FloatRect& target,
                         bool preserve_aspect = true) const;
    void draw_sprite_cover_rotated_90(sf::RenderWindow& window,
                                      const sf::Texture& tex) const;
    void update_debug_room_input();
    void draw_debug_room_hint(sf::RenderWindow& window) const;
    int resolve_room_index(int floor_level) const;
    void draw_level_badge(sf::RenderWindow& window,
                          float x,
                          float y,
                          int level) const;
    void draw_parchment_card(sf::RenderWindow& window,
                             const sf::FloatRect& rect,
                             const sf::Color& accent) const;

    static const char* item_icon_path(int registry_index);
};

#endif
