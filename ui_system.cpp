#include "ui_system.h"
#include "character_roster.h"
#include "player_stats.h"
#include "attack_profile.h"
#include "item_registry.h"
#include "tear_profile.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <cstdio>
#include <iostream>
#include <vector>

namespace {

constexpr float k_screen_center_x = 400.f;
constexpr float k_screen_h        = 900.f;
/** 碰撞后展示打开宝箱贴图的时长（秒） */
constexpr float k_chest_open_reveal_duration = 0.75f;
/** 战斗中玩家精灵目标边长（像素）；仅视觉，不影响碰撞盒 */
constexpr float k_battle_player_sprite_px = 88.f;

void center_text_origin(sf::Text& text, bool center_y = false) {
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin(bounds.width / 2.f, center_y ? bounds.height / 2.f : 0.f);
}

sf::Color level_badge_color(int level) {
    switch (std::clamp(level, 0, 4)) {
    case 0: return sf::Color(120, 120, 120);
    case 1: return sf::Color(70, 130, 220);
    case 2: return sf::Color(70, 190, 90);
    case 3: return sf::Color(170, 90, 210);
    default: return sf::Color(230, 190, 60);
    }
}

bool key_just_pressed(sf::Keyboard::Key key) {
    static bool prev[sf::Keyboard::KeyCount] = {};
    const bool now = sf::Keyboard::isKeyPressed(key);
    const bool edge = now && !prev[key];
    prev[key] = now;
    return edge;
}

bool ctrl_held() {
    return sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) ||
           sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
}

/** 相对 cwd 向上查找 gfx：./ → ../ → ../../ */
void append_gfx_asset_paths(std::vector<std::string>& out, const char* relative) {
    if (!relative || relative[0] == '\0') {
        return;
    }
    out.emplace_back(relative);
    out.emplace_back(std::string("../") + relative);
    out.emplace_back(std::string("../../") + relative);
}

bool try_load_texture_paths(sf::Texture& tex, const std::vector<std::string>& paths) {
    for (const std::string& path : paths) {
        if (!path.empty() && tex.loadFromFile(path)) {
            return true;
        }
    }
    return false;
}

bool load_gfx_texture(sf::Texture& tex, const char* relative, bool smooth = true) {
    std::vector<std::string> paths;
    append_gfx_asset_paths(paths, relative);
    if (!try_load_texture_paths(tex, paths)) {
        return false;
    }
    tex.setSmooth(smooth);
    return true;
}

void append_gfx_path(std::vector<std::string>& out, const char* relative) {
    append_gfx_asset_paths(out, relative);
}

void configure_chest_texture(sf::Texture& tex) {
    tex.setSmooth(false);
}

bool load_chest_texture_file(sf::Texture& tex, const char* ascii_name) {
    const std::string rel = std::string("gfx/ui/") + ascii_name;
    if (!load_gfx_texture(tex, rel.c_str(), false)) {
        return false;
    }
    configure_chest_texture(tex);
    return true;
}

bool rects_overlap(const sf::FloatRect& a, const sf::FloatRect& b) {
    return a.left < b.left + b.width &&
           a.left + a.width > b.left &&
           a.top < b.top + b.height &&
           a.top + a.height > b.top;
}

} // namespace

int UISystem::item_display_level(int registry_index, const Player& player) {
    switch (registry_index) {
    case 0: return std::min(4, player.stats.tracking_level);
    case 1: return std::min(4, player.stats.brimstone_level);
    case 2: return std::min(4, player.stats.extra_bullets);
    case 3: return player.stats.has_parasite ? 1 : 0;
    case 4: return player.stats.has_haemolacria ? 1 : 0;
    default: return 0;
    }
}

bool UISystem::load_font() {
    if (font_.loadFromFile("C:/Windows/Fonts/msyh.ttc")) return true;
    if (font_.loadFromFile("C:/Windows/Fonts/msyhbd.ttc")) return true;
    if (font_.loadFromFile("C:/Windows/Fonts/simhei.ttf")) return true;
    if (font_.loadFromFile("C:/Windows/Fonts/simsun.ttc")) return true;
    return font_.loadFromFile("C:/Windows/Fonts/arial.ttf");
}

void UISystem::sync_chest_hitbox_from_texture(const sf::Texture& tex) {
    const sf::Vector2u sz = tex.getSize();
    if (sz.x == 0 || sz.y == 0) {
        return;
    }
    constexpr float k_display_px = 56.f;
    const float scale =
        k_display_px / static_cast<float>(std::max(1u, std::max(sz.x, sz.y)));
    chest_.half_w = static_cast<float>(sz.x) * scale * 0.5f;
    chest_.half_h = static_cast<float>(sz.y) * scale * 0.5f;
}

bool UISystem::load_chest_textures() {
    chest_closed_tex_ok_ = load_chest_texture_file(tex_chest_closed_, "chest_closed.png");
    chest_open_tex_ok_   = load_chest_texture_file(tex_chest_open_, "chest_open.png");

    if (chest_closed_tex_ok_) {
        sync_chest_hitbox_from_texture(tex_chest_closed_);
    } else if (chest_open_tex_ok_) {
        sync_chest_hitbox_from_texture(tex_chest_open_);
    }
    return chest_closed_tex_ok_ || chest_open_tex_ok_;
}

