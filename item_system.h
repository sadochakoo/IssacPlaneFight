/*
 * item_system.h - 被动道具投射物 + 与 IDamageable 的碰撞结算
 */

#ifndef ITEM_SYSTEM_H
#define ITEM_SYSTEM_H

#include "damageable.h"
#include "boss_system.h"
#include <functional>
#include <memory>
#include <vector>

class Player;
struct Bullet;
struct Enemy;

/** 敌人 → IDamageable 适配（不持有道具逻辑） */
class EnemyDamageable : public IDamageable {
public:
    explicit EnemyDamageable(Enemy& enemy);

    Hitbox getHitbox() const override;
    bool   isAlive() const override;
    bool   takeDamage(int damage) override;

    Enemy& enemy() { return enemy_; }

private:
    Enemy& enemy_;
};

class BaseProjectile {
public:
    virtual ~BaseProjectile() = default;

    virtual Hitbox getHitbox() const = 0;
    virtual int    getDamage() const = 0;
    virtual bool   isAlive() const = 0;
    virtual void   update(float dt) = 0;

    /** 命中后是否应从 ItemManager 移除 */
    virtual bool consumes_on_hit() const { return true; }
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

class SpikeProjectile : public BaseProjectile {
public:
    SpikeProjectile(float x, float y, float vx, float vy, int damage, float life);

    Hitbox getHitbox() const override;
    int    getDamage() const override;
    bool   isAlive() const override;
    void   update(float dt) override;

private:
    float x_, y_, vx_, vy_, life_, max_life_;
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

    const std::vector<std::unique_ptr<BaseProjectile>>& passives() const {
        return passives_;
    }

    /**
     * 双层遍历：玩家 bullets + passives vs enemies + bosses。
     * 仅修改数据层，不涉及渲染。
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

#endif
