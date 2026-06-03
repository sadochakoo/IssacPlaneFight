# 飞机大战（以撒版）— 项目框架学习指南

> 版本：v3.4 | 技术栈：C++17 + SFML 2.6.2 | 平台：Windows x64 / VS2022

本文档帮助你快速理解项目的**核心架构**、**程序流程**和**关键函数**，适合作为阅读源码的导航地图。

---

## 1. 项目概览

这是一款仿《以撒的结合》机制的纵版射击游戏：

- **属性系统**：HP、伤害、射速、弹速、射程、移速、额外弹道
- **升级三选一**：分数达标后暂停游戏，从 8 种被动道具中随机抽 3 个
- **波次刷怪**：每层 1 波敌人，清完休息 3.5 秒，再进入下一波/下一层
- **分层难度**：6 层递进，7 种敌人类型，东方风格弹幕
- **被动技能**：道具拾取后自动生效，可叠加，部分组合有额外效果

---

## 2. 目录结构

```
IssacPlaneFight/
├── PlaneWar.sln              # VS2022 解决方案
├── PlaneWar.vcxproj          # 编译配置（C++17 + SFML 静态链接）
│
├── main.cpp                  # ★ 游戏主程序（~2300 行）
│                             #   状态机、主循环、生成、碰撞、绘制
│
├── player_stats.h            # ★ 数据层（头文件即实现）
│                             #   属性、道具池、实体结构体、Player 类
│
├── level_up_panel.h          # 升级 UI 声明
├── level_up_panel.cpp        # 升级 UI 实现（三选一卡牌）
│
├── bin/Debug|Release/        # 编译输出 PlaneWar.exe
├── obj/                      # 中间文件
└── SFML/                     # 本地 SFML 头文件副本（编译用外部 lib）

外部依赖：
└── ../SFML-2.6.2/            # SFML SDK（include + lib）
```

**源码只有 4 个业务文件**，逻辑高度集中在 `main.cpp`，数据定义集中在 `player_stats.h`。

---

## 3. 核心架构

项目采用**三层分离 + 单文件主循环**的经典游戏架构：

```
┌─────────────────────────────────────────────────────────┐
│                    UI 层                                 │
│  level_up_panel.h/.cpp                                    │
│  - LevelUpPanel：升级暂停、三选一、鼠标交互               │
└────────────────────────┬────────────────────────────────┘
                         │ 选中道具索引
                         ▼
┌─────────────────────────────────────────────────────────┐
│                    数据层                                │
│  player_stats.h                                           │
│  - PlayerStats / ItemEffect / Item / ITEM_POOL           │
│  - Enemy / Bullet / PowerUp / Particle / 被动投射物       │
│  - Player 类：apply_item() / take_damage() / 计时器      │
└────────────────────────┬────────────────────────────────┘
                         │ 读写实体与属性
                         ▼
┌─────────────────────────────────────────────────────────┐
│                    游戏层                                │
│  main.cpp                                                 │
│  - GameState 状态机                                       │
│  - 全局 vector 容器（子弹/敌人/粒子…）                    │
│  - 波次系统 / 敌人生成 / 碰撞 / 被动技能 / 渲染           │
└─────────────────────────────────────────────────────────┘
```

### 设计特点

| 特点 | 说明 |
|------|------|
| **ECS 简化版** | 没有完整 ECS，用 `struct` + `std::vector` 管理同类实体 |
| **数据驱动道具** | 道具效果定义在 `ITEM_POOL[]`，`apply_item()` 统一处理 |
| **状态机驱动流程** | `GameState` 枚举控制菜单/游戏/升级/结束 |
| **固定帧率** | `FRAME_TIME = 1/60`，逻辑按 `dt` 更新 |
| **被动技能分散实现** | v3.4 被动道具逻辑写在 `main.cpp` PLAYING 分支中 |

---

## 4. 数据模型（player_stats.h）

### 4.1 核心结构体