void UISystem::ensure_chest_textures_loaded() {
    if (!chest_closed_tex_ok_) {
        chest_closed_tex_ok_ = load_chest_texture_file(tex_chest_closed_, "chest_closed.png");
    }
    if (!chest_open_tex_ok_) {
        chest_open_tex_ok_ = load_chest_texture_file(tex_chest_open_, "chest_open.png");
    }
    if (chest_closed_tex_ok_) {
        sync_chest_hitbox_from_texture(tex_chest_closed_);
    } else if (chest_open_tex_ok_) {
        sync_chest_hitbox_from_texture(tex_chest_open_);
    }
}

const sf::Texture* UISystem::chest_texture_for_draw() const {
    if (chest_.opened) {
        if (chest_open_tex_ok_) {
            return &tex_chest_open_;
        }
        if (chest_closed_tex_ok_) {
            return &tex_chest_closed_;
        }
        return nullptr;
    }
    if (chest_closed_tex_ok_) {
        return &tex_chest_closed_;
    }
    if (chest_open_tex_ok_) {
        return &tex_chest_open_;
    }
    return nullptr;
}

bool UISystem::load_flow_screen_texture(sf::Texture& tex, const char* ascii_file) {
    const std::string rel = std::string("gfx/ui/") + ascii_file;
    return load_gfx_texture(tex, rel.c_str(), true);
}

bool UISystem::load_flow_textures() {
    const bool waiting_ok = load_flow_screen_texture(tex_waiting_, "waiting_screen.png");
    const bool select_ok =
        load_flow_screen_texture(tex_char_select_, "character_select_screen.png");
    flow_tex_loaded_ = waiting_ok && select_ok;
    return flow_tex_loaded_;
}

bool UISystem::load_character_portrait(CharacterId id, const char* relative_path) {
    const int key = static_cast<int>(id);
    if (portrait_by_character_.count(key) != 0) {
        return true;
    }

    std::vector<std::string> paths;
    if (id == CharacterId::Isaac) {
        append_gfx_path(paths, "gfx/ui/isaac_portrait.png");
    } else {
        append_gfx_path(paths, relative_path);
    }

    sf::Texture tex;
    if (!try_load_texture_paths(tex, paths)) {
        return false;
    }
    tex.setSmooth(false);
    portrait_by_character_[key] = std::move(tex);
    return true;
}

