/*
 * damageable.h - 可受击实体标准接口（碰撞箱 + 伤害结算 + 场控状态）
 *
 * 阵营/场控仅通过本接口与外部交互；禁止对 Enemy / Boss 做 static_cast。
 * 界面层读取 combat_visual_state 渲染光环/冰晶/残影/图标（不修改 ui_system 文件）。
 *
 * 本文件与 item_system.h 为道具/场控域唯一扩展入口；其它 .cpp 勿重复实现接口。
 */

#ifndef DAMAGEABLE_H
#define DAMAGEABLE_H

#include <cmath>
#include <vector>

struct Hitbox {
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
};

inline bool hitbox_intersects(const Hitbox& a, const Hitbox& b) {
    return (a.x < b.x + b.w && a.x + a.w > b.x &&
            a.y < b.y + b.h && a.y + a.h > b.y);
}

inline Hitbox hitbox_from_circle(float cx, float cy, float radius) {
    return Hitbox{cx - radius, cy - radius, radius * 2.f, radius * 2.f};
}

inline bool circle_intersects_hitbox(
    float cx, float cy, float radius, const Hitbox& target)
{
    const float tcx = target.x + target.w * 0.5f;
    const float tcy = target.y + target.h * 0.5f;
    const float dx = tcx - cx;
    const float dy = tcy - cy;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float target_r =
        0.5f * std::sqrt(target.w * target.w + target.h * target.h);
    return dist <= radius + target_r;
}

inline bool ring_intersects_hitbox(
    float cx, float cy, float outer_radius, float thickness,
    const Hitbox& target)
{
    const float tcx = target.x + target.w * 0.5f;
    const float tcy = target.y + target.h * 0.5f;
    const float dx = tcx - cx;
    const float dy = tcy - cy;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float target_r =
        0.5f * std::sqrt(target.w * target.w + target.h * target.h);
    const float inner = outer_radius - thickness;
    if (inner < 0.f) {
        return dist <= outer_radius + target_r;
    }
    return (dist + target_r >= inner) && (dist - target_r <= outer_radius);
}

enum class Faction {
    player,
    enemy
};

/** 血条下方状态图标位掩码 */
enum class combat_status_icon : unsigned {
    none      = 0,
    frozen    = 1u << 0,  // ❄️
    betrayed  = 1u << 1,  // 🔄
    knockback = 1u << 2,  // 💨
};

inline combat_status_icon operator|(
    combat_status_icon a, combat_status_icon b)
{
    return static_cast<combat_status_icon>(
        static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

inline combat_status_icon operator&(
    combat_status_icon a, combat_status_icon b)
{
    return static_cast<combat_status_icon>(
        static_cast<unsigned>(a) & static_cast<unsigned>(b));
}

inline bool has_combat_status_icon(
    combat_status_icon flags, combat_status_icon bit)
{
    return (static_cast<unsigned>(flags) & static_cast<unsigned>(bit)) != 0;
}

/** 击退参数：径向推开 + 硬直 + 可选震屏 */
struct knockback_params {
    float origin_x       = 0.f;
    float origin_y       = 0.f;
    float distance       = 0.f;   // 0 表示在 [min,max] 内取默认
    float distance_min   = 80.f;
    float distance_max   = 150.f;
    int   stun_frames    = 12;    // 约 0.2s @60fps
    bool  screen_shake   = true;
    float screen_shake_strength = 9.f; // 震屏幅度（像素），0 关闭
};

/**
 * 场控视觉效果快照（供渲染层每帧读取，不含具体绘制代码）。
 */
struct combat_visual_state {
    // --- 倒戈 / 背叛 ---
    bool  betray_active           = false;
    float betray_halo_angle       = 0.f;   // 头顶蓝光环旋转角
    float betray_particle_alpha   = 0.f;   // 蓝色粒子飘散 0~1
    bool  bullets_player_tint     = false; // 弹幕改蓝色（效忠玩家）
    float body_tint_r             = 255.f;
    float body_tint_g             = 0.f;
    float body_tint_b             = 0.f;

    // --- 冰冻 ---
    bool  freeze_active           = false;
    int   freeze_frames_left      = 0;     // 血条下 ❄️ 倒计时
    float freeze_ice_layer_alpha  = 0.f;   // 半透明冰层
    float freeze_shatter_flash    = 0.f;   // 受击碎裂闪 0~1
    float freeze_hit_burst        = 0.f;   // 命中冰花爆开
    float freeze_crack_pulse      = 0.f;   // 即将解除时裂纹闪烁
    bool  freeze_thaw_warning     = false; // 解除前警告
    float freeze_snow_density     = 0.f;   // 周围雪花密度 0~1

    // --- 击退（八寸钉）---
    float knockback_trail         = 0.f;   // 拖尾残影强度 0~1
    float knockback_shockwave     = 0.f;   // 冲击波环 0~1
    int   knockback_stun_frames   = 0;     // 硬直，无法攻击
    bool  request_screen_shake    = false;
    std::vector<float> knockback_ghost_x;
    std::vector<float> knockback_ghost_y; // 2~3 个残像位置
};

class IDamageable {
public:
    virtual ~IDamageable() = default;

    virtual Hitbox getHitbox() const = 0;
    virtual bool   isAlive() const = 0;
    virtual bool   takeDamage(int damage) = 0;

    // ---------- 状态查询 ----------
    virtual Faction getFaction() const { return Faction::enemy; }
    virtual bool    isFrozen() const { return false; }
    virtual bool    can_move() const { return !isFrozen(); }
    virtual bool    can_shoot() const {
        return !isFrozen() && knockback_stun_remaining() <= 0;
    }

    /** 倒戈期间不与玩家发生伤害性碰撞 */
    virtual bool hurts_player_on_contact() const {
        return getFaction() != Faction::player;
    }

    virtual combat_status_icon combat_status_icons() const {
        return combat_status_icon::none;
    }

    virtual int freeze_frames_remaining() const { return 0; }
    virtual int knockback_stun_remaining() const { return 0; }
    virtual float knockback_trail_intensity() const { return 0.f; }

    /** 界面专用：本帧应绘制的场控特效参数 */
    virtual combat_visual_state get_combat_visual() const {
        return combat_visual_state{};
    }

    // ---------- 状态施加 ----------
    virtual void setFaction(Faction f) { (void)f; }

    virtual void applyFreeze(int frames) { (void)frames; }

    /** 冰冻弹命中瞬间：冰花爆开 + 进入冻结 */
    virtual void applyFreezeHit(int frames) {
        applyFreeze(frames);
    }

    virtual void applyKnockback(float distance) { (void)distance; }

    /** 八寸钉：沿 origin→实体 径向推开 distance（80~150），硬直 + 残影 */
    virtual void applyKnockbackRadial(const knockback_params& params) {
        applyKnockback(params.distance);
    }

    /** 背叛道具效果：魅惑并倒戈，持续 frames 帧（非独立道具） */
    virtual void applyBetrayalEffect(int frames) { (void)frames; }

    /** 消费本帧震屏请求（读后复位） */
    virtual bool consume_screen_shake_request() { return false; }
};

#endif