```
PlayerStats          玩家数值属性（hp/damage/tear_rate/speed…）
ItemEffect           道具效果描述（属性修改 + 定时效果标志）
Item                 道具展示信息（名称/描述/颜色/effect）
Enemy                敌人（位置/HP/类型/射击计时器/螺旋角…）
Bullet               子弹（位置/速度/寿命/追踪/颜色）
PowerUp              地图掉落物（位置/item_type）
Particle             粒子特效
HealthDrop           红心回血掉落（main.cpp 定义）
SpikeProjectile…     v3.4 被动技能专用投射物（8 种）
```

### 4.2 Player 类（最重要）

```cpp
class Player {
    sf::Vector2f pos;       // 位置
    PlayerStats  stats;     // 当前属性
    PlayerStats  base_stats;// 永久属性快照（定时效果恢复用）

    int fire_cooldown;      // 射击冷却（帧）
    int tracking_timer;     // 追踪弹计时
    int invisible_timer;    // 隐形计时
    int shield_timer;       // 护盾（-1=一次性）
    int clone_count;        // 淫魔分身数

    int count_spike…        // v3.4 各被动道具叠数

    void apply_item(const ItemEffect& e);  // ★ 道具效果入口
    void update_timers();                  // 每帧递减计时器
    int  get_damage() const;               // ceil(damage) 取整伤害
    bool take_damage();                    // 护盾→隐形→扣血
};
```

### 4.3 道具池（8 种，v3.4）

| 索引 | 枚举 | 名称 | 效果概要 |
|------|------|------|----------|
| 0 | ITEM_SPIKE | 八寸钉 | 8 方向尖刺，2 波，伤害随距离衰减 |
| 1 | ITEM_LANTERN | 小夜灯 | 竖直激光，碰敌半血 |
| 2 | ITEM_BETRAY | 背叛 | 子弹首中→敌机转阵营上行攻击 |
| 3 | ITEM_APPLE | 牛顿的苹果 | 底部升苹果，碰敌散 8 方向刀片 |
| 4 | ITEM_GLASS | 玻璃碎片 | 周期性散射碎片 |
| 5 | ITEM_CHARM | 寻友护符 | 镜像分身同步射击 |
| 6 | ITEM_ICE_BABY | 冰块宝宝 | 弹跳冰球，冰冻敌人 5 秒 |
| 7 | ITEM_BEST_FRIEND | 好朋友一辈子 | 周期短暂加速 1.5× |

**组合效果**（在 main.cpp 中硬编码判断）：
- 背叛 + 八寸钉 → 尖刺击杀也转阵营
- 玻璃碎片 + 苹果 → 苹果变 AOE 爆炸 -10HP
- 好朋友一辈子 + 寻友护符 → 2× 速 + 护盾 5 秒

### 4.4 敌人类型（7 种）

| 类型 | 移动 | 弹幕 |
|------|------|------|
| NORMAL | 直线下落 | 单发瞄准 |
| DOUBLE_SHOOT | 直线下落 | 双发 ±10° 扩散 |
| TRACKING | 追踪玩家 | 单发瞄准 |
| ELITE | 追踪玩家 | 三发 ±15° 扩散 |
| BLOOM | 直线下落 | 圆形散射（8+层数×2 发） |
| SPIRAL | 直线下落 | 双臂螺旋旋转 |
| SPIRAL_ELITE | 追踪玩家 | 三臂螺旋旋转 |

层数 `player_level`（1~6）决定各类型出现概率，见 `get_layer_probs()`。

---

## 5. 游戏状态机

```
                    Enter
         ┌──────────────────────────┐
         │                          │
         ▼                          │
      ┌──────┐    Enter         ┌─────────┐
      │ MENU │ ──────────────► │ PLAYING │
      └──────┘                  └────┬────┘
                                     │
                        分数 ≥ next_level_threshold
                                     │
                                     ▼
                               ┌───────────┐
                               │ LEVEL_UP  │ ← 游戏暂停，显示三选一
                               └─────┬─────┘
                                     │ 鼠标点击选道具
                                     ▼
                               apply_item()
                               player_level++
                               回到 PLAYING

      PLAYING ── HP ≤ 0 ──► GAME_OVER ── Enter ──► reset_game() ──► PLAYING
```

### 状态枚举

```cpp
enum class GameState {
    MENU,       // 主菜单，等 Enter
    PLAYING,    // 正常游戏
    LEVEL_UP,   // 升级面板（逻辑暂停，仍渲染游戏画面+遮罩）
    GAME_OVER   // 死亡，等 Enter 重开
};
```