bool UISystem::load_room_textures() {
    room_tex_loaded_ = true;
    for (int i = 0; i < 5; ++i) {
        const std::string path =
            std::string("gfx/rooms/room-") + std::to_string(i + 1) + ".png";
        if (!load_gfx_texture(room_textures_[i], path.c_str(), false)) {
            room_tex_loaded_ = false;
        }
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    random_room_for_floor6_ = std::uniform_int_distribution<int>(1, 5)(gen);
    return room_tex_loaded_;
}

bool UISystem::load_battle_anim_frames() {
    const GameCharacter& ch = CharacterRoster::instance().selected();
    const int frame_count = std::max(1, ch.battle_frame_count());
    battle_anim_loaded_ = true;

    for (int i = 0; i < frame_count && i < 6; ++i) {
        char path_buf[128] = {};
        std::snprintf(path_buf, sizeof(path_buf), ch.battle_anim_pattern(), i);

        std::vector<std::string> paths;
        append_gfx_path(paths, path_buf);
        if (!try_load_texture_paths(battle_anim_frames_[i], paths)) {
            battle_anim_loaded_ = false;
        }
    }
    return battle_anim_loaded_;
}

bool UISystem::load_item_icon(int registry_index) {
    if (item_icon_by_registry_.count(registry_index)) {
        return true;
    }
    sf::Texture tex;
    std::vector<std::string> paths;
    append_gfx_path(paths, ItemRegistry::iconPath(registry_index));
    if (!try_load_texture_paths(tex, paths)) {
        return false;
    }
    tex.setSmooth(true);
    item_icon_by_registry_[registry_index] = std::move(tex);
    return true;
}

void UISystem::reset_run_state() {
    chest_.active = false;
    chest_.opened = false;
    chest_.opening_reveal_timer = 0.f;
    chest_.vy = 0.f;
    pick_options_.clear();
    char_select_block_confirm_until_key_up_ = false;
}

void UISystem::notify_enter_character_select_screen() {
    char_select_block_confirm_until_key_up_ = true;
}

bool UISystem::load_tear_textures() {
    bool any_ok = false;
    for (int i = 0; i < static_cast<int>(TearTextureId::Count); ++i) {
        const auto id = static_cast<TearTextureId>(i);
        std::vector<std::string> paths;
        append_gfx_path(paths, tear_texture_asset_path(id));
        tear_texture_ok_[static_cast<size_t>(i)] =
            try_load_texture_paths(tear_textures_[static_cast<size_t>(i)], paths);
        if (tear_texture_ok_[static_cast<size_t>(i)]) {
            tear_textures_[static_cast<size_t>(i)].setSmooth(true);
            any_ok = true;
        }
    }
    return any_ok;
}

void UISystem::ensure_tear_textures_loaded() {
    if (std::none_of(
            tear_texture_ok_.begin(),
            tear_texture_ok_.end(),
            [](bool ok) { return ok; })) {
        load_tear_textures();
    }
}

void UISystem::draw_player_tears(sf::RenderWindow& window,
                                 const Player& player,
                                 const std::vector<Bullet>& tears)
{
    ensure_tear_textures_loaded();

    for (const Bullet& t : tears) {
        if (t.is_dead || t.is_haemolacria_orb) {
            continue;
        }
        if (t.module3_apple_razor || t.module3_betrayal_tear
            || t.module3_godhead || t.module3_glass) {
            continue;
        }

        const TearTextureId tex_id = resolve_player_tear_texture(player, t);
        const size_t slot = static_cast<size_t>(tex_id);
        const float scale_mul = std::max(0.35f, t.visual_scale);
        const float diameter = t.radius * 2.f * scale_mul;

        if (slot < tear_texture_ok_.size() && tear_texture_ok_[slot]) {
            const sf::Texture& tex = tear_textures_[slot];
            sf::Sprite sprite(tex);
            const sf::Vector2u sz = tex.getSize();
            if (sz.x == 0 || sz.y == 0) {
                continue;
            }
            const float s =
                diameter / static_cast<float>(std::max(1u, std::max(sz.x, sz.y)));
            sprite.setScale(s, s);
            sprite.setOrigin(
                static_cast<float>(sz.x) * 0.5f,
                static_cast<float>(sz.y) * 0.5f);
            sprite.setPosition(t.x, t.y);
            sprite.setColor(t.bullet_color);
            window.draw(sprite);
            continue;
        }

        sf::CircleShape fallback(t.radius * scale_mul);
        fallback.setOrigin(fallback.getRadius(), fallback.getRadius());
        fallback.setPosition(t.x, t.y);
        fallback.setFillColor(t.bullet_color);
        window.draw(fallback);
    }
}

bool UISystem::initialize() {
    reset_run_state();
    CharacterRoster::instance().initialize();

    font_loaded_ = load_font();
    load_chest_textures();
    load_flow_textures();
    load_room_textures();
    load_battle_anim_frames();
    load_tear_textures();

    const GameCharacter& selected = CharacterRoster::instance().selected();
    load_character_portrait(selected.id(), selected.select_portrait_path());

    for (int i = 0; i < ItemRegistry::itemCount(); ++i) {
        load_item_icon(i);
    }
    return font_loaded_;
}

void UISystem::draw_debug_toast(sf::RenderWindow& window,
                                const std::wstring& message) const
{
    sf::RectangleShape bar(sf::Vector2f(780.f, 32.f));
    bar.setPosition(10.f, 48.f);
    bar.setFillColor(sf::Color(0, 80, 160, 230));
    bar.setOutlineColor(sf::Color(100, 220, 255));
    bar.setOutlineThickness(2.f);
    window.draw(bar);

    if (!font_loaded_ || message.empty()) {
        sf::RectangleShape ok(sf::Vector2f(120.f, 12.f));
        ok.setPosition(340.f, 58.f);
        ok.setFillColor(sf::Color(80, 255, 120));
        window.draw(ok);
        return;
    }

    sf::Text text(message, font_, 18);
    text.setFillColor(sf::Color(230, 250, 255));
    text.setOutlineColor(sf::Color(10, 40, 80));
    text.setOutlineThickness(1.f);
    center_text_origin(text, false);
    text.setPosition(k_screen_center_x, 52.f);
    window.draw(text);
}

const sf::Font& UISystem::font() const {
    return font_;
}

void UISystem::draw_sprite_fit(sf::RenderWindow& window,
                               const sf::Texture& tex,
                               const sf::FloatRect& target,
                               bool preserve_aspect) const
{
    sf::Sprite sprite(tex);
    const sf::Vector2u sz = tex.getSize();
    if (sz.x == 0 || sz.y == 0) {
        return;
    }

    float scale_x = target.width / static_cast<float>(sz.x);
    float scale_y = target.height / static_cast<float>(sz.y);
    if (preserve_aspect) {
        const float s = std::min(scale_x, scale_y);
        scale_x = s;
        scale_y = s;
    }

    sprite.setScale(scale_x, scale_y);
    sprite.setPosition(
        target.left + (target.width - static_cast<float>(sz.x) * scale_x) * 0.5f,
        target.top + (target.height - static_cast<float>(sz.y) * scale_y) * 0.5f);
    window.draw(sprite);
}

void UISystem::draw_sprite_cover(sf::RenderWindow& window, const sf::Texture& tex) const
{
    const sf::Vector2u win = window.getSize();
    const float win_w = static_cast<float>(win.x);
    const float win_h = static_cast<float>(win.y);
    const sf::Vector2u sz = tex.getSize();
    if (sz.x == 0 || sz.y == 0 || win.x == 0 || win.y == 0) {
        return;
    }

    const float tex_w = static_cast<float>(sz.x);
    const float tex_h = static_cast<float>(sz.y);
    const float scale = std::max(win_w / tex_w, win_h / tex_h);

    sf::Sprite sprite(tex);
    sprite.setOrigin(tex_w * 0.5f, tex_h * 0.5f);
    sprite.setPosition(win_w * 0.5f, win_h * 0.5f);
    sprite.setScale(scale, scale);
    window.draw(sprite);
}

void UISystem::draw_flow_fullscreen(sf::RenderWindow& window, const sf::Texture& tex) const
{
    window.clear(sf::Color(18, 14, 22));
    draw_sprite_cover(window, tex);
}

void UISystem::draw_sprite_cover_rotated_90(sf::RenderWindow& window,
                                            const sf::Texture& tex) const
{
    const sf::Vector2u win = window.getSize();
    const float win_w = static_cast<float>(win.x);
    const float win_h = static_cast<float>(win.y);
    const sf::Vector2u sz = tex.getSize();
    if (sz.x == 0 || sz.y == 0 || win.x == 0 || win.y == 0) {
        return;
    }

    const float tex_w = static_cast<float>(sz.x);
    const float tex_h = static_cast<float>(sz.y);
    // 旋转 90° 后轴对齐包围盒为 (tex_h, tex_w)，cover 取较大缩放比
    const float scale = std::max(win_w / tex_h, win_h / tex_w);

    sf::Sprite sprite(tex);
    sprite.setOrigin(tex_w * 0.5f, tex_h * 0.5f);
    sprite.setPosition(win_w * 0.5f, win_h * 0.5f);
    sprite.setRotation(90.f);
    sprite.setScale(scale, scale);
    window.draw(sprite);
}

void UISystem::draw_level_badge(sf::RenderWindow& window,
                                float x,
                                float y,
                                int level) const
{
    constexpr float k_unit = 4.f;
    const float w = 3.f * k_unit;
    const float h = 4.f * k_unit;

    sf::RectangleShape bg(sf::Vector2f(w, h));
    bg.setFillColor(level_badge_color(level));
    bg.setOutlineThickness(2.f);
    bg.setOutlineColor(sf::Color(30, 25, 20));
    bg.setPosition(x, y);
    window.draw(bg);

    if (!font_loaded_) {
        return;
    }

    const int clamped = std::clamp(level, 0, 4);
    sf::Text num(std::to_wstring(clamped), font_, 16);
    num.setFillColor(sf::Color::White);
    num.setStyle(sf::Text::Bold);
    const sf::FloatRect tb = num.getLocalBounds();
    num.setOrigin(tb.width / 2.f, tb.height / 2.f);
    num.setPosition(x + w * 0.5f, y + h * 0.5f);
    window.draw(num);
}

void UISystem::draw_parchment_card(sf::RenderWindow& window,
                                   const sf::FloatRect& rect,
                                   const sf::Color& accent) const
{
    sf::RectangleShape body(sf::Vector2f(rect.width, rect.height));
    body.setPosition(rect.left, rect.top);
    body.setFillColor(sf::Color(245, 230, 200));
    body.setOutlineThickness(6.f);
    body.setOutlineColor(sf::Color(45, 30, 20));
    window.draw(body);

    sf::RectangleShape inner(sf::Vector2f(rect.width - 14.f, rect.height - 14.f));
    inner.setPosition(rect.left + 7.f, rect.top + 7.f);
    inner.setFillColor(sf::Color(0, 0, 0, 0));
    inner.setOutlineThickness(3.f);
    inner.setOutlineColor(accent);
    window.draw(inner);
}

namespace {

constexpr float k_char_portrait_x       = 400.f;
constexpr float k_char_portrait_y       = 388.f;
constexpr float k_char_portrait_display = 92.f;
constexpr float k_char_stat_col_x       = 332.f;
constexpr float k_char_stat_health_y    = 498.f;
constexpr float k_char_stat_speed_y     = 538.f;
constexpr float k_char_stat_damage_y    = 578.f;

} // namespace

void UISystem::draw_character_portrait(sf::RenderWindow& window,
                                      const sf::Texture& portrait) const
{
    sf::Sprite sprite(portrait);
    const sf::Vector2u sz = portrait.getSize();
    const float scale =
        k_char_portrait_display /
        static_cast<float>(std::max(1u, std::max(sz.x, sz.y)));
    sprite.setScale(scale, scale);
    sprite.setOrigin(static_cast<float>(sz.x) * 0.5f, static_cast<float>(sz.y) * 0.5f);
    sprite.setPosition(k_char_portrait_x, k_char_portrait_y);
    window.draw(sprite);
}

void UISystem::draw_character_stat_pips(sf::RenderWindow& window,
                                        const CharacterStatDisplay& stats) const
{
    auto draw_row = [&](float row_y, int count) {
        for (int i = 0; i < count; ++i) {
            sf::RectangleShape pip(sf::Vector2f(4.f, 14.f));
            pip.setFillColor(sf::Color(25, 20, 18));
            pip.setOutlineThickness(1.f);
            pip.setOutlineColor(sf::Color(10, 8, 6));
            pip.setPosition(k_char_stat_col_x, row_y + static_cast<float>(i) * 16.f);
            window.draw(pip);
        }
    };

    draw_row(k_char_stat_health_y, stats.health_pips);
    draw_row(k_char_stat_speed_y, stats.speed_pips);
    draw_row(k_char_stat_damage_y, stats.damage_pips);
}

// ==================== 等待 / 选角 ====================
void UISystem::draw_waiting_screen(sf::RenderWindow& window) {
    if (!flow_tex_loaded_) {
        load_flow_textures();
    }

    if (flow_tex_loaded_) {
        draw_flow_fullscreen(window, tex_waiting_);
    } else {
        window.clear(sf::Color(25, 15, 35));
        if (font_loaded_) {
            sf::Text hint(L"缺少 gfx/ui/waiting_screen.png", font_, 20);
            hint.setFillColor(sf::Color::White);
            center_text_origin(hint);
            hint.setPosition(k_screen_center_x, k_screen_h * 0.5f);
            window.draw(hint);
        }
    }
}

void UISystem::draw_character_select(sf::RenderWindow& window) {
    if (!flow_tex_loaded_) {
        load_flow_textures();
    }

    if (flow_tex_loaded_) {
        draw_flow_fullscreen(window, tex_char_select_);
    } else {
        window.clear(sf::Color(30, 20, 40));
    }

    CharacterRoster& roster = CharacterRoster::instance();
    const GameCharacter& ch = roster.selected();
    load_character_portrait(ch.id(), ch.select_portrait_path());

    const auto portrait_it = portrait_by_character_.find(static_cast<int>(ch.id()));
    if (portrait_it != portrait_by_character_.end()) {
        draw_character_portrait(window, portrait_it->second);
    }

    draw_character_stat_pips(window, ch.stat_display());
}

int UISystem::update_character_select(sf::RenderWindow& window) {
    CharacterRoster& roster = CharacterRoster::instance();

    if (char_select_block_confirm_until_key_up_) {
        const bool enter_held =
            sf::Keyboard::isKeyPressed(sf::Keyboard::Enter) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Return);
        if (!enter_held) {
            char_select_block_confirm_until_key_up_ = false;
        }
    }

    if (key_just_pressed(sf::Keyboard::Left)) {
        roster.cycle(-1);
        const GameCharacter& ch = roster.selected();
        load_character_portrait(ch.id(), ch.select_portrait_path());
    }
    if (key_just_pressed(sf::Keyboard::Right)) {
        roster.cycle(1);
        const GameCharacter& ch = roster.selected();
        load_character_portrait(ch.id(), ch.select_portrait_path());
    }

    const bool confirm_key =
        !char_select_block_confirm_until_key_up_ &&
        (key_just_pressed(sf::Keyboard::Enter) ||
         key_just_pressed(sf::Keyboard::Return));

    const sf::Vector2i mp = sf::Mouse::getPosition(window);
    const sf::FloatRect paper_sheet(210.f, 110.f, 380.f, 540.f);
    static bool prev_mouse_left = false;
    const bool mouse_left = sf::Mouse::isButtonPressed(sf::Mouse::Left);
    const bool confirm_click =
        mouse_left && !prev_mouse_left &&
        paper_sheet.contains(static_cast<float>(mp.x), static_cast<float>(mp.y));
    prev_mouse_left = mouse_left;

    if (confirm_key || confirm_click) {
        return static_cast<int>(roster.selected_id());
    }
    return -1;
}

