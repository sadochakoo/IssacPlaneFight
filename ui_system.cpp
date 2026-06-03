#include "ui_system.h"
#include "player_stats.h"
#include "attack_profile.h"
#include "item_registry.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>

namespace {

constexpr float k_screen_center_x = 400.f;
constexpr float k_screen_h        = 900.f;

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

} // namespace

// ==================== 资源路径 ====================
const char* UISystem::item_icon_path(int registry_index) {
    static const char* k_paths[] = {
        u8"gfx/items/寻友护符.png",   // 0 魔术弯勺
        u8"gfx/items/糖心.png",       // 1 硫磺火
        u8"gfx/items/寻友护符.png",   // 2 20/20（双发视觉）
        u8"gfx/items/冰块宝宝.png",   // 3 寄生虫
        u8"gfx/items/背叛.png",       // 4 泪血症（血刃/爆裂视觉）
    };
    if (registry_index < 0 || registry_index >= 5) {
        return u8"gfx/items/糖心.png";
    }
    return k_paths[registry_index];
}

const CharacterVisualProfile& UISystem::profile_for(CharacterId id) {
    static const CharacterVisualProfile k_isaac = {
        CharacterId::Isaac,
        "Isaac",
        "gfx/player/%d.png",
        6
    };
    (void)id;
    return k_isaac;
}

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

bool UISystem::load_chest_textures() {
    chest_tex_loaded_ =
        tex_chest_closed_.loadFromFile(u8"gfx/ui/宝箱0.png") &&
        tex_chest_open_.loadFromFile(u8"gfx/ui/宝箱1.png");
    return chest_tex_loaded_;
}

bool UISystem::load_flow_textures() {
    flow_tex_loaded_ =
        tex_waiting_.loadFromFile(u8"gfx/ui/等待界面.jpg") &&
        tex_char_select_.loadFromFile(u8"gfx/ui/选择角色界面.jpg");
    return flow_tex_loaded_;
}

