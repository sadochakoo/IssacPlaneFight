/*
 * parasite_bullet.h - 寄生虫弹幕：四向命中分裂 + 触壁反弹分裂
 */

#ifndef PARASITE_BULLET_H
#define PARASITE_BULLET_H

#include "player_stats.h"
#include <vector>

constexpr int   k_parasite_max_generation = 4;
constexpr float k_parasite_damage_decay   = 0.75f;
constexpr float k_parasite_min_damage     = 0.2f;

bool can_parasite_split(const Bullet& bullet);

// 命中敌机：四向十字分裂（4 发）
void enqueue_parasite_hit_splits(const Bullet& parent,
                                 std::vector<Bullet>& pending_bullets);

// 触壁反弹：分裂 2 发次级弹
void enqueue_parasite_wall_splits(const Bullet& parent,
                                  std::vector<Bullet>& pending_bullets);

void setup_player_bullet(Bullet& bullet, const Player& player, bool is_baby_tear = false);

#endif