void UISystem::draw_main_menu(sf::RenderWindow& window) {
    draw_waiting_screen(window);
}

// ==================== 背景 & Isaac ====================
int UISystem::debug_room_override() const {
    return debug_room_override_;
}

int UISystem::resolve_room_index(int floor_level) const {
    if (debug_room_override_ >= 1 && debug_room_override_ <= 5) {
        return debug_room_override_;
    }

    int room_index = floor_level;
    if (room_index < 1) {
        room_index = 1;
    } else if (room_index > 5) {
        room_index = random_room_for_floor6_;
    }
    return room_index;
}

void UISystem::update_debug_room_input() {
    auto set_room = [this](int room) {
        debug_room_override_ = std::clamp(room, 1, 5);
    };

    if (key_just_pressed(sf::Keyboard::F1)) set_room(1);
    if (key_just_pressed(sf::Keyboard::F2)) set_room(2);
    if (key_just_pressed(sf::Keyboard::F3)) set_room(3);
    if (key_just_pressed(sf::Keyboard::F4)) set_room(4);
    if (key_just_pressed(sf::Keyboard::F5)) set_room(5);
    if (key_just_pressed(sf::Keyboard::F6)) {
        debug_room_override_ = -1;
    }

    const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) ||
                       sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
    if (shift) {
        if (key_just_pressed(sf::Keyboard::Num1) ||
            key_just_pressed(sf::Keyboard::Numpad1)) {
            set_room(1);
        }
        if (key_just_pressed(sf::Keyboard::Num2) ||
            key_just_pressed(sf::Keyboard::Numpad2)) {
            set_room(2);
        }
        if (key_just_pressed(sf::Keyboard::Num3) ||
            key_just_pressed(sf::Keyboard::Numpad3)) {
            set_room(3);
        }
        if (key_just_pressed(sf::Keyboard::Num4) ||
            key_just_pressed(sf::Keyboard::Numpad4)) {
            set_room(4);
        }
        if (key_just_pressed(sf::Keyboard::Num5) ||
            key_just_pressed(sf::Keyboard::Numpad5)) {
            set_room(5);
        }
    }

    const bool page_up = key_just_pressed(sf::Keyboard::PageUp);
    const bool page_down = key_just_pressed(sf::Keyboard::PageDown);
    const bool ctrl = ctrl_held();
    const bool room_left =
        page_down ||
        (ctrl && key_just_pressed(sf::Keyboard::Left));
    const bool room_right =
        page_up ||
        (ctrl && key_just_pressed(sf::Keyboard::Right));

    if (room_left || room_right) {
        int current = debug_room_override_;
        if (current < 1 || current > 5) {
            current = 1;
        }
        if (room_right) {
            current = (current % 5) + 1;
        } else {
            current = (current - 2 + 5) % 5 + 1;
        }
        debug_room_override_ = current;
    }
}