---

## 6. 程序流程（主循环）

### 6.1 启动流程

```
main()
  ├── 创建 sf::RenderWindow (800×900, 60FPS)
  ├── load_font()          加载系统中文字体
  ├── Player player        初始化玩家
  ├── LevelUpPanel panel   初始化升级 UI
  └── game_state = MENU
```

### 6.2 每帧循环（while window.isOpen）

```
┌─ 1. 事件处理 ─────────────────────────────────────┐
│  pollEvent：Closed / Escape 关闭窗口               │
│  Enter：MENU 或 GAME_OVER → reset_game → PLAYING  │
└───────────────────────────────────────────────────┘
                         │
                         ▼
┌─ 2. 逻辑更新（switch game_state）─────────────────┐
│  MENU      → 无逻辑                                │
│  PLAYING   → 见下方「PLAYING 子流程」              │
│  LEVEL_UP  → panel.update() → apply_item → PLAYING│
│  GAME_OVER → 无逻辑                                │
└───────────────────────────────────────────────────┘
                         │
                         ▼
┌─ 3. 渲染（switch game_state）─────────────────────┐
│  clear(深紫背景)                                   │
│  MENU      → 标题 + 操作说明                       │
│  PLAYING   → 实体 + HUD + 波次公告                 │
│  LEVEL_UP  → 游戏画面 + panel.render() 遮罩        │
│  GAME_OVER → 分数 + 重开提示                       │
│  display()                                         │
└───────────────────────────────────────────────────┘
```

### 6.3 PLAYING 子流程（核心，按执行顺序）

```
输入与移动
  └── WASD/方向键 → 更新 player.pos（边界 clamp）

玩家射击
  └── Space/J + fire_cooldown → 鼠标瞄准方向发射
      └── 额外弹道 / 追踪弹 / 淫魔分身射击

更新投射物
  ├── bullets[]        玩家子弹（含追踪逻辑）
  ├── enemy_bullets[]  敌人子弹
  └── 各被动投射物 vector（尖刺/苹果/刀片/冰球…）

更新敌人
  ├── 追踪型敌人向玩家加速
  ├── 碰撞玩家 → take_damage() → 可能 GAME_OVER
  └── 飞出屏幕 → 删除

碰撞检测
  ├── 玩家子弹 vs 敌人 → 扣 HP / 死亡 / 掉落 / 背叛转化
  ├── 拾取 power_ups[] → apply_item()
  └── 拾取 health_drops[] → +1 HP

被动技能系统 (v3.4)
  ├── 八寸钉 / 小夜灯 / 苹果 / 玻璃 / 护符 / 冰球 / 好朋友
  └── 转换敌人 update

玩家计时器
  └── player.update_timers()

波次系统 (v3.1)
  ├── wave_pause → 倒计时 → start_layer_wave()
  ├── wave_active → 间隔 spawn_enemy()
  └── 全灭 → start_wave_pause() 或 init_layer_waves()

敌人射击
  └── 每个 enemy.shoot_timer → 按类型生成弹幕

玩家被弹击中
  └── enemy_bullets vs player → take_damage()

升级检测
  └── game_score ≥ next_level_threshold → LEVEL_UP
```

---

## 7. 关键系统详解

### 7.1 波次系统（v3.1）

**设计**：每层 = 1 波，模仿 PvZ「一大波敌人来袭」节奏。

```
init_layer_waves()     进入新层：计算敌人数，开始 2.5s 准备
start_layer_wave()     开始刷怪
start_wave_pause()     清完休息 3.5s
calc_enemies_for_layer 公式：8 + layer × 5（L1=13, L6=38）
```

**状态变量**：

| 变量 | 含义 |
|------|------|
| `wave_active` | 当前波是否进行中 |
| `wave_pause` | 是否波间休息 |
| `enemies_in_wave` | 本波计划总数 |
| `enemies_spawned_this_wave` | 已生成数 |
| `wave_spawn_timer` | 生成间隔计时 |
| `prev_player_level` | 检测升级以重置波次 |

### 7.2 分层难度（v2.9）

