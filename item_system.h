/*
 * item_system.h - 被动道具投射物 + 场控状态 + 与 IDamageable 的碰撞结算
 *
 * 模块一：PlayerStats 扩展 + StatusEffect + EnemyDamageable 场控实现 + UI 可读快照。
 * 约束：不修改 ui_system / boss_system；阵营交互仅经 IDamageable；id 纯小写。
 *
 * 实现约定：EnemyDamageable / SpikeProjectile / 模块二投射物 等在本头文件内联实现。
 * 请勿在 item_system.cpp 中重复定义同名成员函数，否则链接报错。
 */

#ifndef ITEM_SYSTEM_H
#define ITEM_SYSTEM_H

#include "damageable.h"
#include "boss_system.h"
#include "player_stats.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Player;
struct Bullet;
struct Enemy;

// ==================== StatusEffect 状态管理 ====================

enum class status_type {
    freeze,
    slow,
    charm
};

struct StatusEffect {
    status_type type = status_type::freeze;
    int         frames_remaining = 0;

    std::function<void(int frames_remaining)> on_tick;
    std::function<void()>                     on_remove;
};

/** 统一维护冰冻 / 减速等状态的生命周期（STL vector + remove_if） */
class StatusEffectManager {
public:
    void add(
        status_type type,
        int frames,
        std::function<void(int frames_remaining)> on_tick = {},
        std::function<void()> on_remove = {})
    {
        if (frames <= 0) {
            return;
        }
        remove_type(type);
        effects_.push_back(StatusEffect{
            type,
            frames,
            std::move(on_tick),
            std::move(on_remove)});
    }

    void tick_one_frame() {
        for (auto& effect : effects_) {
            if (effect.frames_remaining <= 0) {
                continue;
            }
            if (effect.on_tick) {
                effect.on_tick(effect.frames_remaining);
            }
            --effect.frames_remaining;
        }
        cull_expired();
    }

    bool has(status_type type) const {
        for (const auto& effect : effects_) {
            if (effect.type == type && effect.frames_remaining > 0) {
                return true;
            }
        }
        return false;
    }

    int remaining_frames(status_type type) const {
        int max_frames = 0;
        for (const auto& effect : effects_) {
            if (effect.type == type && effect.frames_remaining > max_frames) {
                max_frames = effect.frames_remaining;
            }
        }
        return max_frames;
    }