void UISystem::draw_debug_room_hint(sf::RenderWindow& window) const {
    if (!font_loaded_ || debug_room_override_ < 1) {
        return;
    }

    const sf::Vector2u win = window.getSize();
    const std::wstring label =
        L"[验收] room-" + std::to_wstring(debug_room_override_) +
        L"  F1-5 / Shift+1-5 直选 PageUp/Dn F6自动";

    sf::Text hint(label, font_, 14);
    hint.setFillColor(sf::Color(255, 255, 200, 220));
    hint.setOutlineColor(sf::Color(20, 10, 30));
    hint.setOutlineThickness(2.f);
    hint.setPosition(8.f, static_cast<float>(win.y) - 28.f);
    window.draw(hint);
}

void UISystem::draw_room_background(sf::RenderWindow& window, int floor_level) {
    update_debug_room_input();

    const int room_index = resolve_room_index(floor_level);

    if (!room_tex_loaded_) {
        window.clear(sf::Color(20, 10, 30));
        return;
    }

    window.clear(sf::Color(20, 10, 30));
    const sf::Texture& tex = room_textures_[room_index - 1];
    draw_sprite_cover_rotated_90(window, tex);
    draw_debug_room_hint(window);
}

void UISystem::draw_isaac_player(sf::RenderWindow& window,
                                 float x,
                                 float y,
                                 float anim_time_sec)
{
    const float fallback_r = k_battle_player_sprite_px * 0.32f;

    if (!battle_anim_loaded_) {
        sf::CircleShape fallback(fallback_r);
        fallback.setOrigin(fallback_r, fallback_r);
        fallback.setPosition(x, y);
        fallback.setFillColor(sf::Color(220, 200, 180));
        window.draw(fallback);
        return;
    }

    const GameCharacter& ch = CharacterRoster::instance().selected();
    const int frame_count = std::max(1, ch.battle_frame_count());
    const int frame =
        static_cast<int>(anim_time_sec * 10.f) % std::min(6, frame_count);
    const sf::Texture& tex = battle_anim_frames_[frame];
    sf::Sprite sprite(tex);
    const sf::Vector2u sz = tex.getSize();
    const float scale =
        k_battle_player_sprite_px
        / static_cast<float>(std::max(1u, std::max(sz.x, sz.y)));
    sprite.setScale(scale, scale);
    sprite.setOrigin(static_cast<float>(sz.x) * 0.5f, static_cast<float>(sz.y) * 0.5f);
    sprite.setPosition(x, y);
    sprite.setColor(sf::Color::White);
    window.draw(sprite);
}