```
get_layer_probs(layer)      返回各敌人类型概率
determine_enemy_type(layer)  累积概率随机选类型
spawn_enemy()               按类型设置 HP/颜色/速度/分数
```

HP 公式：`base_hp = 2 + layer²`，再乘类型倍率（精英 2.0×，螺旋精英 2.5×）。

### 7.3 升级与分数

```
next_level_threshold  初始 800
升级后递增：+ 500 + player_level² × 80
player_level          同时作为难度层数（上限 6）
score_at_level_start  用于 HUD 进度条在本级内归零
```

### 7.4 道具掉落

```
spawn_item_pickup(x, y)
  ├── 分数门槛：game_score ≥ next_drop_score（初始 8000）
  ├── 冷却：item_drop_timer ≤ 0（间隔 20 秒）
  ├── 概率：8%
  └── 随机 ITEM_POOL 索引 → 生成 PowerUp
```

击杀敌人另有 **5% 概率掉红心**（`health_drops[]`）。

### 7.5 升级面板（LevelUpPanel）

```
triggerLevelUp()   Fisher-Yates 洗牌，取 3 个道具索引
update(window)     检测鼠标左键点击卡牌 → 返回选中索引（-1=未选）
render(window)     半透明遮罩 + 3 张卡牌 + 标题
isActive()         current_options 非空即激活
```

---

## 8. 关键函数速查表

### 8.1 main.cpp — 工具函数

| 函数 | 作用 |
|------|------|
| `random_float(min, max)` | 均匀随机浮点数 |
| `random_int(min, max)` | 均匀随机整数 |
| `load_font()` | 加载系统中文字体（msyh/simhei/simsun/arial） |
| `check_collision(x1,y1,w1,h1, x2,y2,w2,h2)` | AABB 矩形碰撞 |
| `spawn_particles(x, y, color, count)` | 生成爆炸粒子 |

### 8.2 main.cpp — 难度与波次

| 函数 | 作用 |
|------|------|
| `get_layer_probs(layer)` | 获取该层敌人类型概率表 |
| `determine_enemy_type(layer)` | 随机决定敌人类型 |
| `calc_enemies_for_layer(layer)` | 计算本层敌人总数 |
| `init_layer_waves()` | 新层初始化波次 |
| `start_layer_wave()` | 开始刷怪 |
| `start_wave_pause()` | 进入波间休息 |

### 8.3 main.cpp — 游戏逻辑

| 函数 | 作用 |
|------|------|
| `spawn_enemy()` | 生成一个敌人（类型/HP/颜色/速度） |
| `spawn_item_pickup(x, y)` | 概率掉落道具 |
| `find_nearest_enemy(bx, by)` | 追踪弹找最近敌人 |
| `reset_game(player)` | 重置所有状态，返回 player 指针 |

### 8.4 main.cpp — 绘制

| 函数 | 作用 |
|------|------|
| `draw_player_hp_hearts(window, player, x, y)` | 左上角红心 HP |
| `draw_player_shape(window, player)` | 玩家三角形 + 护盾/隐形特效 |
| `draw_clone_shape(window, player)` | 淫魔分身绘制 |

### 8.5 player_stats.h — Player 类

| 方法 | 作用 |
|------|------|
| `apply_item(effect)` | **核心**：应用道具效果（属性/计时器/边界检查） |
| `update_timers()` | 每帧递减 tracking/invisible/shield/damage_boost |
| `get_damage()` | `ceil(stats.damage)` 取整伤害 |
| `take_damage()` | 护盾吸收 → 隐形免疫 → HP-1 |
| `is_alive()` | `stats.hp > 0` |

### 8.6 level_up_panel.cpp — UI

| 方法 | 作用 |
|------|------|
| `LevelUpPanel()` | 构造函数，加载字体 |
| `triggerLevelUp()` | 随机抽 3 道具 |
| `update(window)` | 处理点击，返回道具索引 |
| `render(window)` | 绘制遮罩和卡牌 |
| `isActive()` | 面板是否显示中 |

---

## 9. 全局容器（main.cpp）

所有游戏实体用 `std::vector` 管理，每帧遍历 + 标记删除：

