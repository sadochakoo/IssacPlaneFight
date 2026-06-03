/*
 * damageable.h - 可受击实体标准接口（碰撞箱 + 伤害结算）
 */

#ifndef DAMAGEABLE_H
#define DAMAGEABLE_H

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

class IDamageable {
public:
    virtual ~IDamageable() = default;

    virtual Hitbox getHitbox() const = 0;
    virtual bool   isAlive() const = 0;

    /** @return true 表示实体已被消灭，应从战场移除 */
    virtual bool takeDamage(int damage) = 0;
};

#endif