// ==================== 宝箱 ====================
void UISystem::spawn_item_chest(float center_x) {
    ensure_chest_textures_loaded();
    chest_.active = true;
    chest_.opened = false;
    chest_.opening_reveal_timer = 0.f;
    chest_.x = center_x;
    chest_.y = -56.f;
    chest_.vy = 28.f;
    std::cout << "[Chest] spawn at (" << chest_.x << ", " << chest_.y
              << ") closed_tex=" << (chest_closed_tex_ok_ ? "ok" : "fail")
              << " open_tex=" << (chest_open_tex_ok_ ? "ok" : "fail") << "\n";
}

bool UISystem::test_chest_player_overlap(const sf::Vector2f& player_pos) const {
    // 与 main.cpp 机体碰撞盒一致，并略放大便于接住下落宝箱
    const sf::FloatRect player_rect(
        player_pos.x - 22.f,
        player_pos.y - 26.f,
        44.f,
        52.f);

    sf::FloatRect chest_rect = item_chest_bounds();
    constexpr float k_pad = 18.f;
    chest_rect.left -= k_pad;
    chest_rect.top -= k_pad;
    chest_rect.width += k_pad * 2.f;
    chest_rect.height += k_pad * 2.f;

    return rects_overlap(player_rect, chest_rect);
}