    void remove_type(status_type type) {
        for (auto it = effects_.begin(); it != effects_.end();) {
            if (it->type == type) {
                if (it->on_remove) {
                    it->on_remove();
                }
                it = effects_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void clear() {
        for (auto& effect : effects_) {
            if (effect.on_remove) {
                effect.on_remove();
            }
        }
        effects_.clear();
    }

    const std::vector<StatusEffect>& effects() const { return effects_; }

private:
    void cull_expired() {
        const auto new_end = std::remove_if(
            effects_.begin(),
            effects_.end(),
            [](const StatusEffect& effect) {
                return effect.frames_remaining <= 0;
            });
        for (auto it = new_end; it != effects_.end(); ++it) {
            if (it->on_remove) {
                it->on_remove();
            }
        }
        effects_.erase(new_end, effects_.end());
    }

    std::vector<StatusEffect> effects_;
};

// ==================== PlayerStats 场控/数值变异扩展 ====================
/**
 * PlayerStats 扩展（与 Player::stats 同步；后续可并入 player_stats.h）。
 * 速度：base_speed（像素/秒）× current_speed_multiplier。
 */
struct PlayerStatsItemFields {
    int  mirror_clone_level = 0;
    bool has_glass_shard    = false;
    bool has_tiny_planet    = false;
    bool has_godhead        = false;
    bool has_apple          = false;
    bool has_spike_nail     = false;
    bool has_ice_baby       = false;
    bool has_betrayal       = false;

    float base_speed               = 320.f;
    float current_speed_multiplier = 1.f;

    StatusEffectManager player_status_effects;

    float getEffectiveSpeed() const {
        return base_speed * current_speed_multiplier;
    }

    /** @deprecated 使用 getEffectiveSpeed */
    float effective_speed() const { return getEffectiveSpeed(); }

    bool is_speed_debuff_active() const {
        return current_speed_multiplier < 0.999f;
    }

    /** @deprecated 使用 is_speed_debuff_active */
    bool is_slowed() const { return is_speed_debuff_active(); }

    void resetSpeedMultiplier() {
        current_speed_multiplier = 1.f;
    }

    void sync_base_speed_from(const PlayerStats& stats) {
        if (base_speed <= 0.f) {
            base_speed = static_cast<float>(stats.speed);
        }
    }

    void applySlow(float multiplier, int duration_frames) {
        current_speed_multiplier = std::max(0.f, std::min(1.f, multiplier));
        player_status_effects.add(
            status_type::slow,
            duration_frames,
            {},
            [this]() { resetSpeedMultiplier(); });
    }

    void tick_one_frame(float recovery_step = 0.02f) {
        player_status_effects.tick_one_frame();
        if (!player_status_effects.has(status_type::slow)) {
            if (current_speed_multiplier < 1.f) {
                current_speed_multiplier =
                    std::min(1.f, current_speed_multiplier + recovery_step);
            }
        }
    }
};

// ==================== 模块一：界面可读数据（非 ui_system 绘制） ====================

namespace item_ui {
inline constexpr int k_pickup_toast_frames = 120;  // 2s @60fps

struct pickup_toast {
    std::string item_id;
    std::wstring display_text;
    int          frames_remaining = 0;
};

struct passive_status_bar {
    bool  has_glass_shard     = false;
    bool  has_tiny_planet     = false;
    bool  has_godhead         = false;
    bool  has_apple           = false;
    bool  has_betrayal        = false;
    int   mirror_clone_level  = 0;
    float effective_speed     = 320.f;
    bool  speed_debuff_flash  = false;
};

inline const wchar_t* pickup_message_for_item_id(const char* item_id) {
    if (!item_id) {
        return L"";
    }
    const std::string id(item_id);
    if (id == "glass_shard") {
        return L"获得玻璃碎片- 距离越近伤害越高";
    }
    if (id == "tiny_planet") {
        return L"获得迷你星球 - 环绕泪弹";
    }
    if (id == "godhead") {
        return L"获得神性- 追踪光环持续伤害";
    }
    if (id == "apple" || id == "apple_razor") {
        return L"获得苹果刀片-子弹命中后分裂";
    }
    if (id == "betrayal") {
        return L"获得背叛-子弹使敌人倒戈";
    }
    if (id == "mirror_clone") {
        return L"获得镜之分身 - 镜像射击";
    }
    return L"获得新道具";
}

inline void push_pickup_toast(
    std::vector<pickup_toast>& queue,
    const char* item_id,
    const wchar_t* message = nullptr)
{
    pickup_toast toast;
    toast.item_id = item_id ? item_id : "";
    toast.display_text = message ? message : pickup_message_for_item_id(item_id);
    toast.frames_remaining = k_pickup_toast_frames;
    queue.push_back(std::move(toast));
}

inline passive_status_bar make_status_bar_snapshot(
    const PlayerStatsItemFields& fields)
{
    passive_status_bar bar;
    bar.has_glass_shard    = fields.has_glass_shard;
    bar.has_tiny_planet    = fields.has_tiny_planet;
    bar.has_godhead        = fields.has_godhead;
    bar.has_apple          = fields.has_apple;
    bar.has_betrayal       = fields.has_betrayal;
    bar.mirror_clone_level = fields.mirror_clone_level;
    bar.effective_speed    = fields.getEffectiveSpeed();
    bar.speed_debuff_flash = fields.is_speed_debuff_active();
    return bar;
}

inline void tick_pickup_toasts(std::vector<pickup_toast>& queue) {
    for (auto& toast : queue) {
        if (toast.frames_remaining > 0) {
            --toast.frames_remaining;
        }
    }
    queue.erase(
        std::remove_if(
            queue.begin(),
            queue.end(),
            [](const pickup_toast& t) { return t.frames_remaining <= 0; }),
        queue.end());
}
} // namespace item_ui

/** @deprecated 使用 PlayerStatsItemFields::applySlow */
inline void apply_player_slow(
    PlayerStatsItemFields& fields,
    StatusEffectManager& mgr,
    float multiplier,
    int frames)
{
    fields.applySlow(multiplier, frames);
    (void)mgr;
}

/** 每帧 tick 玩家扩展字段（含减速恢复） */
inline void tick_player_item_fields(
    PlayerStatsItemFields& fields,
    float recovery_step = 0.02f)
{
    fields.tick_one_frame(recovery_step);
}

// ==================== 场控特效常量（界面/逻辑共用） ====================

namespace combat_fx {
inline constexpr int   k_betrayal_effect_frames  = 600; // 背叛魅惑 ~10s @60fps
inline constexpr float k_betray_halo_spin      = 0.09f;
inline constexpr float k_freeze_thaw_warn_frames = 90;
inline constexpr int   k_freeze_shatter_frames   = 14;
inline constexpr int   k_freeze_recondense_frames = 10;
inline constexpr int   k_freeze_hit_burst_frames = 12;
inline constexpr int   k_ice_baby_freeze_frames  = 300; // 5s @60fps
inline constexpr int   k_ice_baby_max_active     = 5;   // 同屏最多 5 个冰弹
inline constexpr int   k_freeze_max_stack_frames = 900; // 冰冻累积上限 ~15s
inline constexpr float k_enemy_default_descent_vy = 90.f;
inline constexpr int   k_knockback_trail_frames  = 18;
inline constexpr int   k_knockback_shock_frames  = 10;
inline constexpr int   k_knockback_ghost_count   = 3;
inline constexpr float k_knockback_dist_min      = 80.f;
inline constexpr float k_knockback_dist_max      = 150.f;
inline constexpr int   k_knockback_stun_frames   = 12;   // ~0.2s @60fps

/** 八寸钉击退：距离/硬直/震屏（可在 main 或测试 JSON 覆盖思路） */
struct spike_nail_config {
    static constexpr float distance_min          = 80.f;
    static constexpr float distance_max          = 150.f;
    static constexpr int   stun_frames           = k_knockback_stun_frames;
    static constexpr bool  screen_shake_enabled  = true;
    static constexpr float screen_shake_strength = 9.f;
};

inline const sf::Color k_betray_body_color(120, 90, 220);
inline const sf::Color k_betray_bullet_color(80, 160, 255);
inline const sf::Color k_enemy_body_color = sf::Color::Red;

inline float roll_knockback_distance(float seed_x, float seed_y) {
    return k_knockback_dist_min
           + std::fmod(seed_x * 1.7f + seed_y * 2.3f,
                       k_knockback_dist_max - k_knockback_dist_min);
}

inline knockback_params make_spike_nail_knockback(float player_x, float player_y) {
    knockback_params params;
    params.origin_x              = player_x;
    params.origin_y              = player_y;
    params.distance              = 0.f;
    params.distance_min          = spike_nail_config::distance_min;
    params.distance_max          = spike_nail_config::distance_max;
    params.stun_frames           = spike_nail_config::stun_frames;
    params.screen_shake          = spike_nail_config::screen_shake_enabled;
    params.screen_shake_strength = spike_nail_config::screen_shake_strength;
    return params;
}
} // namespace combat_fx

// ==================== 敌人 → IDamageable 适配 ====================

class EnemyDamageable : public IDamageable {
public:
    explicit EnemyDamageable(Enemy& enemy) : enemy_(enemy) {}

    Hitbox getHitbox() const override {
        return Hitbox{
            enemy_.x - 15.f,
            enemy_.y - 15.f,
            30.f,
            30.f};
    }

    bool isAlive() const override { return enemy_.hp > 0; }

    bool takeDamage(int damage) override {
        if (enemy_.hp <= 0) {
            return true;
        }
        if (isFrozen()) {
            enemy_.combat_freeze_shatter = combat_fx::k_freeze_shatter_frames;
        }
        enemy_.hp -= damage;
        return enemy_.hp <= 0;
    }

    Faction getFaction() const override {
        return enemy_.combat_betrayed ? Faction::player : Faction::enemy;
    }

    bool isFrozen() const override { return enemy_.combat_freeze_frames > 0; }

    bool can_move() const override {
        return !isFrozen() && enemy_.combat_knockback_stun <= 0;
    }

    bool can_shoot() const override {
        return !isFrozen() && enemy_.combat_knockback_stun <= 0;
    }

    bool hurts_player_on_contact() const override {
        return !enemy_.combat_betrayed;
    }

    combat_status_icon combat_status_icons() const override {
        combat_status_icon icons = combat_status_icon::none;
        if (isFrozen()) {
            icons = icons | combat_status_icon::frozen;
        }
        if (enemy_.combat_betrayed) {
            icons = icons | combat_status_icon::betrayed;
        }
        if (enemy_.combat_knockback_trail > 0) {
            icons = icons | combat_status_icon::knockback;
        }
        return icons;
    }

    int freeze_frames_remaining() const override {
        return enemy_.combat_freeze_frames;
    }

    int knockback_stun_remaining() const override {
        return enemy_.combat_knockback_stun;
    }

    float knockback_trail_intensity() const override {
        if (enemy_.combat_knockback_trail <= 0) {
            return 0.f;
        }
        return static_cast<float>(enemy_.combat_knockback_trail)
               / static_cast<float>(combat_fx::k_knockback_trail_frames);
    }

    combat_visual_state get_combat_visual() const override {
        combat_visual_state v;
        const int freeze_left = freeze_frames_remaining();

        if (enemy_.combat_betrayed) {
            v.betray_active         = true;
            v.betray_halo_angle     = enemy_.combat_betray_halo_angle;
            v.betray_particle_alpha =
                0.35f + 0.15f * std::sin(enemy_.combat_betray_halo_angle * 2.f);
            v.bullets_player_tint   = true;
            v.body_tint_r           = static_cast<float>(combat_fx::k_betray_body_color.r);
            v.body_tint_g           = static_cast<float>(combat_fx::k_betray_body_color.g);
            v.body_tint_b           = static_cast<float>(combat_fx::k_betray_body_color.b);
        }

        if (isFrozen()) {
            v.freeze_active          = true;
            v.freeze_frames_left     = freeze_left;
            v.freeze_ice_layer_alpha = 0.55f;
            v.freeze_snow_density    = 0.7f;
            if (enemy_.combat_freeze_shatter > 0) {
                v.freeze_shatter_flash =
                    static_cast<float>(enemy_.combat_freeze_shatter)
                    / static_cast<float>(combat_fx::k_freeze_shatter_frames);
            }
            if (enemy_.combat_freeze_recondense > 0) {
                v.freeze_hit_burst =
                    static_cast<float>(enemy_.combat_freeze_recondense)
                    / static_cast<float>(combat_fx::k_freeze_recondense_frames)
                    * 0.55f;
            }
            if (enemy_.combat_freeze_hit_burst > 0) {
                v.freeze_hit_burst =
                    static_cast<float>(enemy_.combat_freeze_hit_burst)
                    / static_cast<float>(combat_fx::k_freeze_hit_burst_frames);
            }
            if (freeze_left > 0
                && freeze_left <= static_cast<int>(combat_fx::k_freeze_thaw_warn_frames)) {
                v.freeze_thaw_warning = true;
                v.freeze_crack_pulse =
                    0.5f + 0.5f * std::sin(static_cast<float>(freeze_left) * 0.35f);
            }
        }

        v.knockback_trail = knockback_trail_intensity();
        if (enemy_.combat_shockwave > 0) {
            v.knockback_shockwave =
                static_cast<float>(enemy_.combat_shockwave)
                / static_cast<float>(combat_fx::k_knockback_shock_frames);
        }
        v.knockback_stun_frames = enemy_.combat_knockback_stun;
        v.request_screen_shake  = enemy_.combat_screen_shake_pending;
        v.knockback_ghost_x.assign(
            enemy_.combat_ghost_x,
            enemy_.combat_ghost_x + enemy_.combat_ghost_count);
        v.knockback_ghost_y.assign(
            enemy_.combat_ghost_y,
            enemy_.combat_ghost_y + enemy_.combat_ghost_count);
        return v;
    }

    void setFaction(Faction f) override {
        if (f == Faction::player) {
            enemy_.combat_betrayed              = true;
            enemy_.combat_betrayal_frames =
                combat_fx::k_betrayal_effect_frames;
            enemy_.color                = combat_fx::k_betray_body_color;
        } else {
            enemy_.combat_betrayed      = false;
            enemy_.combat_betrayal_frames = 0;
            enemy_.color               = combat_fx::k_enemy_body_color;
        }
    }

    void applyBetrayalEffect(int frames) override {
        if (frames <= 0) {
            return;
        }
        enemy_.combat_betrayed = true;
        enemy_.combat_betrayal_frames =
            std::max(enemy_.combat_betrayal_frames, frames);
        enemy_.combat_betray_blend = 18;
        enemy_.color = combat_fx::k_betray_body_color;
        enemy_.vy = std::min(-80.f, enemy_.vy - 60.f);
        if (enemy_.vy > -50.f) {
            enemy_.vy = -110.f;
        }
    }

    void applyFreeze(int frames) override {
        if (frames <= 0) {
            return;
        }
        if (enemy_.combat_freeze_frames <= 0) {
            enemy_.combat_saved_vx = enemy_.vx;
            enemy_.combat_saved_vy = enemy_.vy;
        }
        enemy_.combat_freeze_frames = std::min(
            combat_fx::k_freeze_max_stack_frames,
            enemy_.combat_freeze_frames + frames);
        enemy_.vx = 0.f;
        enemy_.vy = 0.f;
    }

    void applyFreezeHit(int frames) override {
        enemy_.combat_freeze_hit_burst = combat_fx::k_freeze_hit_burst_frames;
        applyFreeze(frames);
    }

    void applyKnockback(float distance) override {
        knockback_params params;
        params.distance = distance;
        applyKnockbackRadial(params);
    }

    void applyKnockbackRadial(const knockback_params& params) override {
        float dist = params.distance;
        if (dist <= 0.f) {
            dist = combat_fx::roll_knockback_distance(enemy_.x, enemy_.y);
        }
        dist = std::max(params.distance_min,
                        std::min(params.distance_max, dist));

        float dx = enemy_.x - params.origin_x;
        float dy = enemy_.y - params.origin_y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1.f) {
            dx = 0.f;
            dy = -1.f;
            len = 1.f;
        }
        const float nx = dx / len;
        const float ny = dy / len;

        enemy_.combat_shockwave_x = enemy_.x;
        enemy_.combat_shockwave_y = enemy_.y;

        const float start_x = enemy_.x;
        const float start_y = enemy_.y;
        static const float ghost_alphas[3] = {0.22f, 0.38f, 0.54f};
        enemy_.combat_ghost_count = 3;
        for (int i = 0; i < 3; ++i) {
            const float t = static_cast<float>(i + 1) / 3.f;
            enemy_.combat_ghost_x[i]     = start_x + nx * dist * t;
            enemy_.combat_ghost_y[i]     = start_y + ny * dist * t;
            enemy_.combat_ghost_alpha[i] = ghost_alphas[i];
        }

        enemy_.x = start_x + nx * dist;
        enemy_.y = start_y + ny * dist;
        enemy_.x = std::max(20.f, std::min(780.f, enemy_.x));
        enemy_.y = std::max(20.f, std::min(880.f, enemy_.y));

        enemy_.combat_knockback_trail = combat_fx::k_knockback_trail_frames;
        enemy_.combat_knockback_stun  = (params.stun_frames > 0)
            ? params.stun_frames
            : combat_fx::k_knockback_stun_frames;
        enemy_.combat_shockwave       = combat_fx::k_knockback_shock_frames;
        if (params.screen_shake && params.screen_shake_strength > 0.f) {
            enemy_.combat_screen_shake_pending  = true;
            enemy_.combat_screen_shake_strength = params.screen_shake_strength;
        }
        enemy_.vx *= 0.2f;
        enemy_.vy *= 0.2f;
    }

    bool consume_screen_shake_request() override {
        const bool pending = enemy_.combat_screen_shake_pending;
        enemy_.combat_screen_shake_pending = false;
        return pending;
    }

    void tick_status_effects() {
        if (enemy_.combat_freeze_frames > 0) {
            --enemy_.combat_freeze_frames;
            if (enemy_.combat_freeze_frames == 0) {
                enemy_.vx = enemy_.combat_saved_vx;
                enemy_.vy = enemy_.combat_saved_vy;
                if (enemy_.vy < 30.f) {
                    enemy_.vy = combat_fx::k_enemy_default_descent_vy;
                }
            }
        }
        if (enemy_.combat_betrayal_frames > 0) {
            --enemy_.combat_betrayal_frames;
            if (enemy_.combat_betrayal_frames == 0 && enemy_.combat_betrayed) {
                setFaction(Faction::enemy);
            }
        }

        if (enemy_.combat_betrayed) {
            enemy_.combat_betray_halo_angle += combat_fx::k_betray_halo_spin;
        }
        if (enemy_.combat_betray_blend > 0) {
            --enemy_.combat_betray_blend;
        }

        if (isFrozen()) {
            enemy_.vx = 0.f;
            enemy_.vy = 0.f;
        }

        if (enemy_.combat_freeze_shatter > 0) {
            --enemy_.combat_freeze_shatter;
            if (enemy_.combat_freeze_shatter == 0
                && enemy_.combat_freeze_frames > 0) {
                enemy_.combat_freeze_recondense =
                    combat_fx::k_freeze_recondense_frames;
            }
        }
        if (enemy_.combat_freeze_hit_burst > 0) {
            --enemy_.combat_freeze_hit_burst;
        }
        if (enemy_.combat_freeze_recondense > 0) {
            --enemy_.combat_freeze_recondense;
        }
        if (enemy_.combat_knockback_trail > 0) {
            --enemy_.combat_knockback_trail;
        }
        if (enemy_.combat_shockwave > 0) {
            --enemy_.combat_shockwave;
        }
        if (enemy_.combat_knockback_stun > 0) {
            --enemy_.combat_knockback_stun;
        }
    }

    Enemy& enemy() { return enemy_; }

private:
    Enemy& enemy_;
};

// ==================== 投射物 ====================

class BaseProjectile {
public:
    virtual ~BaseProjectile() = default;

    virtual Hitbox getHitbox() const = 0;
    virtual int    getDamage() const = 0;
    virtual bool   isAlive() const = 0;
    virtual void   update(float dt) = 0;
    virtual void   render(sf::RenderTarget& target) const { (void)target; }

    /** 与目标碰撞箱是否相交（子类可重写为圆环等判定） */
    virtual bool test_hit(const Hitbox& target) const {
        return hitbox_intersects(getHitbox(), target);
    }

    /** 命中时是否走 takeDamage（准星/冰球等可走 on_contact） */
    virtual bool deals_direct_damage() const { return true; }

    /** 为 true 时 ItemManager 对本帧所有 IDamageable 做范围判定 */
    virtual bool is_aoe_burst() const { return false; }

    /** 命中 IDamageable 时；@return 是否消灭目标 */
    virtual bool on_contact(IDamageable& target) {
        if (deals_direct_damage() && getDamage() > 0) {
            return target.takeDamage(getDamage());
        }
        return false;
    }

    virtual bool consumes_on_hit() const { return true; }

    virtual const char* projectile_id() const { return "base"; }
};

class BulletProjectile : public BaseProjectile {
public:
    BulletProjectile(Bullet& bullet, int player_damage_fallback);

    Hitbox getHitbox() const override;
    int    getDamage() const override;
    bool   isAlive() const override;
    void   update(float dt) override;
    bool   consumes_on_hit() const override { return true; }

    Bullet& bullet() { return bullet_; }

private:
    Bullet& bullet_;
    int     player_damage_fallback_ = 1;
};

/** 八寸钉：命中后径向击退 80~150px + 冲击波/硬直 */
class SpikeProjectile : public BaseProjectile {
public:
    SpikeProjectile(
        float x, float y, float vx, float vy, int damage, float life,
        float player_x = 400.f, float player_y = 700.f)
        : x_(x)
        , y_(y)
        , vx_(vx)
        , vy_(vy)
        , damage_(damage)
        , life_(life)
        , max_life_(life)
        , player_x_(player_x)
        , player_y_(player_y) {}

    Hitbox getHitbox() const override {
        return Hitbox{x_ - radius_, y_ - radius_, radius_ * 2.f, radius_ * 2.f};
    }

    int  getDamage() const override { return damage_; }
    bool isAlive() const override { return life_ > 0.f; }

    void update(float dt) override {
        x_ += vx_ * dt;
        y_ += vy_ * dt;
        life_ -= dt;
    }

    bool on_contact(IDamageable& target) override {
        target.applyKnockbackRadial(
            combat_fx::make_spike_nail_knockback(player_x_, player_y_));
        if (getDamage() > 0) {
            return target.takeDamage(getDamage());
        }
        return false;
    }

    void render(sf::RenderTarget& target) const override {
        sf::CircleShape core(radius_ * 0.55f);
        core.setOrigin(core.getRadius(), core.getRadius());
        core.setPosition(x_, y_);
        core.setFillColor(sf::Color(220, 220, 230, 230));
        core.setOutlineColor(sf::Color(80, 80, 100));
        core.setOutlineThickness(1.5f);
        target.draw(core);

        const float life_ratio = (max_life_ > 0.f) ? (life_ / max_life_) : 1.f;
        sf::RectangleShape spike(sf::Vector2f(radius_ * 1.6f, 3.f));
        spike.setOrigin(spike.getSize().x * 0.5f, 1.5f);
        spike.setPosition(x_, y_);
        spike.setFillColor(sf::Color(200, 200, 210,
            static_cast<sf::Uint8>(180 * life_ratio)));
        spike.setRotation(
            std::atan2(vy_, vx_) * 57.2958f);
        target.draw(spike);
    }

    const char* projectile_id() const override { return "spike"; }

private:
    float x_, y_, vx_, vy_, life_, max_life_;
    float player_x_, player_y_;
    int   damage_;
    float radius_ = 10.f;
};

class AppleProjectile : public BaseProjectile {
public:
    AppleProjectile(float x, float y, float vy, int damage);

    Hitbox getHitbox() const override;
    int    getDamage() const override;
    bool   isAlive() const override;
    void   update(float dt) override;

private:
    float x_, y_, vy_;
    int   damage_;
    float radius_ = 14.f;
};

class GlassShardProjectile : public BaseProjectile {
public:
    GlassShardProjectile(float x, float y, float vx, float vy, int damage, float life);

    Hitbox getHitbox() const override;
    int    getDamage() const override;
    bool   isAlive() const override;
    void   update(float dt) override;

private:
    float x_, y_, vx_, vy_, life_;
    int   damage_;
    float radius_ = 6.f;
};

// ==================== 模块二：物理变异投射物（实现均在本头文件） ====================

namespace item_combat {
inline void draw_frame_count_digits(
    sf::RenderTarget& target,
    int               frames,
    float             center_x,
    float             row_y);
} // forward

namespace module2_fx {

struct ParticleRequest {
    float     x = 0.f;
    float     y = 0.f;
    sf::Color color = sf::Color::White;
    int       count = 8;
};

inline std::vector<ParticleRequest> g_pending_particles;
inline float g_pending_screen_shake = 0.f;
inline int   g_screen_flash_frames  = 0;

inline void request_particles(float x, float y, sf::Color color, int count) {
    g_pending_particles.push_back({x, y, color, count});
}

inline void flush_particles(
    const std::function<void(float, float, sf::Color, int)>& spawn)
{
    for (const ParticleRequest& req : g_pending_particles) {
        spawn(req.x, req.y, req.color, req.count);
    }
    g_pending_particles.clear();
}

inline void request_screen_shake(float strength) {
    g_pending_screen_shake = std::max(g_pending_screen_shake, strength);
}

inline float consume_screen_shake() {
    const float strength = g_pending_screen_shake;
    g_pending_screen_shake = 0.f;
    return strength;
}

inline void request_screen_flash(int frames = 6) {
    g_screen_flash_frames = std::max(g_screen_flash_frames, frames);
}

inline void tick_screen_flash() {
    if (g_screen_flash_frames > 0) {
        --g_screen_flash_frames;
    }
}

inline sf::Uint8 screen_flash_alpha() {
    if (g_screen_flash_frames <= 0) {
        return 0;
    }
    return static_cast<sf::Uint8>(
        210.f * static_cast<float>(g_screen_flash_frames) / 6.f);
}

inline void draw_trail_line_strip(
    sf::RenderTarget&              target,
    const std::vector<sf::Vector2f>& points,
    sf::Color                      base_color,
    float                          width = 2.f)
{
    if (points.size() < 2) {
        return;
    }
    for (size_t i = 1; i < points.size(); ++i) {
        const float t =
            static_cast<float>(i) / static_cast<float>(points.size());
        sf::Color c = base_color;
        c.a = static_cast<sf::Uint8>(base_color.a * t);
        sf::Vertex seg[] = {
            sf::Vertex(points[i - 1], c),
            sf::Vertex(points[i], sf::Color(c.r, c.g, c.b, 0)),
        };
        target.draw(seg, 2, sf::Lines);
    }
    (void)width;
}

} // namespace module2_fx

/** 小小星球：螺旋公转 + 星轨拖尾，命中小爆炸 */
class OrbitingTear : public BaseProjectile {
public:
    OrbitingTear(
        std::function<sf::Vector2f()> anchor_getter,
        float start_radius,
        float orbit_growth,
        float angular_speed,
        int   damage)
        : anchor_getter_(std::move(anchor_getter))
        , orbit_radius_(start_radius)
        , orbit_growth_(orbit_growth)
        , angular_speed_(angular_speed)
        , damage_(damage) {}

    Hitbox getHitbox() const override {
        return hitbox_from_circle(x_, y_, tear_radius_);
    }

    int  getDamage() const override { return damage_; }
    bool isAlive() const override { return alive_; }

    void update(float dt) override {
        if (!anchor_getter_) {
            alive_ = false;
            return;
        }
        const sf::Vector2f anchor = anchor_getter_();
        orbit_radius_ += orbit_growth_ * dt;
        orbit_radius_ = std::min(orbit_radius_, max_orbit_radius_);
        orbit_angle_  += angular_speed_ * dt;
        x_ = anchor.x + orbit_radius_ * std::cos(orbit_angle_);
        y_ = anchor.y + orbit_radius_ * std::sin(orbit_angle_);

        trail_.push_back(sf::Vector2f(x_, y_));
        if (trail_.size() > trail_capacity_) {
            trail_.erase(trail_.begin());
        }
        if (orbit_trace_timer_++ % 3 == 0) {
            orbit_arcs_.push_back(sf::Vector2f(x_, y_));
            if (orbit_arcs_.size() > 48) {
                orbit_arcs_.erase(orbit_arcs_.begin());
            }
        }
        spin_angle_ += dt * 5.f;

        if (x_ < -60.f || x_ > static_cast<float>(SCREEN_WIDTH) + 60.f ||
            y_ < -60.f || y_ > static_cast<float>(SCREEN_HEIGHT) + 60.f) {
            alive_ = false;
        }
    }

    bool on_contact(IDamageable& target) override {
        module2_fx::request_particles(
            x_, y_, sf::Color(160, 210, 255), 18);
        module2_fx::request_particles(
            x_, y_, sf::Color(255, 255, 255), 8);
        return target.takeDamage(getDamage());
    }

    void render(sf::RenderTarget& target) const override {
        for (size_t i = 2; i < orbit_arcs_.size(); i += 4) {
            const float fade =
                static_cast<float>(i) / static_cast<float>(orbit_arcs_.size());
            sf::Vertex arc[] = {
                sf::Vertex(
                    orbit_arcs_[i - 1],
                    sf::Color(120, 180, 255,
                        static_cast<sf::Uint8>(50 * fade))),
                sf::Vertex(
                    orbit_arcs_[i],
                    sf::Color(120, 180, 255, 0)),
            };
            target.draw(arc, 2, sf::Lines);
        }

        module2_fx::draw_trail_line_strip(
            target, trail_, sf::Color(180, 220, 255, 140));

        for (int p = 0; p < 5; ++p) {
            const float ang = spin_angle_ + static_cast<float>(p) * 1.256f;
            sf::CircleShape spark(1.5f);
            spark.setOrigin(1.5f, 1.5f);
            spark.setPosition(
                x_ - std::cos(ang) * tear_radius_ * 1.6f,
                y_ - std::sin(ang) * tear_radius_ * 1.6f);
            spark.setFillColor(sf::Color(220, 240, 255, 160));
            target.draw(spark);
        }

        sf::CircleShape glow(tear_radius_ + 4.f);
        glow.setOrigin(glow.getRadius(), glow.getRadius());
        glow.setPosition(x_, y_);
        glow.setFillColor(sf::Color(80, 140, 255, 45));
        target.draw(glow);

        sf::CircleShape tear(tear_radius_);
        tear.setOrigin(tear_radius_, tear_radius_);
        tear.setPosition(x_, y_);
        tear.setFillColor(sf::Color(140, 190, 255, 230));
        tear.setOutlineColor(sf::Color(60, 120, 220));
        tear.setOutlineThickness(1.5f);
        target.draw(tear);

        sf::CircleShape crater(tear_radius_ * 0.35f);
        crater.setOrigin(crater.getRadius(), crater.getRadius());
        crater.setPosition(
            x_ + tear_radius_ * 0.25f * std::cos(spin_angle_),
            y_ + tear_radius_ * 0.25f * std::sin(spin_angle_));
        crater.setFillColor(sf::Color(90, 130, 200, 180));
        target.draw(crater);

        sf::CircleShape highlight(tear_radius_ * 0.18f);
        highlight.setOrigin(highlight.getRadius(), highlight.getRadius());
        highlight.setPosition(
            x_ - tear_radius_ * 0.3f,
            y_ - tear_radius_ * 0.35f);
        highlight.setFillColor(sf::Color(255, 255, 255, 150));
        target.draw(highlight);
    }

    const char* projectile_id() const override { return "orbiting_tear"; }

private:
    std::function<sf::Vector2f()> anchor_getter_;
    float orbit_radius_      = 24.f;
    float orbit_angle_       = 0.f;
    float orbit_growth_      = 40.f;
    float angular_speed_     = 4.f;
    float max_orbit_radius_  = 130.f;
    float spin_angle_        = 0.f;
    float x_                 = 0.f;
    float y_                 = 0.f;
    int   damage_            = 1;
    float tear_radius_       = 7.f;
    bool  alive_             = true;

    mutable std::vector<sf::Vector2f> trail_;
    mutable std::vector<sf::Vector2f> orbit_arcs_;
    mutable int orbit_trace_timer_ = 0;
    static constexpr size_t trail_capacity_ = 28;
};

/** 科技X：上行空心激光环，仅边缘命中，扩大后淡出 */
class LaserRing : public BaseProjectile {
public:
    LaserRing(float x, float y, float vy, float start_radius, int damage)
        : x_(x), y_(y), vy_(vy), radius_(start_radius), damage_(damage) {}

    Hitbox getHitbox() const override {
        return hitbox_from_circle(x_, y_, radius_ + ring_thickness_);
    }

    int  getDamage() const override { return damage_; }
    bool isAlive() const override { return alive_ && alpha_ > 0.02f; }
    bool consumes_on_hit() const override { return false; }

    void update(float dt) override {
        y_ += vy_ * dt;
        radius_ += radius_growth_ * dt;
        sparkle_angle_ += dt * 8.f;

        if (radius_ > max_radius_) {
            alpha_ -= dt * 1.8f;
        }
        if (y_ < -radius_ * 3.f || alpha_ <= 0.f) {
            alive_ = false;
        }
        for (auto& spark : hit_sparks_) {
            spark.life -= dt;
        }
        hit_sparks_.erase(
            std::remove_if(
                hit_sparks_.begin(),
                hit_sparks_.end(),
                [](const HitSpark& s) { return s.life <= 0.f; }),
            hit_sparks_.end());
    }

    bool test_hit(const Hitbox& target) const override {
        return ring_intersects_hitbox(x_, y_, radius_, ring_thickness_, target);
    }

    bool on_contact(IDamageable& target) override {
        const uintptr_t key = reinterpret_cast<uintptr_t>(&target);
        if (std::find(damaged_targets_.begin(), damaged_targets_.end(), key)
            != damaged_targets_.end()) {
            return false;
        }
        damaged_targets_.push_back(key);

        const Hitbox hb = target.getHitbox();
        const float tx = hb.x + hb.w * 0.5f;
        const float ty = hb.y + hb.h * 0.5f;
        hit_sparks_.push_back(HitSpark{tx, ty, 0.35f});
        module2_fx::request_particles(
            tx, ty, sf::Color(120, 255, 220), 10);
        module2_fx::request_particles(
            tx, ty, sf::Color(255, 255, 255), 4);
        return target.takeDamage(getDamage());
    }

    void render(sf::RenderTarget& target) const override {
        const sf::Uint8 base_a =
            static_cast<sf::Uint8>(200.f * std::max(0.f, alpha_));

        sf::CircleShape outer_glow(radius_ + ring_thickness_ + 6.f);
        outer_glow.setOrigin(outer_glow.getRadius(), outer_glow.getRadius());
        outer_glow.setPosition(x_, y_);
        outer_glow.setFillColor(sf::Color::Transparent);
        outer_glow.setOutlineColor(
            sf::Color(80, 220, 200, static_cast<sf::Uint8>(base_a * 0.35f)));
        outer_glow.setOutlineThickness(4.f);
        target.draw(outer_glow);

        sf::CircleShape ring(radius_);
        ring.setOrigin(radius_, radius_);
        ring.setPosition(x_, y_);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineColor(sf::Color(100, 255, 230, base_a));
        ring.setOutlineThickness(ring_thickness_);
        target.draw(ring);

        sf::CircleShape inner(radius_ - ring_thickness_ * 0.35f);
        inner.setOrigin(inner.getRadius(), inner.getRadius());
        inner.setPosition(x_, y_);
        inner.setFillColor(sf::Color::Transparent);
        inner.setOutlineColor(
            sf::Color(180, 255, 240, static_cast<sf::Uint8>(base_a * 0.5f)));
        inner.setOutlineThickness(2.f);
        target.draw(inner);

        for (int i = 0; i < 12; ++i) {
            const float ang =
                sparkle_angle_ + static_cast<float>(i) * 0.523599f;
            const float px = x_ + std::cos(ang) * radius_;
            const float py = y_ + std::sin(ang) * radius_;
            sf::CircleShape dot(2.f);
            dot.setOrigin(2.f, 2.f);
            dot.setPosition(px, py);
            dot.setFillColor(sf::Color(255, 255, 255,
                static_cast<sf::Uint8>(base_a * 0.8f)));
            target.draw(dot);
        }

        for (const HitSpark& spark : hit_sparks_) {
            const float t = spark.life / 0.35f;
            sf::CircleShape burst(6.f * t);
            burst.setOrigin(burst.getRadius(), burst.getRadius());
            burst.setPosition(spark.x, spark.y);
            burst.setFillColor(sf::Color(255, 200, 120,
                static_cast<sf::Uint8>(180 * t)));
            target.draw(burst);
        }
    }

    const char* projectile_id() const override { return "laser_ring"; }

private:
    struct HitSpark {
        float x;
        float y;
        float life;
    };

    float x_;
    float y_;
    float vy_;
    float radius_;
    float ring_thickness_ = 10.f;
    float radius_growth_  = 55.f;
    float max_radius_     = 125.f;
    float alpha_          = 1.f;
    float sparkle_angle_  = 0.f;
    int   damage_;
    bool  alive_          = true;
    mutable std::vector<HitSpark> hit_sparks_;
    std::vector<uintptr_t> damaged_targets_;
};

/** 史诗婴儿：准星倒计时 60 帧 → 范围爆炸 + 全屏闪 + 震屏 */
class EpicCrosshair : public BaseProjectile {
public:
    EpicCrosshair(float x, float y, int explosion_damage, float explosion_radius)
        : x_(x)
        , y_(y)
        , explosion_damage_(explosion_damage)
        , explosion_radius_(explosion_radius) {}

    Hitbox getHitbox() const override {
        if (burst_active_) {
            return hitbox_from_circle(x_, y_, explosion_radius_);
        }
        return hitbox_from_circle(x_, y_, crosshair_radius_);
    }

    int getDamage() const override {
        return burst_active_ ? explosion_damage_ : 0;
    }

    bool isAlive() const override { return alive_; }
    bool is_aoe_burst() const override { return burst_active_; }
    bool deals_direct_damage() const override { return false; }
    bool consumes_on_hit() const override { return false; }

    void update(float dt) override {
        (void)dt;
        if (burst_active_) {
            ++burst_frame_;
            if (burst_frame_ >= burst_visual_frames_) {
                alive_ = false;
            }
            return;
        }
        ++frame_counter_;
        if (frame_counter_ >= fuse_frames_) {
            burst_active_   = true;
            burst_frame_    = 0;
            module2_fx::request_screen_flash(6);
            module2_fx::request_screen_shake(11.f);
            module2_fx::request_particles(
                x_, y_, sf::Color(255, 180, 80), 28);
        }
    }

    bool test_hit(const Hitbox& target) const override {
        if (!burst_active_ || burst_frame_ > 2) {
            return false;
        }
        return circle_intersects_hitbox(x_, y_, explosion_radius_, target);
    }

    bool on_contact(IDamageable& target) override {
        if (burst_active_ && burst_frame_ <= 2 && explosion_damage_ > 0) {
            const Hitbox hb = target.getHitbox();
            module2_fx::request_particles(
                hb.x + hb.w * 0.5f,
                hb.y + hb.h * 0.5f,
                sf::Color(255, 100, 40),
                12);
            return target.takeDamage(explosion_damage_);
        }
        return false;
    }

    void render(sf::RenderTarget& target) const override {
        if (burst_active_) {
            const float progress =
                static_cast<float>(burst_frame_) /
                static_cast<float>(burst_visual_frames_);
            for (int ring = 0; ring < 3; ++ring) {
                const float expand =
                    explosion_radius_ * (0.35f + progress * (0.45f + ring * 0.2f));
                sf::CircleShape wave(expand);
                wave.setOrigin(expand, expand);
                wave.setPosition(x_, y_);
                wave.setFillColor(sf::Color::Transparent);
                wave.setOutlineColor(sf::Color(
                    255,
                    220 - ring * 30,
                    120,
                    static_cast<sf::Uint8>(200 * (1.f - progress))));
                wave.setOutlineThickness(3.f - static_cast<float>(ring) * 0.5f);
                target.draw(wave);
            }
            sf::CircleShape core(explosion_radius_ * (0.25f + progress * 0.5f));
            core.setOrigin(core.getRadius(), core.getRadius());
            core.setPosition(x_, y_);
            core.setFillColor(sf::Color(255, 140, 60,
                static_cast<sf::Uint8>(120 * (1.f - progress))));
            target.draw(core);
            return;
        }

        const int countdown = fuse_frames_ - frame_counter_;
        const float pulse =
            1.f + 0.18f * std::sin(static_cast<float>(frame_counter_) * 0.35f);
        const float arm = crosshair_radius_ * pulse;

        sf::CircleShape range(explosion_radius_);
        range.setOrigin(explosion_radius_, explosion_radius_);
        range.setPosition(x_, y_);
        range.setFillColor(sf::Color(255, 60, 60, 35));
        range.setOutlineColor(sf::Color(255, 100, 100, 90));
        range.setOutlineThickness(1.5f);
        target.draw(range);

        sf::RectangleShape h(sf::Vector2f(arm * 2.f, 2.5f));
        sf::RectangleShape v(sf::Vector2f(2.5f, arm * 2.f));
        h.setOrigin(arm, 1.25f);
        v.setOrigin(1.25f, arm);
        h.setPosition(x_, y_);
        v.setPosition(x_, y_);
        h.setFillColor(sf::Color(255, 90, 110, 240));
        v.setFillColor(sf::Color(255, 90, 110, 240));
        target.draw(h);
        target.draw(v);

        sf::CircleShape center_dot(3.f);
        center_dot.setOrigin(3.f, 3.f);
        center_dot.setPosition(x_, y_);
        center_dot.setFillColor(sf::Color(255, 255, 255, 230));
        target.draw(center_dot);

        item_combat::draw_frame_count_digits(
            target,
            std::max(0, countdown),
            x_,
            y_ - 10.f);
    }

    const char* projectile_id() const override { return "epic_crosshair"; }

private:
    float x_;
    float y_;
    int   frame_counter_        = 0;
    int   fuse_frames_          = 60;
    bool  burst_active_         = false;
    int   burst_frame_          = 0;
    int   burst_visual_frames_  = 10;
    int   explosion_damage_;
    float explosion_radius_;
    float crosshair_radius_     = 14.f;
    bool  alive_                = true;
};

/** 冰块宝宝：立方体弹跳 + 冰晶拖尾，命中冰冻（模块一） */
class IceCubeBaby : public BaseProjectile {
public:
    IceCubeBaby(float x, float y, float vx, float vy, int damage)
        : x_(x), y_(y), vx_(vx), vy_(vy), damage_(damage) {}

    Hitbox getHitbox() const override {
        return hitbox_from_circle(x_, y_, radius_);
    }

    int  getDamage() const override { return damage_; }
    bool isAlive() const override { return alive_; }
    bool deals_direct_damage() const override { return false; }
    bool consumes_on_hit() const override { return false; }

    void update(float dt) override {
        prev_x_ = x_;
        prev_y_ = y_;
        x_ += vx_ * dt;
        y_ += vy_ * dt;
        rotation_ = std::atan2(vy_, vx_) * 57.2958f;
        crystal_angle_ += dt * 6.f;

        trail_.push_back(sf::Vector2f(x_, y_));
        if (trail_.size() > 22) {
            trail_.erase(trail_.begin());
        }

        bounce_screen_edges();
    }

    bool on_contact(IDamageable& target) override {
        module2_fx::request_particles(
            x_, y_, sf::Color(200, 240, 255), 16);
        module2_fx::request_particles(
            x_, y_, sf::Color(255, 255, 255), 8);
        target.applyFreezeHit(combat_fx::k_ice_baby_freeze_frames);
        reflect_from(target.getHitbox());
        return false;
    }

    void render(sf::RenderTarget& target) const override {
        module2_fx::draw_trail_line_strip(
            target, trail_, sf::Color(230, 245, 255, 120));

        for (int s = 0; s < 6; ++s) {
            const float ang = crystal_angle_ + static_cast<float>(s) * 1.047f;
            sf::CircleShape snow(1.8f);
            snow.setOrigin(1.8f, 1.8f);
            snow.setPosition(
                x_ + std::cos(ang) * (radius_ + 5.f),
                y_ + std::sin(ang) * (radius_ + 5.f));
            snow.setFillColor(sf::Color(255, 255, 255, 170));
            target.draw(snow);
        }

        sf::CircleShape glow(radius_ + 3.f);
        glow.setOrigin(glow.getRadius(), glow.getRadius());
        glow.setPosition(x_, y_);
        glow.setFillColor(sf::Color(160, 220, 255, 50));
        target.draw(glow);

        sf::ConvexShape cube;
        cube.setPointCount(4);
        const float hw = radius_ * 0.92f;
        cube.setPoint(0, sf::Vector2f(-hw, -hw));
        cube.setPoint(1, sf::Vector2f(hw, -hw));
        cube.setPoint(2, sf::Vector2f(hw, hw));
        cube.setPoint(3, sf::Vector2f(-hw, hw));
        cube.setOrigin(0.f, 0.f);
        cube.setPosition(x_, y_);
        cube.setRotation(rotation_);
        cube.setFillColor(sf::Color(150, 225, 255, 220));
        cube.setOutlineColor(sf::Color(230, 250, 255));
        cube.setOutlineThickness(2.f);
        target.draw(cube);

        sf::ConvexShape facet;
        facet.setPointCount(3);
        facet.setPoint(0, sf::Vector2f(-hw * 0.3f, -hw * 0.3f));
        facet.setPoint(1, sf::Vector2f(hw * 0.5f, -hw * 0.1f));
        facet.setPoint(2, sf::Vector2f(-hw * 0.1f, hw * 0.4f));
        facet.setPosition(x_, y_);
        facet.setRotation(rotation_);
        facet.setFillColor(sf::Color(255, 255, 255, 100));
        target.draw(facet);
    }

    const char* projectile_id() const override { return "ice_cube_baby"; }

private:
    void bounce_screen_edges() {
        const float margin = radius_;
        auto bounce_at = [this](float px, float py) {
            module2_fx::request_particles(
                px, py, sf::Color(200, 235, 255), 10);
            module2_fx::request_particles(
                px, py, sf::Color(255, 255, 255), 5);
        };
        if (x_ < margin) {
            x_ = margin;
            if (prev_x_ >= margin) {
                bounce_at(x_, y_);
            }
            vx_ = std::fabs(vx_);
        } else if (x_ > static_cast<float>(SCREEN_WIDTH) - margin) {
            x_ = static_cast<float>(SCREEN_WIDTH) - margin;
            if (prev_x_ <= static_cast<float>(SCREEN_WIDTH) - margin) {
                bounce_at(x_, y_);
            }
            vx_ = -std::fabs(vx_);
        }
        if (y_ < margin) {
            y_ = margin;
            if (prev_y_ >= margin) {
                bounce_at(x_, y_);
            }
            vy_ = std::fabs(vy_);
        } else if (y_ > static_cast<float>(SCREEN_HEIGHT) - margin) {
            y_ = static_cast<float>(SCREEN_HEIGHT) - margin;
            if (prev_y_ <= static_cast<float>(SCREEN_HEIGHT) - margin) {
                bounce_at(x_, y_);
            }
            vy_ = -std::fabs(vy_);
        }
    }

    void reflect_from(const Hitbox& target) {
        const float tcx = target.x + target.w * 0.5f;
        const float tcy = target.y + target.h * 0.5f;
        float nx = x_ - tcx;
        float ny = y_ - tcy;
        const float len = std::sqrt(nx * nx + ny * ny);
        if (len > 0.001f) {
            nx /= len;
            ny /= len;
        } else {
            nx = 0.f;
            ny = -1.f;
        }
        const float dot = vx_ * nx + vy_ * ny;
        vx_ = vx_ - 2.f * dot * nx;
        vy_ = vy_ - 2.f * dot * ny;
        x_ += nx * (radius_ + 4.f);
        y_ += ny * (radius_ + 4.f);
    }

    float x_;
    float y_;
    float prev_x_ = 0.f;
    float prev_y_ = 0.f;
    float vx_;
    float vy_;
    float rotation_       = 0.f;
    float crystal_angle_  = 0.f;
    int   damage_;
    float radius_         = 14.f;
    bool  alive_          = true;
    mutable std::vector<sf::Vector2f> trail_;
};

struct ProjectileHitCallbacks {
    std::function<void(Enemy&, int damage, bool killed)> on_enemy_hit;
    std::function<void(BaseBoss&, int damage, bool killed)> on_boss_hit;
    std::function<void(Bullet& bullet)> on_bullet_consumed;
};

class ItemManager {
public:
    void clear();

    void update(float dt);

    void spawn_passive(std::unique_ptr<BaseProjectile> projectile);

    /** 绘制所有存活被动投射物（数据层 render，非 ui_system） */
    void render_passives(sf::RenderTarget& target) const {
        for (const auto& projectile : passives_) {
            if (projectile && projectile->isAlive()) {
                projectile->render(target);
            }
        }
    }

    const std::vector<std::unique_ptr<BaseProjectile>>& passives() const {
        return passives_;
    }

    /**
     * 双层遍历：玩家 bullets + passives vs enemies + bosses。
     * 命中结算仅通过 IDamageable 多态接口，不 static_cast 具体敌/Boss 类。
     */
    void resolve_projectile_hits(
        std::vector<Bullet>& bullets,
        std::vector<Enemy>& enemies,
        BossSystem& boss_system,
        const Player& player,
        const ProjectileHitCallbacks& callbacks,
        std::vector<Bullet>& pending_bullets);

private:
    void cull_dead_passives();

    std::vector<std::unique_ptr<BaseProjectile>> passives_;
};

// ==================== 主循环集成（main.cpp 调用） ====================

namespace item_combat {

inline void sync_item_fields_from_player(
    const Player& player,
    PlayerStatsItemFields& fields)
{
    fields.mirror_clone_level = player.stats_ext.mirror_clone_level;
    fields.has_glass_shard    = player.stats_ext.has_glass_shard;
    fields.has_tiny_planet    = player.stats_ext.has_tiny_planet;
    fields.has_godhead        = player.stats_ext.has_godhead;
    fields.has_apple          = player.stats_ext.has_apple;
    fields.has_spike_nail     = player.stats_ext.has_spike_nail;
    fields.has_ice_baby       = player.stats_ext.has_ice_baby;
    fields.has_betrayal       = player.stats_ext.has_betrayal;
    fields.base_speed         = player.stats_ext.base_speed;
    fields.current_speed_multiplier = player.stats_ext.current_speed_multiplier;
    fields.sync_base_speed_from(player.stats);
}

inline void apply_player_move_speed(Player& player, PlayerStatsItemFields& fields) {
    sync_item_fields_from_player(player, fields);
    tick_player_item_fields(fields);
    player.stats_ext.current_speed_multiplier = fields.current_speed_multiplier;
    player.stats.speed = static_cast<double>(fields.getEffectiveSpeed());
}

inline void tick_enemies_combat(std::vector<Enemy>& enemies) {
    for (Enemy& enemy : enemies) {
        EnemyDamageable view(enemy);
        view.tick_status_effects();
    }
}

inline float collect_screen_shake(std::vector<Enemy>& enemies) {
    float strength = 0.f;
    for (Enemy& enemy : enemies) {
        EnemyDamageable view(enemy);
        if (view.consume_screen_shake_request()) {
            strength = std::max(strength, enemy.combat_screen_shake_strength);
        }
    }
    return strength;
}

inline void remove_dead_enemies(std::vector<Enemy>& enemies) {
    enemies.erase(
        std::remove_if(
            enemies.begin(),
            enemies.end(),
            [](const Enemy& enemy) { return enemy.hp <= 0; }),
        enemies.end());
}

/** 倒戈单位：寻找最近未倒戈敌人；若无则返回 false */
inline bool find_nearest_hostile_enemy(
    const std::vector<Enemy>& enemies,
    const Enemy&              self,
    float&                    out_x,
    float&                    out_y)
{
    float best_dist_sq = 1e12f;
    bool  found        = false;
    for (const Enemy& other : enemies) {
        if (other.hp <= 0 || &other == &self || other.combat_betrayed) {
            continue;
        }
        const float dx = other.x - self.x;
        const float dy = other.y - self.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < best_dist_sq) {
            best_dist_sq = d2;
            out_x        = other.x;
            out_y        = other.y;
            found        = true;
        }
    }
    return found;
}

/** 倒戈敌人瞄准：最近敌方单位，否则向上方攻击 */
inline void resolve_betrayed_enemy_aim(
    const std::vector<Enemy>& enemies,
    const Enemy&              self,
    float&                    aim_x,
    float&                    aim_y)
{
    if (find_nearest_hostile_enemy(enemies, self, aim_x, aim_y)) {
        return;
    }
    aim_x = self.x;
    aim_y = self.y - 240.f;
}

inline void mark_loyal_enemy_bullet(Bullet& bullet) {
    bullet.loyal_to_player = true;
    bullet.bullet_color      = combat_fx::k_betray_bullet_color;
}

/** 对敌人施加背叛道具效果（魅惑 → 倒戈） */
inline void apply_betrayal_effect(
    Enemy& enemy,
    int    frames = combat_fx::k_betrayal_effect_frames)
{
    EnemyDamageable view(enemy);
    view.applyBetrayalEffect(frames);
}

/** 玩家持有「背叛」道具时，投射物命中未倒戈敌人则触发魅惑 */
inline void try_apply_betrayal_on_hit(const Player& player, Enemy& enemy) {
    if (!player.stats_ext.has_betrayal || enemy.combat_betrayed) {
        return;
    }
    apply_betrayal_effect(enemy);
}

inline void draw_betray_glyph(
    sf::RenderTarget& target,
    float             x,
    float             y,
    float             size = 7.f)
{
    sf::CircleShape ring(size);
    ring.setOrigin(size, size);
    ring.setPosition(x, y);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineColor(sf::Color(160, 130, 255, 220));
    ring.setOutlineThickness(1.5f);
    target.draw(ring);

    for (int i = 0; i < 2; ++i) {
        const float angle = static_cast<float>(i) * 3.14159265f + 0.6f;
        sf::Vertex arc[] = {
            sf::Vertex(
                sf::Vector2f(
                    x + std::cos(angle) * size * 0.55f,
                    y + std::sin(angle) * size * 0.55f),
                sf::Color(180, 150, 255, 230)),
            sf::Vertex(
                sf::Vector2f(
                    x + std::cos(angle + 0.9f) * size * 0.95f,
                    y + std::sin(angle + 0.9f) * size * 0.95f),
                sf::Color(180, 150, 255, 230)),
            sf::Vertex(
                sf::Vector2f(
                    x + std::cos(angle + 1.15f) * size * 0.75f,
                    y + std::sin(angle + 1.15f) * size * 0.75f),
                sf::Color(180, 150, 255, 230)),
        };
        target.draw(arc, 3, sf::LineStrip);
    }
}

/** 八方向尖刺两波（八寸钉） */
inline void spawn_spike_nail_burst(
    Player& player,
    ItemManager& mgr,
    int   damage,
    float wave_speed   = 520.f,
    float spike_life   = 0.75f,
    int   wave_count   = 2,
    float wave_delay_s = 0.12f)
{
    (void)wave_delay_s;
    (void)wave_count;
    static const float k_pi = 3.14159265f;
    for (int wave = 0; wave < wave_count; ++wave) {
        const float phase = (wave_count > 1) ? static_cast<float>(wave) * 0.18f : 0.f;
        for (int i = 0; i < 8; ++i) {
            const float angle = phase + static_cast<float>(i) * (k_pi * 2.f / 8.f);
            const float vx    = std::cos(angle) * wave_speed;
            const float vy    = std::sin(angle) * wave_speed;
            mgr.spawn_passive(std::make_unique<SpikeProjectile>(
                player.pos.x,
                player.pos.y,
                vx,
                vy,
                damage,
                spike_life,
                player.pos.x,
                player.pos.y));
        }
    }
}

struct PassiveSpawnTimers {
    float glass_shard_cd = 0.f;
    float tiny_planet_cd = 0.f;
    float apple_cd       = 0.f;
    float spike_nail_cd  = 0.f;
    float ice_baby_cd    = 0.f;
};

/** 调试：不依赖 JSON，直接开启冰块宝宝 */
inline void enable_ice_baby(Player& player) {
    player.stats_ext.has_ice_baby = true;
}

/** 统计当前存活的冰块宝宝数量 */
inline int count_active_ice_babies(const ItemManager& mgr) {
    int count = 0;
    for (const auto& projectile : mgr.passives()) {
        if (projectile && projectile->isAlive()
            && std::string(projectile->projectile_id()) == "ice_cube_baby") {
            ++count;
        }
    }
    return count;
}

/** 从玩家位置发射一枚弹跳冰弹（冰块宝宝）；同屏最多 k_ice_baby_max_active 个 */
inline void spawn_ice_cube_baby(Player& player, ItemManager& mgr) {
    if (count_active_ice_babies(mgr) >= combat_fx::k_ice_baby_max_active) {
        return;
    }
    const float speed = 340.f;
    const float angle = -1.5707963f + std::sin(player.pos.x * 0.02f) * 0.35f;
    mgr.spawn_passive(std::make_unique<IceCubeBaby>(
        player.pos.x,
        player.pos.y - 18.f,
        std::cos(angle) * speed,
        std::sin(angle) * speed,
        0));
}

inline void update_passive_spawns(
    Player& player,
    ItemManager& mgr,
    PassiveSpawnTimers& timers,
    float dt)
{
    PlayerStatsItemFields fields;
    sync_item_fields_from_player(player, fields);
    const int dmg = player.get_damage();

    if (fields.has_tiny_planet) {
        timers.tiny_planet_cd -= dt;
        if (timers.tiny_planet_cd <= 0.f) {
            timers.tiny_planet_cd = 4.5f;
            mgr.spawn_passive(std::make_unique<OrbitingTear>(
                [&player]() { return player.pos; },
                28.f, 35.f, 3.5f, dmg));
        }
    }

    // 模块三：玻璃碎片 / 苹果刀片 改为玩家弹幕变异，不再额外生成被动投射物
    (void)timers.glass_shard_cd;
    (void)timers.apple_cd;

    if (fields.has_spike_nail) {
        timers.spike_nail_cd -= dt;
        if (timers.spike_nail_cd <= 0.f) {
            timers.spike_nail_cd = 1.35f;
            spawn_spike_nail_burst(player, mgr, dmg);
        }
    }

    if (fields.has_ice_baby) {
        timers.ice_baby_cd -= dt;
        if (timers.ice_baby_cd <= 0.f) {
            timers.ice_baby_cd = 2.8f;
            spawn_ice_cube_baby(player, mgr);
        }
    }
}

/** 血条下方状态行 Y（敌机中心坐标系；max_hp>1 时在血条下，否则在机体下） */
inline float enemy_status_row_y(const Enemy& enemy) {
    if (enemy.max_hp > 1) {
        // 血条顶 y-22、高 4px → 行心在 y-12
        return enemy.y - 12.f;
    }
    return enemy.y + static_cast<float>(enemy.height) * 0.5f + 8.f;
}

inline const sf::Font& combat_status_font() {
    static sf::Font font;
    static bool    loaded = false;
    static bool    tried  = false;
    if (!tried) {
        tried = true;
        loaded = font.loadFromFile("C:/Windows/Fonts/msyh.ttc")
                 || font.loadFromFile("C:/Windows/Fonts/simhei.ttf")
                 || font.loadFromFile("C:/Windows/Fonts/seguiemj.ttf");
    }
    return font;
}

inline bool combat_status_font_loaded() {
    static bool loaded = false;
    static bool tried  = false;
    if (!tried) {
        tried  = true;
        sf::Font probe;
        loaded = probe.loadFromFile("C:/Windows/Fonts/msyh.ttc")
                 || probe.loadFromFile("C:/Windows/Fonts/simhei.ttf");
    }
    return loaded;
}

inline void center_sf_text(sf::Text& text, float cx, float cy) {
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin(bounds.left + bounds.width * 0.5f, bounds.top);
    text.setPosition(cx, cy);
}

/** 矢量雪花（字体无 emoji 时的后备） */
inline void draw_snowflake_glyph(
    sf::RenderTarget& target,
    float cx,
    float cy,
    float radius)
{
    for (int arm = 0; arm < 6; ++arm) {
        const float angle =
            static_cast<float>(arm) * 1.047198f; // pi/3
        sf::RectangleShape ray(sf::Vector2f(radius * 2.f, 2.f));
        ray.setOrigin(radius, 1.f);
        ray.setPosition(cx, cy);
        ray.setRotation(angle * 57.2958f);
        ray.setFillColor(sf::Color(200, 240, 255, 230));
        target.draw(ray);
    }
}

/** 7 段数码管式数字（不依赖字体，保证倒计时可见） */
inline void draw_digit_glyph(
    sf::RenderTarget& target,
    int               digit,
    float             x,
    float             y,
    float             scale = 1.f)
{
    static const unsigned char segments[10][7] = {
        {1, 1, 1, 0, 1, 1, 1}, // 0
        {0, 0, 1, 0, 0, 1, 0}, // 1
        {1, 0, 1, 1, 1, 0, 1}, // 2
        {1, 0, 1, 1, 0, 1, 1}, // 3
        {0, 1, 1, 1, 0, 1, 0}, // 4
        {1, 1, 0, 1, 0, 1, 1}, // 5
        {1, 1, 0, 1, 1, 1, 1}, // 6
        {1, 0, 1, 0, 0, 1, 0}, // 7
        {1, 1, 1, 1, 1, 1, 1}, // 8
        {1, 1, 1, 1, 0, 1, 1}, // 9
    };
    if (digit < 0 || digit > 9) {
        return;
    }
    const float w = 2.f * scale;
    const float h = 1.2f * scale;
    const float gw = 6.f * scale;
    const float gh = 8.f * scale;
    const sf::Color c(210, 245, 255);
    const auto seg = [&](float sx, float sy, float sw, float sh) {
        sf::RectangleShape bar(sf::Vector2f(sw, sh));
        bar.setPosition(x + sx, y + sy);
        bar.setFillColor(c);
        target.draw(bar);
    };
    if (segments[digit][0]) seg(0.f, 0.f, gw, h);
    if (segments[digit][1]) seg(0.f, gh * 0.5f, h, gw * 0.45f);
    if (segments[digit][2]) seg(gw - h, gh * 0.5f, h, gw * 0.45f);
    if (segments[digit][3]) seg(0.f, gh - h, gw, h);
    if (segments[digit][4]) seg(0.f, 0.f, h, gw * 0.45f);
    if (segments[digit][5]) seg(gw - h, 0.f, h, gw * 0.45f);
    if (segments[digit][6]) seg(0.f, gh * 0.5f - h * 0.5f, gw, h);
}

inline void draw_frame_count_digits(
    sf::RenderTarget& target,
    int               frames,
    float             center_x,
    float             row_y)
{
    const int clamped = std::max(0, frames);
    std::string text  = std::to_string(clamped);
    const float digit_w = 8.f;
    float start_x = center_x + 6.f - text.size() * digit_w * 0.5f;
    for (char ch : text) {
        if (ch >= '0' && ch <= '9') {
            draw_digit_glyph(target, ch - '0', start_x, row_y + 1.f, 1.f);
        }
        start_x += digit_w;
    }
}

inline void draw_combat_status_icons(
    sf::RenderTarget& target,
    float center_x,
    float row_y,
    combat_status_icon icons,
    int freeze_frames_remaining = 0)
{
    if (icons == combat_status_icon::none && freeze_frames_remaining <= 0) {
        return;
    }

    float cursor_x = center_x - 22.f;

    if ((icons & combat_status_icon::frozen) != combat_status_icon::none
        || freeze_frames_remaining > 0) {
        const int frames = std::max(0, freeze_frames_remaining);

        // 背景条提高对比度（避免被冰弹/弹幕盖住后看不清）
        sf::RectangleShape bg(sf::Vector2f(52.f, 14.f));
        bg.setOrigin(26.f, 0.f);
        bg.setPosition(center_x + 2.f, row_y - 1.f);
        bg.setFillColor(sf::Color(20, 40, 80, 170));
        bg.setOutlineColor(sf::Color(120, 200, 255, 200));
        bg.setOutlineThickness(1.f);
        target.draw(bg);

        draw_snowflake_glyph(target, center_x - 20.f, row_y + 8.f, 6.f);
        draw_frame_count_digits(target, frames, center_x + 4.f, row_y);

        cursor_x = center_x + 34.f;
    }

    const sf::Font& font = combat_status_font();
    const bool font_ok   = combat_status_font_loaded();

    if ((icons & combat_status_icon::betrayed) != combat_status_icon::none) {
        if (font_ok) {
            sf::Text betray_text(std::wstring(1, wchar_t(0x21BB)), font, 14);
            betray_text.setFillColor(sf::Color(180, 140, 255));
            center_sf_text(betray_text, cursor_x, row_y);
            target.draw(betray_text);
        } else {
            draw_betray_glyph(target, cursor_x, row_y + 6.f, 6.f);
        }
        cursor_x += 22.f;
    }

    if ((icons & combat_status_icon::knockback) != combat_status_icon::none) {
        sf::CircleShape gust(5.f);
        gust.setOrigin(5.f, 5.f);
        gust.setPosition(cursor_x, row_y + 6.f);
        gust.setFillColor(sf::Color(200, 230, 255, 200));
        target.draw(gust);
    }
}

/** 血条下方状态行（须在 render_passives 之后调用，保证在最上层） */
inline void draw_enemy_status_row(
    sf::RenderTarget& target,
    const Enemy&      enemy)
{
    if (enemy.combat_freeze_frames <= 0
        && !enemy.combat_betrayed
        && enemy.combat_knockback_trail <= 0) {
        return;
    }
    EnemyDamageable view(const_cast<Enemy&>(enemy));
    draw_combat_status_icons(
        target,
        enemy.x,
        enemy_status_row_y(enemy),
        view.combat_status_icons(),
        view.freeze_frames_remaining());
}

inline void draw_freeze_combat_fx(
    sf::RenderTarget& target,
    const Enemy& enemy,
    const combat_visual_state& v,
    int frame_count)
{
    if (!v.freeze_active && v.freeze_hit_burst <= 0.01f) {
        return;
    }

    const float body_r =
        static_cast<float>(enemy.width) * 0.55f + 6.f;

    // 命中瞬间：冰花爆开
    if (v.freeze_hit_burst > 0.01f) {
        const float burst_r = body_r + 8.f + 28.f * v.freeze_hit_burst;
        sf::CircleShape burst_core(burst_r * 0.35f);
        burst_core.setOrigin(burst_core.getRadius(), burst_core.getRadius());
        burst_core.setPosition(enemy.x, enemy.y);
        burst_core.setFillColor(sf::Color(220, 250, 255,
            static_cast<sf::Uint8>(160 * v.freeze_hit_burst)));
        target.draw(burst_core);

        for (int petal = 0; petal < 10; ++petal) {
            const float angle =
                static_cast<float>(petal) * 0.628318f
                + static_cast<float>(frame_count) * 0.04f;
            const float pr = burst_r * (0.55f + 0.45f * v.freeze_hit_burst);
            sf::CircleShape flake(2.5f + v.freeze_hit_burst * 2.f);
            flake.setOrigin(flake.getRadius(), flake.getRadius());
            flake.setPosition(
                enemy.x + std::cos(angle) * pr,
                enemy.y + std::sin(angle) * pr);
            flake.setFillColor(sf::Color(240, 250, 255,
                static_cast<sf::Uint8>(200 * v.freeze_hit_burst)));
            target.draw(flake);
        }
    }

    if (!v.freeze_active) {
        return;
    }

    // 半透明冰层（双层）
    const float layer_alpha = v.freeze_ice_layer_alpha;
    sf::CircleShape ice_outer(body_r + 6.f);
    ice_outer.setOrigin(ice_outer.getRadius(), ice_outer.getRadius());
    ice_outer.setPosition(enemy.x, enemy.y);
    ice_outer.setFillColor(sf::Color(160, 220, 255,
        static_cast<sf::Uint8>(50 + 90 * layer_alpha)));
    ice_outer.setOutlineColor(sf::Color(230, 245, 255,
        static_cast<sf::Uint8>(100 + 80 * layer_alpha)));
    ice_outer.setOutlineThickness(2.f);
    target.draw(ice_outer);

    sf::CircleShape ice_inner(body_r + 2.f);
    ice_inner.setOrigin(ice_inner.getRadius(), ice_inner.getRadius());
    ice_inner.setPosition(enemy.x, enemy.y);
    ice_inner.setFillColor(sf::Color(200, 240, 255,
        static_cast<sf::Uint8>(70 + 110 * layer_alpha)));
    ice_inner.setOutlineColor(sf::Color(255, 255, 255,
        static_cast<sf::Uint8>(60 + 100 * layer_alpha)));
    ice_inner.setOutlineThickness(1.f);
    target.draw(ice_inner);

    // 冰花闪烁粒子（绕体）
    const int sparkle_count = 6;
    for (int i = 0; i < sparkle_count; ++i) {
        const float phase =
            static_cast<float>(frame_count) * 0.11f
            + static_cast<float>(i) * 1.047198f;
        const float flicker = 0.45f + 0.55f * std::sin(phase * 2.3f);
        const float orbit_r = body_r + 4.f + std::sin(phase) * 3.f;
        sf::CircleShape sparkle(2.f + flicker * 1.5f);
        sparkle.setOrigin(sparkle.getRadius(), sparkle.getRadius());
        sparkle.setPosition(
            enemy.x + std::cos(phase) * orbit_r,
            enemy.y + std::sin(phase) * orbit_r * 0.85f);
        sparkle.setFillColor(sf::Color(255, 255, 255,
            static_cast<sf::Uint8>(180 * flicker * layer_alpha)));
        target.draw(sparkle);
    }

    // 周围飘落细雪
    const int snow_count =
        static_cast<int>(4 + v.freeze_snow_density * 6.f);
    for (int s = 0; s < snow_count; ++s) {
        const float seed = enemy.x * 0.17f + enemy.y * 0.23f
                           + static_cast<float>(s) * 2.71f;
        const float drift =
            std::sin(static_cast<float>(frame_count) * 0.04f + seed) * 22.f;
        const float fall =
            std::fmod(
                static_cast<float>(frame_count) * 0.9f + seed * 17.f,
                48.f);
        sf::CircleShape snow(1.2f + std::fmod(seed, 1.5f));
        snow.setOrigin(snow.getRadius(), snow.getRadius());
        snow.setPosition(
            enemy.x + drift + std::cos(seed) * 18.f,
            enemy.y - 28.f + fall);
        snow.setFillColor(sf::Color(240, 248, 255,
            static_cast<sf::Uint8>(120 + 80 * layer_alpha)));
        target.draw(snow);
    }

    // 受击碎裂闪（短暂裂纹后重新凝结）
    if (v.freeze_shatter_flash > 0.01f) {
        for (int crack = 0; crack < 7; ++crack) {
            const float angle =
                static_cast<float>(crack) * 0.897598f
                + enemy.x * 0.01f;
            const float len = body_r * (0.55f + 0.55f * v.freeze_shatter_flash);
            sf::Vertex shatter[] = {
                sf::Vertex(
                    sf::Vector2f(enemy.x, enemy.y),
                    sf::Color(255, 255, 255,
                        static_cast<sf::Uint8>(240 * v.freeze_shatter_flash))),
                sf::Vertex(
                    sf::Vector2f(
                        enemy.x + std::cos(angle) * len,
                        enemy.y + std::sin(angle) * len),
                    sf::Color(200, 240, 255, 0)),
            };
            target.draw(shatter, 2, sf::Lines);
        }
        sf::CircleShape shatter_ring(body_r + 8.f * v.freeze_shatter_flash);
        shatter_ring.setOrigin(shatter_ring.getRadius(), shatter_ring.getRadius());
        shatter_ring.setPosition(enemy.x, enemy.y);
        shatter_ring.setFillColor(sf::Color::Transparent);
        shatter_ring.setOutlineColor(sf::Color(255, 255, 255,
            static_cast<sf::Uint8>(220 * v.freeze_shatter_flash)));
        shatter_ring.setOutlineThickness(3.f);
        target.draw(shatter_ring);
    }

    // 即将解除：裂纹闪烁
    if (v.freeze_thaw_warning && v.freeze_crack_pulse > 0.01f) {
        const sf::Uint8 crack_a =
            static_cast<sf::Uint8>(80 + 140 * v.freeze_crack_pulse);
        for (int c = 0; c < 4; ++c) {
            const float angle =
                static_cast<float>(c) * 1.5707963f + 0.35f;
            const float len = body_r * 0.95f;
            sf::Vertex crack[] = {
                sf::Vertex(
                    sf::Vector2f(
                        enemy.x + std::cos(angle) * body_r * 0.2f,
                        enemy.y + std::sin(angle) * body_r * 0.2f),
                    sf::Color(180, 220, 255, crack_a)),
                sf::Vertex(
                    sf::Vector2f(
                        enemy.x + std::cos(angle + 0.25f) * len,
                        enemy.y + std::sin(angle + 0.25f) * len),
                    sf::Color(255, 255, 255, crack_a / 2)),
            };
            target.draw(crack, 2, sf::Lines);
        }
    }
}

inline void draw_enemy_combat_overlay(
    sf::RenderTarget& target,
    const Enemy& enemy,
    int frame_count = 0)
{
    EnemyDamageable view(const_cast<Enemy&>(enemy));
    const combat_visual_state v = view.get_combat_visual();

    const float hw = static_cast<float>(enemy.width) * 0.5f;
    const float hh = static_cast<float>(enemy.height) * 0.5f;

    for (int gi = 0; gi < enemy.combat_ghost_count; ++gi) {
        const float age_fade =
            1.f - static_cast<float>(gi) * 0.22f;
        const float alpha =
            enemy.combat_ghost_alpha[gi] * v.knockback_trail * age_fade;
        if (alpha < 0.03f) {
            continue;
        }
        sf::RectangleShape ghost(sf::Vector2f(hw * 2.f, hh * 2.f));
        ghost.setOrigin(hw, hh);
        ghost.setPosition(enemy.combat_ghost_x[gi], enemy.combat_ghost_y[gi]);
        ghost.setFillColor(enemy.color);
        ghost.setOutlineColor(sf::Color(255, 255, 255,
            static_cast<sf::Uint8>(80 * alpha)));
        ghost.setOutlineThickness(1.f);
        sf::Color fill = enemy.color;
        fill.a = static_cast<sf::Uint8>(255.f * std::min(1.f, alpha));
        ghost.setFillColor(fill);
        target.draw(ghost);
    }

    if (v.knockback_trail > 0.05f && enemy.combat_ghost_count >= 2) {
        for (int gi = 0; gi < enemy.combat_ghost_count - 1; ++gi) {
            sf::Vertex line[] = {
                sf::Vertex(
                    sf::Vector2f(enemy.combat_ghost_x[gi], enemy.combat_ghost_y[gi]),
                    sf::Color(255, 255, 255,
                        static_cast<sf::Uint8>(40 * v.knockback_trail))),
                sf::Vertex(
                    sf::Vector2f(enemy.combat_ghost_x[gi + 1],
                                 enemy.combat_ghost_y[gi + 1]),
                    sf::Color(255, 255, 255, 0)),
            };
            target.draw(line, 2, sf::Lines);
        }
        sf::Vertex to_body[] = {
            sf::Vertex(
                sf::Vector2f(
                    enemy.combat_ghost_x[enemy.combat_ghost_count - 1],
                    enemy.combat_ghost_y[enemy.combat_ghost_count - 1]),
                sf::Color(255, 255, 255,
                    static_cast<sf::Uint8>(30 * v.knockback_trail))),
            sf::Vertex(
                sf::Vector2f(enemy.x, enemy.y),
                sf::Color(255, 255, 255, 0)),
        };
        target.draw(to_body, 2, sf::Lines);
    }

    if (v.betray_active) {
        const float halo_y = enemy.y - 24.f;

        sf::CircleShape halo(20.f);
        halo.setOrigin(20.f, 20.f);
        halo.setPosition(enemy.x, halo_y);
        halo.setFillColor(sf::Color::Transparent);
        halo.setOutlineColor(sf::Color(120, 160, 255, 110));
        halo.setOutlineThickness(2.5f);
        halo.setRotation(v.betray_halo_angle * 57.3f);
        target.draw(halo);

        sf::CircleShape halo_inner(13.f);
        halo_inner.setOrigin(13.f, 13.f);
        halo_inner.setPosition(enemy.x, halo_y);
        halo_inner.setFillColor(sf::Color::Transparent);
        halo_inner.setOutlineColor(sf::Color(180, 200, 255, 90));
        halo_inner.setOutlineThickness(1.5f);
        halo_inner.setRotation(-v.betray_halo_angle * 72.f);
        target.draw(halo_inner);

        if (v.betray_particle_alpha > 0.02f) {
            for (int p = 0; p < 8; ++p) {
                const float angle =
                    v.betray_halo_angle * 1.8f
                    + static_cast<float>(p) * 0.785398f;
                const float dist =
                    12.f + 10.f * std::sin(
                        static_cast<float>(frame_count) * 0.08f + p * 0.7f);
                sf::CircleShape particle(1.5f + static_cast<float>(p) * 0.15f);
                particle.setOrigin(particle.getRadius(), particle.getRadius());
                particle.setPosition(
                    enemy.x + std::cos(angle) * dist,
                    enemy.y + std::sin(angle) * dist * 0.65f - 6.f);
                particle.setFillColor(sf::Color(
                    130, 180, 255,
                    static_cast<sf::Uint8>(
                        180.f * v.betray_particle_alpha)));
                target.draw(particle);
            }
        }
    }

    draw_freeze_combat_fx(target, enemy, v, frame_count);

    if (v.knockback_shockwave > 0.01f) {
        const float expand = 12.f + 52.f * (1.f - v.knockback_shockwave);
        sf::CircleShape wave(expand);
        wave.setOrigin(expand, expand);
        wave.setPosition(enemy.combat_shockwave_x, enemy.combat_shockwave_y);
        wave.setFillColor(sf::Color::Transparent);
        wave.setOutlineColor(sf::Color(255, 240, 200,
            static_cast<sf::Uint8>(220 * v.knockback_shockwave)));
        wave.setOutlineThickness(3.f);
        target.draw(wave);

        const float inner_expand = 6.f + 28.f * (1.f - v.knockback_shockwave);
        sf::CircleShape inner_wave(inner_expand);
        inner_wave.setOrigin(inner_expand, inner_expand);
        inner_wave.setPosition(enemy.combat_shockwave_x, enemy.combat_shockwave_y);
        inner_wave.setFillColor(sf::Color::Transparent);
        inner_wave.setOutlineColor(sf::Color(255, 255, 255,
            static_cast<sf::Uint8>(160 * v.knockback_shockwave)));
        inner_wave.setOutlineThickness(2.f);
        target.draw(inner_wave);

        sf::CircleShape flash(8.f + 14.f * v.knockback_shockwave);
        flash.setOrigin(flash.getRadius(), flash.getRadius());
        flash.setPosition(enemy.combat_shockwave_x, enemy.combat_shockwave_y);
        flash.setFillColor(sf::Color(255, 255, 255,
            static_cast<sf::Uint8>(110 * v.knockback_shockwave)));
        target.draw(flash);
    }
}

inline void spawn_demo_module2_projectiles(Player& player, ItemManager& mgr) {
    const int dmg = player.get_damage();
    mgr.spawn_passive(std::make_unique<OrbitingTear>(
        [&player]() { return player.pos; },
        20.f, 40.f, 4.f, dmg));
    mgr.spawn_passive(std::make_unique<LaserRing>(
        player.pos.x, player.pos.y, -280.f, 22.f, dmg));
    mgr.spawn_passive(std::make_unique<EpicCrosshair>(
        player.pos.x + 80.f, player.pos.y - 180.f, dmg + 4, 90.f));
    mgr.spawn_passive(std::make_unique<IceCubeBaby>(
        player.pos.x,
        player.pos.y,
        300.f,
        -260.f,
        0));
}

} // namespace item_combat

#endif
