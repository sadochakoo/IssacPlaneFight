#include "item_system.h"
#include "player_stats.h"
#include "parasite_bullet.h"

#include <algorithm>
#include <cmath>

namespace {

bool bullet_participates_in_combat(const Bullet& b) {
    return !b.is_haemolacria_orb && !b.is_dead;
}

int compute_bullet_damage(const Bullet& bullet, int player_damage_fallback) {
    float hit_damage = bullet.damage;
    if (hit_damage <= 0.f) {
        hit_damage = static_cast<float>(player_damage_fallback);
    }
    return std::max(1, static_cast<int>(std::ceil(hit_damage)));
}

bool resolve_one_hit(
    BaseProjectile& projectile,
    Bullet* bullet_ptr,
    IDamageable& target,
    const ProjectileHitCallbacks& callbacks,
    EnemyDamageable* enemy_view,
    BaseBoss* boss_view,
    std::vector<Bullet>& pending_bullets)
{
    if (!projectile.isAlive() || !target.isAlive()) {
        return false;
    }
    if (!hitbox_intersects(projectile.getHitbox(), target.getHitbox())) {
        return false;
    }

    const int dmg = projectile.getDamage();
    const bool killed = target.takeDamage(dmg);

    if (enemy_view != nullptr) {
        if (callbacks.on_enemy_hit) {
            callbacks.on_enemy_hit(enemy_view->enemy(), dmg, killed);
        }
        if (bullet_ptr != nullptr) {
            bullet_ptr->is_dead = true;
            if (callbacks.on_bullet_consumed) {
                callbacks.on_bullet_consumed(*bullet_ptr);
            }
            if (can_parasite_split(*bullet_ptr)) {
                enqueue_parasite_hit_splits(*bullet_ptr, pending_bullets);
            }
        }
    } else if (boss_view != nullptr) {
        if (callbacks.on_boss_hit) {
            callbacks.on_boss_hit(*boss_view, dmg, killed);
        }
    }

    return bullet_ptr != nullptr || projectile.consumes_on_hit();
}

} // namespace

// ---------- EnemyDamageable ----------

EnemyDamageable::EnemyDamageable(Enemy& enemy)
    : enemy_(enemy) {}

Hitbox EnemyDamageable::getHitbox() const {
    return Hitbox{
        enemy_.x - 15.f,
        enemy_.y - 15.f,
        30.f,
        30.f};
}

bool EnemyDamageable::isAlive() const {
    return enemy_.hp > 0;
}

bool EnemyDamageable::takeDamage(int damage) {
    if (enemy_.hp <= 0) {
        return true;
    }
    enemy_.hp -= damage;
    return enemy_.hp <= 0;
}

// ---------- BulletProjectile ----------

BulletProjectile::BulletProjectile(Bullet& bullet, int player_damage_fallback)
    : bullet_(bullet), player_damage_fallback_(player_damage_fallback) {}

Hitbox BulletProjectile::getHitbox() const {
    const float r = bullet_.radius;
    return Hitbox{
        bullet_.x - r,
        bullet_.y - r,
        r * 2.f,
        r * 2.f};
}

int BulletProjectile::getDamage() const {
    return compute_bullet_damage(bullet_, player_damage_fallback_);
}

bool BulletProjectile::isAlive() const {
    return bullet_participates_in_combat(bullet_) && bullet_.life > 0.f;
}

void BulletProjectile::update(float dt) {
    (void)dt;
}

// ---------- SpikeProjectile ----------

SpikeProjectile::SpikeProjectile(
    float x, float y, float vx, float vy, int damage, float life)
    : x_(x), y_(y), vx_(vx), vy_(vy), damage_(damage), life_(life), max_life_(life) {}

Hitbox SpikeProjectile::getHitbox() const {
    return Hitbox{x_ - radius_, y_ - radius_, radius_ * 2.f, radius_ * 2.f};
}

int SpikeProjectile::getDamage() const { return damage_; }

bool SpikeProjectile::isAlive() const { return life_ > 0.f; }

void SpikeProjectile::update(float dt) {
    x_ += vx_ * dt;
    y_ += vy_ * dt;
    life_ -= dt;
}

// ---------- AppleProjectile ----------

AppleProjectile::AppleProjectile(float x, float y, float vy, int damage)
    : x_(x), y_(y), vy_(vy), damage_(damage) {}

Hitbox AppleProjectile::getHitbox() const {
    return Hitbox{x_ - radius_, y_ - radius_, radius_ * 2.f, radius_ * 2.f};
}

int AppleProjectile::getDamage() const { return damage_; }

bool AppleProjectile::isAlive() const {
    return y_ < static_cast<float>(SCREEN_HEIGHT) + 40.f;
}

void AppleProjectile::update(float dt) {
    y_ += vy_ * dt;
}

// ---------- GlassShardProjectile ----------

GlassShardProjectile::GlassShardProjectile(
    float x, float y, float vx, float vy, int damage, float life)
    : x_(x), y_(y), vx_(vx), vy_(vy), life_(life), damage_(damage) {}