ChestUpdateResult UISystem::update_item_chest(float dt,
                                              const sf::Vector2f& player_pos,
                                              float player_radius)
{
    (void)player_radius;

    if (!chest_.active) {
        return ChestUpdateResult::None;
    }

    if (chest_.opened) {
        if (chest_.opening_reveal_timer > 0.f) {
            chest_.opening_reveal_timer -= dt;
            if (chest_.opening_reveal_timer < 0.f) {
                chest_.opening_reveal_timer = 0.f;
            }
            return ChestUpdateResult::None;
        }
        return ChestUpdateResult::ReadyForItemPick;
    }

    auto try_start_open_reveal = [&]() -> bool {
        if (!test_chest_player_overlap(player_pos)) {
            return false;
        }
        chest_.opened = true;
        chest_.vy = 0.f;
        chest_.opening_reveal_timer = k_chest_open_reveal_duration;
        std::cout << "[Chest] collision -> opening reveal "
                  << k_chest_open_reveal_duration << "s, open_tex="
                  << (chest_open_tex_ok_ ? "loaded" : "MISSING") << "\n";
        return true;
    };

    if (try_start_open_reveal()) {
        return ChestUpdateResult::None;
    }

    chest_.y += chest_.vy * dt;

    if (try_start_open_reveal()) {
        return ChestUpdateResult::None;
    }

    if (chest_.y > k_screen_h + 80.f) {
        chest_.active = false;
        chest_.vy = 0.f;
        std::cout << "[Chest] missed (fell off screen), y=" << chest_.y << "\n";
    }
    return ChestUpdateResult::None;
}

bool UISystem::is_chest_opening_reveal() const {
    return chest_.active && chest_.opened && chest_.opening_reveal_timer > 0.f;
}

bool UISystem::is_chest_opened() const {
    return chest_.active && chest_.opened;
}

void UISystem::draw_item_chest(sf::RenderWindow& window) {
    if (!chest_.active) {
        return;
    }

    ensure_chest_textures_loaded();
    const sf::Texture* tex_ptr = chest_texture_for_draw();
    if (!tex_ptr) {
        return;
    }

    sf::Sprite sprite(*tex_ptr);
    const sf::Vector2u sz = tex_ptr->getSize();
    float display_px = 56.f;
    if (is_chest_opening_reveal()) {
        const float t = 1.f - chest_.opening_reveal_timer / k_chest_open_reveal_duration;
        display_px = 56.f + 10.f * std::sin(t * 3.14159265f);
    }
    const float scale =
        display_px / static_cast<float>(std::max(1u, std::max(sz.x, sz.y)));
    sprite.setScale(scale, scale);
    sprite.setOrigin(static_cast<float>(sz.x) * 0.5f, static_cast<float>(sz.y) * 0.5f);
    sprite.setPosition(chest_.x, chest_.y);
    window.draw(sprite);
}

bool UISystem::has_active_chest() const {
    return chest_.active;
}

sf::FloatRect UISystem::item_chest_bounds() const {
    return sf::FloatRect(
        chest_.x - chest_.half_w,
        chest_.y - chest_.half_h,
        chest_.half_w * 2.f,
        chest_.half_h * 2.f);
}

// ==================== 选道具面板 ====================
void UISystem::begin_item_pick(const std::vector<int>& registry_indices,
                               const Player& player)
{
    pick_options_.clear();

    std::vector<int> indices = registry_indices;
    if (indices.empty()) {
        const int count = ItemRegistry::itemCount();
        for (int i = 0; i < 3 && i < count; ++i) {
            indices.push_back(i);
        }
        std::cout << "[Chest] begin_item_pick: pending options empty, using fallback\n";
    }

    pick_options_.reserve(indices.size());
    for (int idx : indices) {
        ItemPickOption opt;
        opt.registry_index = idx;
        opt.item_level = item_display_level(idx, player);
        pick_options_.push_back(opt);
        load_item_icon(idx);
    }
}

bool UISystem::is_item_pick_active() const {
    return !pick_options_.empty();
}

namespace {

constexpr float k_pick_card_w     = 210.f;
constexpr float k_pick_card_h     = 300.f;
constexpr float k_pick_spacing    = 48.f;
constexpr float k_pick_start_y    = 170.f;
constexpr float k_badge_w         = 12.f;
constexpr float k_badge_h         = 16.f;

float item_pick_row_start_x(const sf::RenderWindow& window, size_t option_count) {
    const float count = static_cast<float>(std::max<size_t>(1, option_count));
    const float total_w = count * k_pick_card_w + (count - 1.f) * k_pick_spacing;
    return (static_cast<float>(window.getSize().x) - total_w) * 0.5f;
}

sf::FloatRect item_pick_card_rect(const sf::RenderWindow& window,
                                  size_t index,
                                  size_t option_count)
{
    const float start_x = item_pick_row_start_x(window, option_count);
    const float card_x = start_x + static_cast<float>(index) * (k_pick_card_w + k_pick_spacing);
    return sf::FloatRect(card_x, k_pick_start_y, k_pick_card_w, k_pick_card_h);
}

} // namespace