```cpp
std::vector<Bullet>  bullets;          // 玩家子弹
std::vector<Bullet>  enemy_bullets;    // 敌人子弹
std::vector<Enemy>   enemies;          // 敌人
std::vector<PowerUp> power_ups;        // 道具掉落
std::vector<HealthDrop> health_drops;  // 红心掉落
std::vector<Particle> particles;       // 粒子

// v3.4 被动投射物
std::vector<SpikeProjectile> spikes;
std::vector<ConvertedEnemy> converted_enemies;
std::vector<AppleProjectile> apples;
std::vector<BladeProjectile> blades;
std::vector<GlassShardProjectile> glass_shards;
std::vector<IceBabyProjectile> ice_babies;
std::vector<CharmClone> charm_clones;
```

**删除模式**（典型写法）：

```cpp
for (size_t i = 0; i < vec.size(); ) {
    if (should_remove) {
        vec.erase(vec.begin() + i);
    } else {
        ++i;
    }
}
```

---

## 10. 编译与运行

### 依赖

- Visual Studio 2022（v143 工具集）
- SFML 2.6.2 静态库，路径：`../SFML-2.6.2/`

### 构建

```powershell
MSBuild PlaneWar.sln /p:Configuration=Debug /p:Platform=x64
```

### 运行

```
bin\Debug\PlaneWar.exe
```

### 操作

| 按键 | 功能 |
|------|------|
| WASD / 方向键 | 移动 |
| 空格 / J | 射击（朝鼠标方向） |
| Enter | 开始 / 重开 |
| ESC | 退出 |

---

## 11. 建议阅读顺序

按以下顺序读源码，由浅入深：

```
1. player_stats.h
   └── PlayerStats → Item → ITEM_POOL → Enemy/Bullet → Player 类

2. level_up_panel.h + level_up_panel.cpp
   └── 最简单的 UI 模块，理解状态机如何暂停游戏

3. main.cpp 分段阅读：
   ├── 第 53~170 行    枚举、全局变量、波次变量
   ├── 第 195~506 行   工具函数、难度、波次、生成
   ├── 第 671~730 行   main 入口、事件处理
   ├── 第 730~1020 行  移动、射击、碰撞核心
   ├── 第 1060~1520 行 v3.4 被动技能（最复杂）
   ├── 第 1520~1710 行 波次、敌人射击、升级检测
   └── 第 1749~末尾     渲染各状态 UI
```

---

## 12. 架构图总览

```
                         ┌─────────────┐
                         │   SFML 窗口  │
                         └──────┬──────┘
                                │
              ┌─────────────────┼─────────────────┐
              │                 │                 │
         事件输入           逻辑更新            渲染输出
              │                 │                 │
              ▼                 ▼                 ▼
        ┌──────────┐    ┌──────────────┐   ┌────────────┐
        │ Keyboard │    │  GameState   │   │ sf::Shape  │
        │  Mouse   │───►│   状态机     │──►│  sf::Text  │
        │  Close   │    └──────┬───────┘   │  Particles │
        └──────────┘           │           └────────────┘
                               │
              ┌────────────────┼────────────────┐
              │                │                │
              ▼                ▼                ▼
        ┌──────────┐   ┌────────────┐  ┌─────────────┐
        │  Player  │   │ vector容器  │  │ LevelUpPanel│
        │ 属性/道具 │   │ 子弹/敌人…  │  │  三选一 UI  │
        └──────────┘   └────────────┘  └─────────────┘
                               │
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
        波次系统          碰撞检测          被动技能
     spawn_enemy      check_collision    8种道具逻辑
```

---

## 13. 扩展建议

若后续要重构，可按模块拆分 `main.cpp`：

| 新文件 | 迁移内容 |
|--------|----------|
| `wave_system.cpp` | 波次相关函数 + 全局波次变量 |
| `enemy_system.cpp` | spawn_enemy / 敌人射击 / 分层概率 |
| `collision.cpp` | check_collision + 各碰撞处理 |
| `passive_skills.cpp` | v3.4 被动技能 update |
| `render.cpp` | 所有 draw_* 和 HUD 渲染 |

当前单文件结构适合学习和小型项目，但 `main.cpp` 超过 2000 行后维护成本会上升。

---

*文档生成日期：2026-06-03*