bool UISystem::load_room_textures() {
    room_tex_loaded_ = true;
    for (int i = 0; i < 5; ++i) {
        const std::string path =
            std::string(u8"gfx/rooms/room-") + std::to_string(i + 1) + ".png";
        if (!room_textures_[i].loadFromFile(path)) {
            room_tex_loaded_ = false;
        }
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    random_room_for_floor6_ = std::uniform_int_distribution<int>(1, 5)(gen);
    return room_tex_loaded_;
}

bool UISystem::load_isaac_frames() {
    isaac_tex_loaded_ = true;
    for (int i = 0; i < 6; ++i) {
        const std::string path = std::string(u8"gfx/player/") + std::to_string(i) + ".png";
        if (!isaac_frames_[i].loadFromFile(path)) {
            isaac_tex_loaded_ = false;
        }
    }
    return isaac_tex_loaded_;
}

bool UISystem::load_item_icon(int registry_index) {
    if (item_icon_by_registry_.count(registry_index)) {
        return true;
    }
    sf::Texture tex;
    if (!tex.loadFromFile(item_icon_path(registry_index))) {
        return false;
    }
    tex.setSmooth(true);
    item_icon_by_registry_[registry_index] = std::move(tex);
    return true;
}

bool UISystem::initialize() {
    font_loaded_ = load_font();
    load_chest_textures();
    load_flow_textures();
    load_room_textures();
    load_isaac_frames();
    for (int i = 0; i < ItemRegistry::itemCount(); ++i) {
        load_item_icon(i);
    }
    return font_loaded_;
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
    constexpr float k_scale = 3.f;
    const float w = 3.f * k_scale;
    const float h = 4.f * k_scale;

    sf::RectangleShape bg(sf::Vector2f(w, h));
    bg.setFillColor(level_badge_color(level));
    bg.setOutlineThickness(1.f);
    bg.setOutlineColor(sf::Color(30, 25, 20));
    bg.setPosition(x, y);
    window.draw(bg);

    if (!font_loaded_) {
        return;
    }

    sf::Text num(std::to_wstring(std::clamp(level, 0, 4)), font_, static_cast<unsigned>(8 * k_scale));
    num.setFillColor(sf::Color::White);
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

// ==================== 等待 / 选角 ====================
void UISystem::draw_waiting_screen(sf::RenderWindow& window) {
    if (flow_tex_loaded_) {
        draw_sprite_fit(window, tex_waiting_, sf::FloatRect(0.f, 0.f, 800.f, 900.f), true);
    } else {
        window.clear(sf::Color(25, 15, 35));
    }

    if (!font_loaded_) {
        return;
    }

    sf::Text hint(L"按 Enter 进入角色选择", font_, 24);
    hint.setFillColor(sf::Color(255, 240, 200));
    center_text_origin(hint);
    hint.setPosition(k_screen_center_x, 820.f);
    window.draw(hint);
}

void UISystem::draw_character_select(sf::RenderWindow& window) {
    if (flow_tex_loaded_) {
        draw_sprite_fit(window, tex_char_select_, sf::FloatRect(0.f, 0.f, 800.f, 900.f), true);
    } else {
        window.clear(sf::Color(30, 20, 40));
    }

    if (!font_loaded_) {
        return;
    }

    sf::Text title(L"选择角色", font_, 32);
    title.setFillColor(sf::Color(255, 230, 180));
    center_text_origin(title);
    title.setPosition(k_screen_center_x, 40.f);
    window.draw(title);

    sf::Text isaac(L"Isaac（默认）", font_, 22);
    isaac.setFillColor(sf::Color::White);
    center_text_origin(isaac);
    isaac.setPosition(k_screen_center_x, 760.f);
    window.draw(isaac);

    sf::Text hint(L"点击屏幕中央 或 按 Enter 确认", font_, 18);
    hint.setFillColor(sf::Color(200, 200, 200));
    center_text_origin(hint);
    hint.setPosition(k_screen_center_x, 820.f);
    window.draw(hint);
}

int UISystem::update_character_select(sf::RenderWindow& window) {
    const sf::Vector2f center(k_screen_center_x, k_screen_h * 0.5f);
    const sf::Vector2i mp = sf::Mouse::getPosition(window);
    const float dx = static_cast<float>(mp.x) - center.x;
    const float dy = static_cast<float>(mp.y) - center.y;
    const bool in_zone = (dx * dx + dy * dy) < 120.f * 120.f;

    if (sf::Mouse::isButtonPressed(sf::Mouse::Left) && in_zone) {
        return static_cast<int>(CharacterId::Isaac);
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
    if (!isaac_tex_loaded_) {
        sf::CircleShape fallback(14.f);
        fallback.setOrigin(14.f, 14.f);
        fallback.setPosition(x, y);
        fallback.setFillColor(sf::Color(220, 200, 180));
        window.draw(fallback);
        return;
    }

    const int frame = static_cast<int>(anim_time_sec * 10.f) % 6;
    const sf::Texture& tex = isaac_frames_[frame];
    sf::Sprite sprite(tex);
    const sf::Vector2u sz = tex.getSize();
    const float target = 48.f;
    const float scale = target / static_cast<float>(std::max(1u, std::max(sz.x, sz.y)));
    sprite.setScale(scale, scale);
    sprite.setOrigin(static_cast<float>(sz.x) * 0.5f, static_cast<float>(sz.y) * 0.5f);
    sprite.setPosition(x, y);
    window.draw(sprite);
}

// ==================== 宝箱 ====================
void UISystem::spawn_item_chest(float center_x) {
    chest_.active = true;
    chest_.opened = false;
    chest_.x = center_x;
    chest_.y = -72.f;
    chest_.vy = 46.f;
}

bool UISystem::update_item_chest(float dt,
                                const sf::Vector2f& player_pos,
                                float player_radius)
{
    if (!chest_.active || chest_.opened) {
        return false;
    }

    chest_.y += chest_.vy * dt;
    if (chest_.y > k_screen_h + 40.f) {
        chest_.active = false;
        return false;
    }

    const float dx = player_pos.x - chest_.x;
    const float dy = player_pos.y - chest_.y;
    const float touch_r = player_radius + std::max(chest_.half_w, chest_.half_h);
    if (dx * dx + dy * dy <= touch_r * touch_r) {
        chest_.opened = true;
        return true;
    }
    return false;
}

void UISystem::draw_item_chest(sf::RenderWindow& window) {
    if (!chest_.active) {
        return;
    }

    if (chest_tex_loaded_) {
        const sf::Texture& tex = chest_.opened ? tex_chest_open_ : tex_chest_closed_;
        sf::Sprite sprite(tex);
        const sf::Vector2u sz = tex.getSize();
        const float scale = 48.f / static_cast<float>(std::max(1u, std::max(sz.x, sz.y)));
        sprite.setScale(scale, scale);
        sprite.setOrigin(static_cast<float>(sz.x) * 0.5f, static_cast<float>(sz.y) * 0.5f);
        sprite.setPosition(chest_.x, chest_.y);
        window.draw(sprite);
        return;
    }

    sf::RectangleShape box(sf::Vector2f(chest_.half_w * 2.f, chest_.half_h * 2.f));
    box.setOrigin(chest_.half_w, chest_.half_h);
    box.setPosition(chest_.x, chest_.y);
    box.setFillColor(chest_.opened ? sf::Color(200, 160, 60) : sf::Color(160, 120, 40));
    box.setOutlineThickness(3.f);
    box.setOutlineColor(sf::Color(40, 30, 20));
    window.draw(box);
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
    pick_options_.reserve(registry_indices.size());
    for (int idx : registry_indices) {
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

int UISystem::update_item_pick(sf::RenderWindow& window) {
    if (pick_options_.empty() || !font_loaded_) {
        return -1;
    }

    const sf::Vector2i mouse_pos = sf::Mouse::getPosition(window);
    if (!sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
        return -1;
    }

    constexpr float card_w = 200.f;
    constexpr float card_h = 280.f;
    constexpr float spacing = 60.f;
    const float total_w = 3.f * card_w + 2.f * spacing;
    const float start_x = (static_cast<float>(window.getSize().x) - total_w) * 0.5f;
    const float start_y = 180.f;

    for (size_t i = 0; i < pick_options_.size(); ++i) {
        const float card_x = start_x + static_cast<float>(i) * (card_w + spacing);
        const sf::FloatRect rect(card_x, start_y, card_w, card_h);
        if (rect.contains(static_cast<float>(mouse_pos.x),
                          static_cast<float>(mouse_pos.y))) {
            const int picked = pick_options_[i].registry_index;
            pick_options_.clear();
            chest_.active = false;
            return picked;
        }
    }
    return -1;
}

void UISystem::draw_item_pick(sf::RenderWindow& window) {
    if (pick_options_.empty()) {
        return;
    }

    sf::RectangleShape overlay(sf::Vector2f(window.getSize()));
    overlay.setFillColor(sf::Color(0, 0, 0, 190));
    window.draw(overlay);

    if (font_loaded_) {
        sf::Text title(L"选择一件道具", font_, 34);
        title.setFillColor(sf::Color(255, 240, 200));
        center_text_origin(title);
        title.setPosition(k_screen_center_x, 70.f);
        window.draw(title);
    }

    constexpr float card_w = 200.f;
    constexpr float card_h = 280.f;
    constexpr float spacing = 60.f;
    const float total_w = 3.f * card_w + 2.f * spacing;
    const float start_x = (static_cast<float>(window.getSize().x) - total_w) * 0.5f;
    const float start_y = 180.f;

    for (size_t i = 0; i < pick_options_.size(); ++i) {
        const ItemPickOption& opt = pick_options_[i];
        const ItemDisplay item = ItemRegistry::getDisplay(opt.registry_index);
        const float card_x = start_x + static_cast<float>(i) * (card_w + spacing);
        const sf::FloatRect card_rect(card_x, start_y, card_w, card_h);

        draw_parchment_card(window, card_rect, item.color);

        const auto tex_it = item_icon_by_registry_.find(opt.registry_index);
        if (tex_it != item_icon_by_registry_.end()) {
            draw_sprite_fit(
                window,
                tex_it->second,
                sf::FloatRect(card_x + 40.f, start_y + 36.f, 120.f, 120.f),
                true);
        }

        if (font_loaded_) {
            draw_level_badge(window, card_x + 14.f, start_y + 118.f, opt.item_level);

            sf::Text name_text(item.name, font_, 20);
            name_text.setFillColor(sf::Color(40, 30, 20));
            center_text_origin(name_text);
            name_text.setPosition(card_x + card_w * 0.5f, start_y + 168.f);
            window.draw(name_text);

            sf::Text desc_text(item.description, font_, 15);
            desc_text.setFillColor(sf::Color(60, 50, 40));
            center_text_origin(desc_text);
            desc_text.setPosition(card_x + card_w * 0.5f, start_y + 200.f);
            window.draw(desc_text);

            sf::Text hint(L"[点击选择]", font_, 14);
            hint.setFillColor(sf::Color(100, 80, 60));
            center_text_origin(hint);
            hint.setPosition(card_x + card_w * 0.5f, start_y + card_h - 36.f);
            window.draw(hint);
        }
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