void UISystem::draw_item_pick_option(sf::RenderWindow& window,
                                     const sf::FloatRect& card_rect,
                                     const ItemPickOption& opt) const
{
    const ItemDisplay item = ItemRegistry::getDisplay(opt.registry_index);
    draw_parchment_card(window, card_rect, item.color);

    const auto tex_it = item_icon_by_registry_.find(opt.registry_index);
    if (tex_it != item_icon_by_registry_.end()) {
        draw_sprite_fit(
            window,
            tex_it->second,
            sf::FloatRect(
                card_rect.left + 24.f,
                card_rect.top + 28.f,
                card_rect.width - 48.f,
                130.f),
            true);
    }

    if (!font_loaded_) {
        return;
    }

    const float name_row_y = card_rect.top + 168.f;
    const float row_center_x = card_rect.left + card_rect.width * 0.5f;

    sf::Text name_text(item.name, font_, 20);
    name_text.setFillColor(sf::Color(40, 30, 20));
    name_text.setStyle(sf::Text::Bold);
    const sf::FloatRect name_bounds = name_text.getLocalBounds();
    const float name_w = name_bounds.width;
    const float badge_x = row_center_x - (k_badge_w + 6.f + name_w) * 0.5f;

    draw_level_badge(window, badge_x, name_row_y, opt.item_level);

    name_text.setOrigin(0.f, 0.f);
    name_text.setPosition(badge_x + k_badge_w + 8.f, name_row_y + 2.f);
    window.draw(name_text);

    sf::Text desc_text(item.description, font_, 14);
    desc_text.setFillColor(sf::Color(60, 50, 40));
    center_text_origin(desc_text);
    desc_text.setPosition(row_center_x, card_rect.top + 210.f);
    window.draw(desc_text);

    sf::Text hint(L"[点击选择]", font_, 14);
    hint.setFillColor(sf::Color(100, 80, 60));
    center_text_origin(hint);
    hint.setPosition(row_center_x, card_rect.top + card_rect.height - 32.f);
    window.draw(hint);
}

int UISystem::update_item_pick(sf::RenderWindow& window) {
    if (pick_options_.empty()) {
        return -1;
    }

    const sf::Vector2i mouse_pos = sf::Mouse::getPosition(window);
    if (!sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
        return -1;
    }

    for (size_t i = 0; i < pick_options_.size(); ++i) {
        const sf::FloatRect rect =
            item_pick_card_rect(window, i, pick_options_.size());
        if (rect.contains(static_cast<float>(mouse_pos.x),
                          static_cast<float>(mouse_pos.y))) {
            const int picked = pick_options_[i].registry_index;
            pick_options_.clear();
            chest_.active = false;
            chest_.opened = false;
            chest_.vy = 0.f;
            return picked;
        }
    }
    return -1;
}

void UISystem::draw_item_pick(sf::RenderWindow& window) {
    if (pick_options_.empty()) {
        return;
    }

    for (const ItemPickOption& opt : pick_options_) {
        load_item_icon(opt.registry_index);
    }

    sf::RectangleShape overlay(sf::Vector2f(window.getSize()));
    overlay.setFillColor(sf::Color(0, 0, 0, 200));
    window.draw(overlay);

    if (font_loaded_) {
        sf::Text title(L"选择一件道具", font_, 34);
        title.setFillColor(sf::Color(255, 240, 200));
        title.setOutlineColor(sf::Color(40, 25, 15));
        title.setOutlineThickness(2.f);
        center_text_origin(title);
        title.setPosition(
            static_cast<float>(window.getSize().x) * 0.5f, 72.f);
        window.draw(title);
    }

    for (size_t i = 0; i < pick_options_.size(); ++i) {
        const sf::FloatRect card_rect =
            item_pick_card_rect(window, i, pick_options_.size());
        draw_item_pick_option(window, card_rect, pick_options_[i]);
    }
}

// ==================== 原有 HUD / 结算 ====================
void UISystem::draw_player_hp_hearts(sf::RenderWindow& window,
                                     const Player& player,
                                     float x,
                                     float y)
{
    for (int i = 0; i < player.stats.max_hp; ++i) {
        sf::CircleShape heart(8.f);
        heart.setPosition(x + static_cast<float>(i) * 20.f, y);
        heart.setFillColor(i < static_cast<int>(player.stats.hp)
            ? sf::Color::Red
            : sf::Color(80, 20, 20));
        window.draw(heart);
    }
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

        std::wstring wave_msg = overlay.wave_pause_timer > 2.0f
            ? L"第 " + std::to_wstring(current_wave) + L" 层已清除！"
            : L"第 " + std::to_wstring(current_wave) + L" 层 - 一大波敌人来袭！";

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
    float level_progress = static_cast<float>(score - overlay.score_at_level_start);
    float progress = level_needed > 0.f ? level_progress / level_needed : 0.f;
    progress = std::clamp(progress, 0.f, 1.f);

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
    fill_ratio = std::clamp(fill_ratio, 0.f, 1.f);

    sf::RectangleShape exp_bar_fill(sf::Vector2f(200.f * fill_ratio, 10.f));
    exp_bar_fill.setPosition(300.f, 10.f);
    exp_bar_fill.setFillColor(sf::Color(255, 200, 0));
    window.draw(exp_bar_fill);
}
