# 道具系统架构（v4.0）

## 核心类

| 模块 | 文件 | 职责 |
|------|------|------|
| `PlayerStats` | `player_stats.h` | HP / 最大红心 / Damage / TearRate / ShotSpeed / Range / Speed |
| `Item`（多态） | `passive_item.h` | `applyTo(Player&)` 修改属性 |
| `ItemFactory` | `passive_items.cpp` | 工厂：按索引/ID 创建道具实例 |
| `ItemRegistry` | `item_registry.cpp` | UI 元数据（名称、描述、颜色） |
| `WeaponSynergyResolver` | `weapon_synergy_resolver.cpp` | **策略表**解析 `collected_items` → `WeaponProfile` |
| `BulletFactory` | `bullet_factory.cpp` | 根据 `WeaponProfile` 发射子弹/激光 |

## 融合规则（优先级自上而下）

1. 硫磺火 + 魔术弯勺 → **追踪激光** (`BrimstoneLaserHoming`)
2. 硫磺火 + 20/20 → **双激光** (`BrimstoneLaserDouble`)
3. 仅硫磺火 → **蓄力激光** (`BrimstoneLaser`)
4. 20/20 + 魔术弯勺 → 双发紫色追踪弹
5. 仅 20/20 → 双发平行弹
6. 仅魔术弯勺 → 单发紫色追踪弹（每帧微调 `vx` 朝向最近敌机）
7. 默认 → 单发向上

## 玩家拾取流程

```
LevelUp / 地图掉落
  → ItemFactory::create(index)
  → Player::applyItem(item*)
       → collected_items.push_back(id)
       → item->applyTo(stats)
  → 每帧 g_weapon_resolver.resolve(collected_items)
  → BulletFactory::tryFire / updateBullets / updateLasers
```

## 清空道具池（调试用）

将 `item_registry.cpp` 中 `DISPLAY_COUNT` 设为 `0`，`ItemFactory::create` 返回 `nullptr`，三选一 UI 仍显示「道具池为空」。
