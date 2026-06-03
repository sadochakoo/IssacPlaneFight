#include "boss_system.h"
#include <algorithm>
#include <cmath>

LayerBoss::LayerBoss(float x, float y, int hp, int score) {
    x_ = x;
    y_ = y;
    hp_ = hp;
    max_hp_ = hp;
    score_value_ = score;
}

void LayerBoss::update(float dt) {
    x_ += vx_ * dt;
    if (x_ < 60.f) {
        x_ = 60.f;
        vx_ = std::fabs(vx_);
    } else if (x_ > 740.f) {
        x_ = 740.f;
        vx_ = -std::fabs(vx_);
    }
}

Hitbox LayerBoss::getHitbox() const {
    return Hitbox{
        x_ - static_cast<float>(width_) * 0.5f,
        y_ - static_cast<float>(height_) * 0.5f,
        static_cast<float>(width_),
        static_cast<float>(height_)};
}

bool LayerBoss::isAlive() const {
    return hp_ > 0;
}

bool LayerBoss::takeDamage(int damage) {
    if (hp_ <= 0) {
        return true;
    }
    hp_ -= damage;
    return hp_ <= 0;
}

void BossSystem::clear() {
    bosses_.clear();
    damageable_cache_.clear();
}

void BossSystem::update(float dt) {
    for (auto& boss : bosses_) {
        if (boss && boss->isAlive()) {
            boss->update(dt);
        }
    }
    remove_dead_bosses();
}

void BossSystem::add_boss(std::unique_ptr<BaseBoss> boss) {
    if (boss) {
        bosses_.push_back(std::move(boss));
    }
}

std::vector<IDamageable*> BossSystem::damageable_view() {
    damageable_cache_.clear();
    damageable_cache_.reserve(bosses_.size());
    for (auto& boss : bosses_) {
        if (boss && boss->isAlive()) {
            damageable_cache_.push_back(boss.get());
        }
    }
    return damageable_cache_;
}

void BossSystem::remove_dead_bosses() {
    bosses_.erase(
        std::remove_if(
            bosses_.begin(),
            bosses_.end(),
            [](const std::unique_ptr<BaseBoss>& boss) {
                return !boss || !boss->isAlive();
            }),
        bosses_.end());
}
