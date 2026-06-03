/*
 * boss_system.h - Boss 逻辑（不依赖玩家道具模块）
 */

#ifndef BOSS_SYSTEM_H
#define BOSS_SYSTEM_H

#include "damageable.h"
#include <memory>
#include <vector>

class BaseBoss : public IDamageable {
public:
    virtual ~BaseBoss() = default;

    virtual void update(float dt) = 0;

    float x() const { return x_; }
    float y() const { return y_; }
    int   hp() const { return hp_; }
    int   max_hp() const { return max_hp_; }
    int   score_value() const { return score_value_; }

protected:
    float x_ = 400.f;
    float y_ = 120.f;
    int   hp_ = 1;
    int   max_hp_ = 1;
    int   width_ = 80;
    int   height_ = 80;
    int   score_value_ = 500;
};

/** 默认层 Boss：矩形碰撞，无道具特判 */
class LayerBoss : public BaseBoss {
public:
    LayerBoss(float x, float y, int hp, int score);

    void update(float dt) override;

    Hitbox getHitbox() const override;
    bool   isAlive() const override;
    bool   takeDamage(int damage) override;

private:
    float vx_ = 40.f;
};

class BossSystem {
public:
    void clear();
    void update(float dt);

    void add_boss(std::unique_ptr<BaseBoss> boss);

    const std::vector<std::unique_ptr<BaseBoss>>& bosses() const { return bosses_; }

    /** 本帧有效的非拥有指针，仅在未修改 bosses_ 容器期间使用 */
    std::vector<IDamageable*> damageable_view();

    void remove_dead_bosses();

private:
    std::vector<std::unique_ptr<BaseBoss>> bosses_;
    std::vector<IDamageable*>              damageable_cache_;
};

#endif