Hitbox GlassShardProjectile::getHitbox() const {
    return Hitbox{x_ - radius_, y_ - radius_, radius_ * 2.f, radius_ * 2.f};
}

int GlassShardProjectile::getDamage() const { return damage_; }

bool GlassShardProjectile::isAlive() const { return life_ > 0.f; }

void GlassShardProjectile::update(float dt) {
    x_ += vx_ * dt;
    y_ += vy_ * dt;
    life_ -= dt;
}

// ---------- ItemManager ----------

void ItemManager::clear() {
    passives_.clear();
}

void ItemManager::update(float dt) {
    for (auto& projectile : passives_) {
        if (projectile) {
            projectile->update(dt);
        }
    }
    cull_dead_passives();
}

void ItemManager::spawn_passive(std::unique_ptr<BaseProjectile> projectile) {
    if (projectile) {
        passives_.push_back(std::move(projectile));
    }
}

void ItemManager::cull_dead_passives() {
    passives_.erase(
        std::remove_if(
            passives_.begin(),
            passives_.end(),
            [](const std::unique_ptr<BaseProjectile>& p) {
                return !p || !p->isAlive();
            }),
        passives_.end());
}

void ItemManager::resolve_projectile_hits(
    std::vector<Bullet>& bullets,
    std::vector<Enemy>& enemies,
    BossSystem& boss_system,
    const Player& player,
    const ProjectileHitCallbacks& callbacks,
    std::vector<Bullet>& pending_bullets)
{
    const int player_damage = player.get_damage();

    std::vector<EnemyDamageable> enemy_views;
    enemy_views.reserve(enemies.size());
    for (Enemy& enemy : enemies) {
        enemy_views.emplace_back(enemy);
    }

    std::vector<IDamageable*> boss_targets = boss_system.damageable_view();

    // --- 玩家子弹（引用 bullets，不复制实体）---
    for (size_t bi = 0; bi < bullets.size(); ) {
        if (!bullet_participates_in_combat(bullets[bi])) {
            ++bi;
            continue;
        }

        BulletProjectile projectile(bullets[bi], player_damage);
        bool bullet_consumed = false;

        for (size_t ei = 0; ei < enemy_views.size(); ++ei) {
            if (!enemy_views[ei].isAlive()) {
                continue;
            }
            if (resolve_one_hit(
                    projectile,
                    &bullets[bi],
                    enemy_views[ei],
                    callbacks,
                    &enemy_views[ei],
                    nullptr,
                    pending_bullets)) {
                bullet_consumed = true;
                break;
            }
        }

        if (!bullet_consumed) {
            for (IDamageable* boss_target : boss_targets) {
                if (boss_target == nullptr || !boss_target->isAlive()) {
                    continue;
                }
                auto* boss = dynamic_cast<BaseBoss*>(boss_target);
                if (boss == nullptr) {
                    continue;
                }
                BulletProjectile boss_proj(bullets[bi], player_damage);
                if (resolve_one_hit(
                        boss_proj,
                        &bullets[bi],
                        *boss_target,
                        callbacks,
                        nullptr,
                        boss,
                        pending_bullets)) {
                    bullet_consumed = true;
                    break;
                }
            }
        }

        if (bullet_consumed || bullets[bi].is_dead) {
            bullets.erase(bullets.begin() + static_cast<std::ptrdiff_t>(bi));
        } else {
            ++bi;
        }
    }

    // --- ItemManager 托管的被动投射物 ---
    for (size_t pi = 0; pi < passives_.size(); ) {
        BaseProjectile& projectile = *passives_[pi];
        if (!projectile.isAlive()) {
            passives_.erase(passives_.begin() + static_cast<std::ptrdiff_t>(pi));
            continue;
        }

        bool projectile_consumed = false;

        for (size_t ei = 0; ei < enemy_views.size(); ++ei) {
            if (!enemy_views[ei].isAlive()) {
                continue;
            }
            if (resolve_one_hit(
                    projectile,
                    nullptr,
                    enemy_views[ei],
                    callbacks,
                    &enemy_views[ei],
                    nullptr,
                    pending_bullets)) {
                projectile_consumed = projectile.consumes_on_hit();
                break;
            }
        }

        if (!projectile_consumed) {
            for (IDamageable* boss_target : boss_targets) {
                if (boss_target == nullptr || !boss_target->isAlive()) {
                    continue;
                }
                auto* boss = dynamic_cast<BaseBoss*>(boss_target);
                if (boss == nullptr) {
                    continue;
                }
                if (resolve_one_hit(
                        projectile,
                        nullptr,
                        *boss_target,
                        callbacks,
                        nullptr,
                        boss,
                        pending_bullets)) {
                    projectile_consumed = projectile.consumes_on_hit();
                    break;
                }
            }
        }

        if (projectile_consumed || !projectile.isAlive()) {
            passives_.erase(passives_.begin() + static_cast<std::ptrdiff_t>(pi));
        } else {
            ++pi;
        }
    }

    boss_system.remove_dead_bosses();

    enemies.erase(
        std::remove_if(
            enemies.begin(),
            enemies.end(),
            [](const Enemy& enemy) { return enemy.hp <= 0; }),
        enemies.end());
}
