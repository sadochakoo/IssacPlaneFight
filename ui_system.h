/*
 * ui_system.h - 菜单 / HUD / 宝箱 / 选道具 / 背景 / 角色动画（纯渲染与 UI 状态）
 *
 * 主程接入要点（第二部分）：
 * 1. 分数达标升级时：ui.spawn_item_chest()，保持 PLAYING，不要立刻 LEVEL_UP
 * 2. 每帧 PLAYING：update_item_chest；返回 ReadyForItemPick 后再 begin_item_pick + LEVEL_UP
 * 3. LEVEL_UP：ui.update_item_pick / draw_item_pick；选中后 applyItem
 * 4. MENU → draw_waiting_screen（全屏参考图）；CHARACTER_SELECT → 参考底图 + 角色立绘
 *    角色类见 game_character.h / CharacterRoster；默认 Isaac + 以撒.png
 * 5. 战斗中：draw_room_background(floor)；draw_isaac_player 替代 CircleShape 机体
 */

#ifndef UI_SYSTEM_H
#define UI_SYSTEM_H

#include "game_character.h"
#include "tear_profile.h"

#include <SFML/Graphics.hpp>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

class Player;
struct Bullet;

/** 宝箱每帧更新结果（主程据此决定是否进入 LEVEL_UP） */
enum class ChestUpdateResult {
    None = 0,           // 下落中 / 开箱展示倒计时中
    ReadyForItemPick,   // 开箱展示结束，可弹出三选一
};

// ==================== 道具宝箱（位置由 UI 更新，主程只读碰撞结果） ====================
struct ItemChestVisual {
    bool  active  = false;
    bool  opened  = false;
    float opening_reveal_timer = 0.f; // >0：碰撞后展示 chest_open 的停留时间
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

    /** 新一局 / 死亡回菜单时调用：清空宝箱与选道具残留，避免挡住下一局升级宝箱 */
    void reset_run_state();

    /** 从等待界面刚进入选角时调用，防止 Enter 连按直接开始游戏 */
    void notify_enter_character_select_screen();

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

    /** 屏幕顶部调试提示（使用 HUD 同款字体） */
    void draw_debug_toast(sf::RenderWindow& window,
                          const std::wstring& message) const;

    bool has_font() const { return font_loaded_; }
    const sf::Font& hud_font() const { return font_; }

    // ---------- 关卡背景 & 玩家精灵 ----------
    /** 旋转 90° + cover 铺满窗口；floor_level 映射 room-1..5，6+ 随机 */
    void draw_room_background(sf::RenderWindow& window, int floor_level);
    void draw_isaac_player(sf::RenderWindow& window, float x, float y, float anim_time_sec);

    /** 玩家泪弹贴图渲染（道具驱动贴图见 tear_profile） */
    void draw_player_tears(sf::RenderWindow& window,
                           const Player& player,
                           const std::vector<Bullet>& tears);

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
    /**
     * 下落 → 碰撞后进入开箱展示（停住 + chest_open，默认约 0.75s）
     * → 倒计时结束返回 ReadyForItemPick
     */
    ChestUpdateResult update_item_chest(float dt,
                                        const sf::Vector2f& player_pos,
                                        float player_radius);
    void draw_item_chest(sf::RenderWindow& window);
    bool has_active_chest() const;
    /** 碰撞后、三选一弹出前的开箱展示阶段 */
    bool is_chest_opening_reveal() const;
    bool is_chest_opened() const;
    sf::FloatRect item_chest_bounds() const;

    // ---------- 选道具面板（替代 LevelUpPanel 绘制） ----------
    void begin_item_pick(const std::vector<int>& registry_indices, const Player& player);
    int  update_item_pick(sf::RenderWindow& window);
    void draw_item_pick(sf::RenderWindow& window);
    bool is_item_pick_active() const;

    /** 根据玩家当前层数/持有情况推断展示等级（0~4） */
    static int item_display_level(int registry_index, const Player& player);

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
    sf::Texture battle_anim_frames_[6];
    std::unordered_map<int, sf::Texture> item_icon_by_registry_;
    std::unordered_map<int, sf::Texture> portrait_by_character_;
    std::array<sf::Texture, static_cast<size_t>(TearTextureId::Count)> tear_textures_{};
    std::array<bool, static_cast<size_t>(TearTextureId::Count)> tear_texture_ok_{};

    bool chest_closed_tex_ok_ = false;
    bool chest_open_tex_ok_   = false;
    bool flow_tex_loaded_    = false;
    bool room_tex_loaded_    = false;
    bool battle_anim_loaded_ = false;

    int random_room_for_floor6_ = 1;
    /** -1 = 跟随 floor_level；1~5 = 强制 room-N */
    int debug_room_override_ = -1;
    /** 为 true 时忽略 Enter 确认，直到按键抬起（避免 MENU→选角 同帧连触） */
    bool char_select_block_confirm_until_key_up_ = false;

    bool load_font();
    bool load_flow_textures();
    bool load_chest_textures();
    void ensure_chest_textures_loaded();
    void sync_chest_hitbox_from_texture(const sf::Texture& tex);
    const sf::Texture* chest_texture_for_draw() const;
    bool test_chest_player_overlap(const sf::Vector2f& player_pos) const;
    bool load_room_textures();
    bool load_battle_anim_frames();
    bool load_tear_textures();
    void ensure_tear_textures_loaded();
    bool load_item_icon(int registry_index);
    bool load_character_portrait(CharacterId id, const char* relative_path);
    bool load_flow_screen_texture(sf::Texture& tex, const char* ascii_file);

    const sf::Font& font() const;
    void draw_sprite_fit(sf::RenderWindow& window,
                         const sf::Texture& tex,
                         const sf::FloatRect& target,
                         bool preserve_aspect = true) const;
    void draw_sprite_cover(sf::RenderWindow& window, const sf::Texture& tex) const;
    void draw_flow_fullscreen(sf::RenderWindow& window, const sf::Texture& tex) const;
    void draw_character_stat_pips(sf::RenderWindow& window,
                                  const CharacterStatDisplay& stats) const;
    void draw_character_portrait(sf::RenderWindow& window,
                                 const sf::Texture& portrait) const;
    void draw_sprite_cover_rotated_90(sf::RenderWindow& window,
                                      const sf::Texture& tex) const;
    void update_debug_room_input();
    void draw_debug_room_hint(sf::RenderWindow& window) const;
    int resolve_room_index(int floor_level) const;
    void draw_level_badge(sf::RenderWindow& window,
                          float x,
                          float y,
                          int level) const;
    void draw_item_pick_option(sf::RenderWindow& window,
                               const sf::FloatRect& card_rect,
                               const ItemPickOption& opt) const;
    void draw_parchment_card(sf::RenderWindow& window,
                             const sf::FloatRect& rect,
                             const sf::Color& accent) const;

    static const char* item_icon_path(int registry_index);
};

#endif
