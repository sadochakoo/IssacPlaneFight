/*
 * main.cpp - 飞机大战：以撒的结合 风格重制
 * 版本：v3.3 (2026-06-02)
 * 技术栈：C++17 / SFML 2.6.x
 * 平台：Windows x64 (Visual Studio 2022)
 *
 * 更新日志：
 *   v3.3 - 血量掉落：击杀敌人5%概率掉红心(+1HP)
 *          鼠标瞄准：子弹沿鼠标方向发射（手动控制方向）
 *          淫魔叠加：分身数量可叠加（最多5个），围绕玩家分布
 *   v3.2 - 血量平方递增：base_hp = 2 + layer^2
 *   v3.1 - 简化波次：每层1波，打完休息进下一层
 *          每层敌人数 = 8 + 层数×5（L1=13, L6=38）
 *          清除后休息 3.5 秒，未升级则刷新本层
 *   v3.0 - 波次系统：替代持续刷怪，PvZ式一波一波来
 *          消灭当前波全部敌人后休息 3.5 秒
 *          每层 3-8 波（L1=3, L6=8），波内敌人渐次增多
 *          波间公告文字："一大波敌人即将来袭！"
 *   v2.9 - 分层怪物难度系统：6层递增，7种敌人类型，东方弹幕设计
 *          层数=player_level，敌人数量×1.5/层，L1-L6逐层解锁新敌人
 *          开花弹（圆形散射）、螺旋弹（旋转弹幕）、追踪移动、精英怪
 *   v2.8 - 敌人击杀分值减半（50→25 基数, 30→15 乘数）
 *   v2.5 - 掉落间隔 4000→8000, 掉率 15%→8%
 *   v2.3 - 掉落间隔改为固定 4000 分
 *   v2.2 - 掉落门槛提高：首掉1500分，间隔1200分
 *   v2.1 - 道具掉落增加分数门槛（首掉1000分，每次+800分递增）
 *   v2.0 - 怪物能力随分数递增 + 掉落冷却 20s + 升级节奏减慢
 *   v1.0 - 初始以撒版（19种道具/三选一升级/追踪子弹/护盾/分身）
 * 命名规范：全小写下划线命名法 (snake_case)
 * 编码：UTF-8 with BOM
 *
 * 核心特性：
 * 1. 以撒风格 PlayerStats 属性系统
 * 2. 19 种道具（apply_item 动态修改属性）
 * 3. 升级三选一 UI 面板
 * 4. 定时效果系统（追踪/隐形/护盾/伤害增强）
 * 5. 粒子特效、分数系统
 * 6. v2.9 分层难度：6层递进、7种敌人、东方弹幕
 * 7. v3.1 简化波次：每层1波，打完休息进下一层
 */

#include <SFML/Graphics.hpp>      // 图形渲染（窗口、形状、文字）
#include <vector>                  // 动态数组（子弹/敌人/粒子列表）
#include <cmath>                   // 数学函数（sin/cos/sqrt）
#include <random>                  // 随机数引擎
#include <cstdlib>                 // rand/srand
#include <ctime>                   // time
#include <string>
#include <memory>

#include "player_stats.h"
#include "level_up_panel.h"
#include "passive_item.h"
#include "item_registry.h"
#include "attack_profile.h"
#include "bullet_factory.h"
#include "brimstone_laser.h"
#include "baby_system.h"
#include "item_test_loader.h"
#include "parasite_bullet.h"
#include "split_laser.h"

// ==================== 游戏状态枚举 ====================
/*
 * GameState - 游戏状态机
 *
 * 状态转换：
 *   MENU → (按 Enter) → PLAYING
 *   PLAYING → (分数达标) → LEVEL_UP
 *   LEVEL_UP → (选择道具) → PLAYING
 *   PLAYING → (玩家死亡) → GAME_OVER
 *   GAME_OVER → (按 Enter) → 重置 → PLAYING
 */
enum class GameState {
    MENU,       // 主菜单
    PLAYING,    // 游戏进行中
    LEVEL_UP,   // 升级面板（游戏暂停）
    GAME_OVER   // 游戏结束
};

// ==================== 全局随机数引擎 ====================
/*
 * 全局随机数生成器
 * 使用 Mersenne Twister 算法（高质量伪随机）
 * 种子：当前时间（每次运行结果不同）
 */
std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));

// ==================== 辅助函数 ====================

/*
 * random_float() - 生成 [min, max] 范围的随机浮点数
 */
float random_float(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

/*
 * random_int() - 生成 [min, max] 范围的随机整数
 */
int random_int(int min, int max) {
    if (min > max) return min;
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

// ==================== 全局游戏变量 ====================
// 子弹列表
std::vector<Bullet> bullets;

// 敌人子弹列表
std::vector<Bullet> enemy_bullets;

// 敌人列表
std::vector<Enemy> enemies;

// 道具拾取物列表
std::vector<PowerUp> power_ups;

// 血量拾取物列表 (v3.3)
struct HealthDrop { float x, y; };
std::vector<HealthDrop> health_drops;

// 粒子特效列表
std::vector<Particle> particles;

// 硫磺火 + 寄生虫：分裂短激光池
std::vector<SplitLaser> split_lasers;
int g_frame_count = 0;

// 游戏分数
int game_score = 0;

// 升级阈值（分数达到阈值时触发升级）
int next_level_threshold = 800;
int player_level = 1;
int score_at_level_start = 0;     // 本等级开始时的分数（用于进度条回零）

// 敌人生成计时器
float enemy_spawn_timer = 0.f;
float enemy_spawn_interval = 2.5f;  // 基础生成间隔（随分数连续递减）

// 敌人射击计时器
float enemy_shoot_timer = 0.f;
float enemy_shoot_interval = 2.0f;  // 基础射击间隔（随分数连续递减）

// 道具掉落冷却（防止短时间内连续掉落）
float item_drop_cooldown = 20.f;     // 两次掉落最少间隔 20 秒
float item_drop_timer = 0.f;         // 当前冷却计时器
int   next_drop_score = 8000;        // 达到此分数后才允许掉落，掉落后递增

// ==================== 波次系统 (v3.1) ====================
/*
 * 波次系统设计思路：
 *   模仿《植物大战僵尸》的节奏感
 *   每层 = 1 波大敌人，打完休息后进入下一层
 *   如果清完敌人但分数还不够升级，则刷新本层再战
 *   替代 v2.9 的持续不停刷怪模式
 *
 * 变量说明：
 *   enemies_in_wave:       本波计划生成的敌人总数
 *   enemies_spawned_this_wave: 已生成数（≤ enemies_in_wave）
 *   wave_active:           当前波是否正在进行中
 *   wave_pause:            是否处于波间休息
 *   wave_pause_timer:      波间休息倒计时（秒）
 *   wave_spawn_timer:      波内敌人生成间隔计时器（秒）
 *   prev_player_level:     上一帧的玩家等级（用于检测升级）
 */
int   enemies_in_wave = 0;
int   enemies_spawned_this_wave = 0;
bool  wave_active = false;
bool  wave_pause = true;          // 首波前也有一段准备时间
float wave_pause_timer = 2.0f;    // 首波前准备 2 秒
float wave_spawn_timer = 0.f;
int   prev_player_level = 1;

// ==================== SFML 全局对象 ====================
// 游戏字体（用于 UI 文字）
sf::Font game_font;

/*
 * load_font() - 加载中文字体
 *
 * 按优先级尝试加载系统字体
 * 返回值：true=加载成功，false=加载失败
 */
bool load_font() {
    // 优先级 1：微软雅黑（最美观）
    if (game_font.loadFromFile("C:/Windows/Fonts/msyh.ttc")) return true;
    if (game_font.loadFromFile("C:/Windows/Fonts/msyhbd.ttc")) return true;
    // 优先级 2：黑体
    if (game_font.loadFromFile("C:/Windows/Fonts/simhei.ttf")) return true;
    // 优先级 3：宋体
    if (game_font.loadFromFile("C:/Windows/Fonts/simsun.ttc")) return true;
    // 优先级 4：Arial（英文）
    if (game_font.loadFromFile("C:/Windows/Fonts/arial.ttf")) return true;
    return false;
}

// ==================== 碰撞检测函数 ====================

/*
 * check_collision() - AABB 矩形碰撞检测
 *
 * 参数：两个矩形的左上角坐标和宽高
 * 返回值：true=碰撞，false=不碰撞
 *
 * 算法：轴对齐包围盒 (Axis-Aligned Bounding Box)
 *       两个矩形在 X 和 Y 轴上的投影都有重叠 → 碰撞
 */
bool check_collision(float x1, float y1, float w1, float h1,
                     float x2, float y2, float w2, float h2) {
    return (x1 < x2 + w2 && x1 + w1 > x2 &&
            y1 < y2 + h2 && y1 + h1 > y2);
}

// ==================== 粒子系统 ====================

/*
 * spawn_particles() - 在指定位置生成爆炸粒子
 *
 * 参数：
 *   x, y  - 爆炸中心位置
 *   color - 粒子颜色
 *   count - 粒子数量
 */
void spawn_particles(float x, float y, sf::Color color, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.x = x;
        p.y = y;
        // 随机方向和速度（形成圆形爆炸效果）
        float angle = random_float(0.f, 6.28318f);  // 0 到 2*PI
        float speed = random_float(50.f, 200.f);
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        p.life = random_float(0.3f, 0.8f);           // 粒子生命 0.3-0.8 秒
        p.max_life = p.life;
        p.color = color;
        particles.push_back(p);
    }
}

// ==================== 分层难度系统 (v2.9) ====================

/*
 * 分层概率数据结构
 * 每个 float 表示该类型敌人占生成总数的比例（0~1）
 */
struct LayerProbs {
    float normal;       float double_shoot;
    float tracking;     float elite;
    float bloom;        float spiral;
    float spiral_elite;
};

/*
 * get_layer_probs() - 获取当前层的敌人类型概率分布
 *
 * 层=player_level，上限裁剪到 6
 * 概率严格遵循用户规格 + 东方风格扩展
 *
 * 概率分布（总和=1.0）：
 *   L1: 普通 90%, 双发 10%
 *   L2: 普通 75%, 双发 10%, 追踪 15%
 *   L3: 普通 65%, 双发 10%, 追踪 15%, 精英 10%
 *   L4: 普通 45%, 双发 10%, 追踪 15%, 精英 10%, 开花 20%
 *   L5: 普通 30%, 追踪 10%, 精英 10%, 开花 20%, 螺旋 15%, 螺旋精英 15%
 *   L6: 普通 20%, 追踪 10%, 精英 10%, 开花 20%, 螺旋 20%, 螺旋精英 20%
 */
LayerProbs get_layer_probs(int layer) {
    if (layer >= 6) return {0.20f, 0.f, 0.10f, 0.10f, 0.20f, 0.20f, 0.20f};
    if (layer == 5) return {0.30f, 0.f, 0.10f, 0.10f, 0.20f, 0.15f, 0.15f};
    if (layer == 4) return {0.45f, 0.10f, 0.15f, 0.10f, 0.20f, 0.f, 0.f};
    if (layer == 3) return {0.65f, 0.10f, 0.15f, 0.10f, 0.f, 0.f, 0.f};
    if (layer == 2) return {0.75f, 0.10f, 0.15f, 0.f, 0.f, 0.f, 0.f};
    return {0.90f, 0.10f, 0.f, 0.f, 0.f, 0.f, 0.f};  // L1
}

/*
 * determine_enemy_type() - 根据层数概率随机决定敌人类型
 *
 * 参数：layer - 当前层数（1-6，超出按 6 处理）
 * 返回值：EnemyType 枚举
 *
 * 算法：生成 [0, 100) 浮点数，按概率累积判定
 */
EnemyType determine_enemy_type(int layer) {
    LayerProbs p = get_layer_probs(layer);
    float roll = random_float(0.f, 100.f);
    float accum = 0.f;

    // 累积概率判定（从最稀有到最常见）
    accum += p.spiral_elite * 100.f;
    if (roll < accum) return ENEMY_SPIRAL_ELITE;
    accum += p.spiral * 100.f;
    if (roll < accum) return ENEMY_SPIRAL;
    accum += p.bloom * 100.f;
    if (roll < accum) return ENEMY_BLOOM;
    accum += p.elite * 100.f;
    if (roll < accum) return ENEMY_ELITE;
    accum += p.tracking * 100.f;
    if (roll < accum) return ENEMY_TRACKING;
    accum += p.double_shoot * 100.f;
    if (roll < accum) return ENEMY_DOUBLE_SHOOT;
    return ENEMY_NORMAL;
}

// ==================== 波次系统核心函数 (v3.1) ====================

/*
 * calc_enemies_for_layer() - 计算本层（1波）的敌人总数
 *
 * 公式：8 + layer × 5
 *   L1: 13   L2: 18   L3: 23
 *   L4: 28   L5: 33   L6: 38
 */
int calc_enemies_for_layer(int layer) {
    return 8 + layer * 5;
}

/*
 * init_layer_waves() - 初始化本层的波次
 *
 * 进入新层时调用（游戏开始或升级后）
 * 设置敌人数量，准备短暂开场暂停
 */
void init_layer_waves() {
    enemies_in_wave = calc_enemies_for_layer(player_level);
    enemies_spawned_this_wave = 0;
    wave_active = false;
    wave_pause = true;
    wave_pause_timer = 2.5f;  // 新层开始前 2.5 秒准备时间
    wave_spawn_timer = 0.f;
}

/*
 * start_layer_wave() - 开始本层唯一的波次
 */
void start_layer_wave() {
    enemies_in_wave = calc_enemies_for_layer(player_level);
    enemies_spawned_this_wave = 0;
    wave_active = true;
    wave_pause = false;
    wave_pause_timer = 0.f;
    wave_spawn_timer = 0.f;
}

/*
 * start_wave_pause() - 波间休息
 *
 * 当前波所有敌人被消灭后，休息 3.5 秒
 */
void start_wave_pause() {
    wave_active = false;
    wave_pause = true;
    wave_pause_timer = 3.5f;
}

// ==================== 敌人生成系统 ====================

/*
 * spawn_enemy() - 生成一个敌人（v2.9 分层难度版）
 *
 * 难度随玩家等级递增：
 * - 敌人类型按层数概率分布随机选择
 * - HP = 层数基础 × 类型倍率
 * - 速度随层数增加
 * - 分数随类型和层数增加
 * - 外观颜色区分类型
 */
void spawn_enemy() {
    Enemy e;
    e.x = random_float(50.f, 750.f);
    e.y = -30.f;

    // 裁剪层数到有效范围
    int layer = player_level;
    if (layer < 1) layer = 1;
    if (layer > 6) layer = 6;

    // === 根据层数概率随机决定敌人类型 ===
    e.enemy_type = determine_enemy_type(layer);

    // === 属性计算 ===
    // 基础 HP：平方递增，后期敌人明显更耐打
    // L1=3, L2=6, L3=11, L4=18, L5=27, L6=38
    int base_hp = 2 + layer * layer;

    // 类型倍率（精英和弹幕型更耐打）
    float type_hp_mult = 1.0f;
    int type_score = 25;
    sf::Color type_color = sf::Color(255, 100, 100);  // 默认红色
    int type_width = 30, type_height = 30;

    switch (e.enemy_type) {
        case ENEMY_DOUBLE_SHOOT:
            type_hp_mult = 1.2f; type_score = 35;
            type_color = sf::Color(255, 160, 60);        // 橙色
            break;
        case ENEMY_TRACKING:
            type_hp_mult = 1.2f; type_score = 35;
            type_color = sf::Color(180, 60, 220);         // 紫色
            break;
        case ENEMY_ELITE:
            type_hp_mult = 2.0f; type_score = 60;
            type_color = sf::Color(180, 30, 30);          // 暗红
            type_width = 36; type_height = 36;
            break;
        case ENEMY_BLOOM:
            type_hp_mult = 1.5f; type_score = 45;
            type_color = sf::Color(60, 180, 255);         // 天蓝
            break;
        case ENEMY_SPIRAL:
            type_hp_mult = 1.3f; type_score = 50;
            type_color = sf::Color(60, 220, 120);         // 绿色
            break;
        case ENEMY_SPIRAL_ELITE:
            type_hp_mult = 2.5f; type_score = 80;
            type_color = sf::Color(30, 180, 60);          // 深绿
            type_width = 36; type_height = 36;
            break;
        default:  // ENEMY_NORMAL
            type_hp_mult = 1.0f; type_score = 25;
            break;
    }

    // 最终属性
    e.max_hp = static_cast<int>(base_hp * type_hp_mult) + random_int(0, 1);
    e.hp = e.max_hp;
    e.color = type_color;
    e.width = type_width;
    e.height = type_height;
    e.score = type_score + layer * 5;  // 层数加成

    // 下落速度：基础 + 层数加成
    e.vy = random_float(70.f, 120.f) + layer * 12.f;

    // 横向飘移
    float drift = 30.f + static_cast<float>(layer) * 10.f;
    e.vx = random_float(-drift, drift);

    // 初始化射击计时器（随机错开，避免所有敌人同时开火）
    e.shoot_timer = random_float(0.f, 1.5f);

    // 初始化螺旋角度
    e.spiral_angle = random_float(0.f, 6.28318f);

    enemies.push_back(e);
}

// ==================== 道具掉落系统 ====================

/*
 * spawn_item_pickup() - 在指定位置掉落道具拾取物
 *
 * 参数：x, y - 掉落位置
 *
 * 概率分布（模拟以撒道具池）：
 *   魔法蘑菇 10% > 午餐 8% > 血袋 7% > ...
 */
void spawn_item_pickup(float x, float y) {
    // 分数门槛：未达到 next_drop_score 不掉落
    if (game_score < next_drop_score) return;
    // 冷却期间不掉落
    if (item_drop_timer > 0.f) return;
    if (random_float(0.f, 100.f) > 8.0f) return;  // 8% 掉落率

    // 触发冷却计时 + 提高下次掉落门槛
    item_drop_timer = item_drop_cooldown;
    next_drop_score = game_score + 8000;  // 下次掉落需要再得 8000 分

    PowerUp p;
    p.x = x;
    p.y = y;
    p.active = true;

    // 随机道具（v3.4：8个被动融合道具，均等概率）
    const int pool_size = ItemRegistry::itemCount();
    if (pool_size <= 0) return;
    p.item_index = random_int(0, pool_size - 1);

    power_ups.push_back(p);
}

// ==================== 追踪子弹逻辑 ====================

/*
 * find_nearest_enemy() - 找到距离子弹最近的敌人
 *
 * 参数：bx, by - 子弹当前位置
 * 返回值：int - 最近敌人的索引（-1 表示没有敌人）
 *
 * 追踪子弹调用此函数来锁定目标
 */
int find_nearest_enemy(float bx, float by) {
    float min_dist = 1e9f;
    int nearest = -1;
    for (size_t i = 0; i < enemies.size(); ++i) {
        float dx = enemies[i].x - bx;
        float dy = enemies[i].y - by;
        float dist = dx * dx + dy * dy;  // 距离平方（避免开方）
        if (dist < min_dist) {
            min_dist = dist;
            nearest = static_cast<int>(i);
        }
    }
    return nearest;
}

// ==================== 重置游戏 ====================

/*
 * reset_game() - 重置所有游戏状态
 *
 * 清空所有列表，重置分数和计时器
 * 用于游戏结束后重新开始
 */
Player* reset_game(Player& player) {
    // 重新构造 Player（重置所有属性）
    player = Player();

    // 清空所有动态列表
    bullets.clear();
    enemy_bullets.clear();
    enemies.clear();
    power_ups.clear();
    particles.clear();
    BrimstoneLaser::reset(player);
    SplitLaserSystem::reset(split_lasers);
    BabySystem::reset(player);
    g_frame_count = 0;
    health_drops.clear();

    // 重置全局变量
    game_score = 0;
    next_level_threshold = 800;
    player_level = 1;
    enemy_spawn_timer = 0.f;
    enemy_shoot_timer = 0.f;
    enemy_spawn_interval = 2.5f;
    enemy_shoot_interval = 2.0f;
    item_drop_timer = 0.f;           // 重置掉落冷却
    next_drop_score = 8000;          // 重置掉落分数门槛
    score_at_level_start = 0;        // 重置等级起始分

    // v3.0 波次系统初始化
    init_layer_waves();
    prev_player_level = 1;

    return &player;
}

// ==================== 绘制辅助函数 ====================

/*
 * draw_player_hp_hearts() - 绘制玩家血量（红心）
 *
 * 在屏幕左上角绘制红心图标
 * 红色 = 有血，灰色 = 空血槽
 */
void draw_player_hp_hearts(sf::RenderWindow& window, const Player& player, float x, float y) {
    for (int i = 0; i < player.stats.max_hp; ++i) {
        sf::CircleShape heart(8.f);
        heart.setPosition(x + static_cast<float>(i) * 20.f, y);

        if (i < static_cast<int>(player.stats.hp)) {
            heart.setFillColor(sf::Color::Red);       // 有血 → 红色
        } else {
            heart.setFillColor(sf::Color(80, 20, 20)); // 空血 → 暗红
        }
        window.draw(heart);
    }
}

/*
 * draw_player_shape() - 绘制玩家飞机（三角形）
 *
 * 以撒风格的玩家外观
 */
void draw_player_shape(sf::RenderWindow& window, const Player& player) {
    // 主体（圆角三角形）
    sf::ConvexShape body;
    body.setPointCount(3);
    body.setPoint(0, sf::Vector2f(0.f, -20.f));    // 顶部
    body.setPoint(1, sf::Vector2f(-16.f, 16.f));    // 左下
    body.setPoint(2, sf::Vector2f(16.f, 16.f));     // 右下

    // 根据状态设置颜色
    sf::Color body_color;
    if (player.invisible_timer > 0) {
        body_color = sf::Color(100, 100, 255, 128);
    } else if (player.shield_timer != 0) {
        body_color = sf::Color(255, 255, 150);
    } else {
        body_color = sf::Color(255, 255, 255);
    }
    body.setFillColor(body_color);
    body.setOutlineThickness(2.f);
    body.setOutlineColor(sf::Color(100, 100, 100));
    body.setPosition(player.pos);
    window.draw(body);

    // 护盾光环（有护盾时外圈发光）
    if (player.shield_timer != 0) {
        sf::CircleShape shield_circle(22.f);
        shield_circle.setOrigin(22.f, 22.f);
        shield_circle.setPosition(player.pos);
        shield_circle.setFillColor(sf::Color::Transparent);
        shield_circle.setOutlineThickness(2.f);
        shield_circle.setOutlineColor(sf::Color(255, 255, 100, 150));
        window.draw(shield_circle);
    }

    // 隐形波纹
    if (player.invisible_timer > 0) {
        sf::CircleShape invis(20.f + std::sin(static_cast<float>(player.invisible_timer) * 0.2f) * 4.f);
        invis.setOrigin(invis.getRadius(), invis.getRadius());
        invis.setPosition(player.pos);
        invis.setFillColor(sf::Color::Transparent);
        invis.setOutlineThickness(1.f);
        invis.setOutlineColor(sf::Color(100, 100, 255, 100));
        window.draw(invis);
    }
}

/*
 * draw_clone_shape() - 绘制分身
 *
 * 分身在玩家旁边飞行，自动射击
 */
void draw_clone_shape(sf::RenderWindow& window, const Player& player) {
    sf::ConvexShape clone_body;
    clone_body.setPointCount(3);
    clone_body.setPoint(0, sf::Vector2f(0.f, -14.f));
    clone_body.setPoint(1, sf::Vector2f(-11.f, 11.f));
    clone_body.setPoint(2, sf::Vector2f(11.f, 11.f));
    clone_body.setFillColor(sf::Color(255, 200, 200, 180));  // 半透明粉色
    clone_body.setOutlineThickness(1.f);
    clone_body.setOutlineColor(sf::Color(150, 100, 100));

    // 分身在玩家左右两侧
    float offset = std::sin(static_cast<float>(std::clock()) * 0.003f) * 30.f + 50.f;
    clone_body.setPosition(player.pos.x + offset, player.pos.y + 10.f);
    window.draw(clone_body);

    clone_body.setPosition(player.pos.x - offset, player.pos.y + 10.f);
    window.draw(clone_body);
}

// ==================== 主函数 ====================
/*
 * main() - 程序入口
 *
 * 游戏循环结构：
 *   初始化 SFML 窗口
 *   while (窗口打开) {
 *       处理事件（按键、关闭窗口）
 *       switch (游戏状态) {
 *           MENU:    显示主菜单
 *           PLAYING: 处理游戏逻辑（移动、射击、碰撞）
 *           LEVEL_UP: 处理升级面板
 *           GAME_OVER: 显示结束画面
 *       }
 *       绘制所有对象
 *       window.display();
 *   }
 */
int main() {
    // ===== 创建 SFML 窗口 =====
    // VideoMode(宽, 高) - 设置窗口大小 800x900
    // L"飞机大战 - 以撒版" - 窗口标题（宽字符）
    sf::RenderWindow window(
        sf::VideoMode(SCREEN_WIDTH, SCREEN_HEIGHT),
        L"飞机大战 - 以撒版"
    );
    window.setFramerateLimit(60);  // 限制 60 FPS

    // ===== 加载字体 =====
    load_font();

    // ===== 创建游戏对象 =====
    Player player;                      // 玩家
    LevelUpPanel level_up_panel;        // 升级面板

    // ===== 游戏状态 =====
    GameState game_state = GameState::MENU;

    // ===== 主循环 =====
    while (window.isOpen()) {
        // ========== 事件处理 ==========
        sf::Event event;
        while (window.pollEvent(event)) {
            // 关闭窗口
            if (event.type == sf::Event::Closed) {
                window.close();
            }

            // 按键事件
            if (event.type == sf::Event::KeyPressed) {
                // ESC 键退出
                if (event.key.code == sf::Keyboard::Escape) {
                    window.close();
                }

                // 菜单/结束画面按 Enter 开始新游戏
                if (event.key.code == sf::Keyboard::Enter) {
                    if (game_state == GameState::MENU ||
                        game_state == GameState::GAME_OVER) {
                        reset_game(player);
                        game_state = GameState::PLAYING;
                    }
                }

                // 调试：数字键 1/2/3 加载 test_items.json 用例（需 PLAYING）
                if (game_state == GameState::PLAYING) {
                    if (event.key.code == sf::Keyboard::Num1 ||
                        event.key.code == sf::Keyboard::Numpad1) {
                        load_test_case(player, "test_01_pure_brimstone");
                    } else if (event.key.code == sf::Keyboard::Num2 ||
                               event.key.code == sf::Keyboard::Numpad2) {
                        load_test_case(player, "test_02_brimstone_spoon_2020");
                    } else if (event.key.code == sf::Keyboard::Num3 ||
                               event.key.code == sf::Keyboard::Numpad3) {
                        load_test_case(player, "test_03_army_of_babies");
                    } else if (event.key.code == sf::Keyboard::Num4 ||
                               event.key.code == sf::Keyboard::Numpad4) {
                        load_test_case(player, "test_04_parasite");
                    } else if (event.key.code == sf::Keyboard::Num5 ||
                               event.key.code == sf::Keyboard::Numpad5) {
                        load_test_case(player, "test_05_brimstone_parasite");
                    }
                }
            }
        }

        // ========== 获取输入 ==========
        float dt = FRAME_TIME;  // 每帧时间

        // ========== 游戏逻辑 ==========
        switch (game_state) {
            // ==== 菜单状态 ====
            case GameState::MENU:
                // 菜单不更新游戏逻辑
                break;

            // ==== 游戏进行状态 ====
            case GameState::PLAYING: {
                ++g_frame_count;

                // --- 玩家移动 ---
                // WASD 或方向键控制移动
                float move_x = 0.f, move_y = 0.f;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::W) ||
                    sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
                    move_y = -1.f;
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::S) ||
                    sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
                    move_y = 1.f;
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::A) ||
                    sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
                    move_x = -1.f;
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::D) ||
                    sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
                    move_x = 1.f;
                }

                // 应用移动（速度 × 时间）
                // 从 stats.speed 读取当前移速
                player.pos.x += move_x * static_cast<float>(player.stats.speed) * dt;
                player.pos.y += move_y * static_cast<float>(player.stats.speed) * dt;

                // 边界限制（玩家不能飞出屏幕）
                if (player.pos.x < 20.f)  player.pos.x = 20.f;
                if (player.pos.x > 780.f) player.pos.x = 780.f;
                if (player.pos.y < 20.f)  player.pos.y = 20.f;
                if (player.pos.y > 880.f) player.pos.y = 880.f;

                BabySystem::updateOrbit(player, dt);

                const AttackProfile attack_profile = buildAttackProfile(player);

                if (player.fire_cooldown > 0) {
                    player.fire_cooldown--;
                }

                const bool fire_pressed =
                    sf::Keyboard::isKeyPressed(sf::Keyboard::Space) ||
                    sf::Keyboard::isKeyPressed(sf::Keyboard::J);

                BrimstoneLaser::updateChargeInput(player, fire_pressed);

                BulletFactory::tryFire(
                    player, attack_profile, bullets,
                    fire_pressed, dt);

                BulletFactory::updateBullets(
                    bullets, enemies,
                    static_cast<float>(player.stats.shot_speed), dt);

                // --- 敌人子弹 ---
                for (size_t i = 0; i < enemy_bullets.size(); ) {
                    enemy_bullets[i].x += enemy_bullets[i].vx * dt;
                    enemy_bullets[i].y += enemy_bullets[i].vy * dt;
                    enemy_bullets[i].life -= dt;

                    if (enemy_bullets[i].life <= 0.f ||
                        enemy_bullets[i].x < -50.f || enemy_bullets[i].x > 850.f ||
                        enemy_bullets[i].y < -50.f || enemy_bullets[i].y > 950.f) {
                        enemy_bullets.erase(enemy_bullets.begin() + i);
                    } else {
                        ++i;
                    }
                }

                // --- 更新敌人 ---
                for (size_t i = 0; i < enemies.size(); ) {
                    if (enemies[i].parasite_split_cooldown > 0) {
                        --enemies[i].parasite_split_cooldown;
                    }

                    // === 追踪型敌人：向玩家加速移动 ===
                    if (enemies[i].enemy_type == ENEMY_TRACKING ||
                        enemies[i].enemy_type == ENEMY_ELITE ||
                        enemies[i].enemy_type == ENEMY_SPIRAL_ELITE) {
                        float dx = player.pos.x - enemies[i].x;
                        float dy = player.pos.y - enemies[i].y;
                        float dist = std::sqrt(dx * dx + dy * dy);
                        if (dist > 1.f) {
                            // 平滑追踪：速度逐渐转向玩家方向
                            float track_speed = 60.f + static_cast<float>(player_level) * 10.f;
                            float target_vx = dx / dist * track_speed;
                            float target_vy = dy / dist * track_speed;
                            // 插值系数（追踪弹性）
                            float lerp = 2.5f * dt;
                            enemies[i].vx += (target_vx - enemies[i].vx) * lerp;
                            enemies[i].vy += (target_vy - enemies[i].vy) * lerp;
                        }
                    }

                    // 移动敌人
                    enemies[i].x += enemies[i].vx * dt;
                    enemies[i].y += enemies[i].vy * dt;

                    // 敌人飞出屏幕下方 → 删除
                    if (enemies[i].y > 950.f || enemies[i].x < -100.f || enemies[i].x > 900.f) {
                        enemies.erase(enemies.begin() + i);
                        continue;
                    }

                    // 检测敌人和玩家碰撞
                    if (check_collision(
                            player.pos.x - 16.f, player.pos.y - 20.f,
                            32.f, 40.f,
                            enemies[i].x - 15.f, enemies[i].y - 15.f,
                            30.f, 30.f)) {
                        // 玩家受伤
                        bool still_alive = player.take_damage();
                        spawn_particles(enemies[i].x, enemies[i].y,
                                       sf::Color(255, 255, 100), 8);
                        enemies.erase(enemies.begin() + i);

                        if (!still_alive) {
                            game_state = GameState::GAME_OVER;
                        }
                        continue;
                    }

                    ++i;
                }

                // --- 子弹与敌人碰撞检测（寄生虫分裂：先写入 pending，循环结束再合并）---
                std::vector<Bullet> pending_bullets;
                pending_bullets.reserve(128);

                for (size_t bi = 0; bi < bullets.size(); ) {
                    bool bullet_hit = false;
                    for (size_t ei = 0; ei < enemies.size(); ++ei) {
                        const float br = bullets[bi].radius;
                        if (check_collision(
                                bullets[bi].x - br, bullets[bi].y - br,
                                br * 2.f, br * 2.f,
                                enemies[ei].x - 15.f, enemies[ei].y - 15.f,
                                30.f, 30.f)) {
                            float hit_damage = bullets[bi].damage;
                            if (hit_damage <= 0.f) {
                                hit_damage = static_cast<float>(player.get_damage());
                            }
                            const int dmg = std::max(
                                1, static_cast<int>(std::ceil(hit_damage)));
                            enemies[ei].hp -= dmg;

                            if (enemies[ei].hp <= 0) {
                                game_score += enemies[ei].score;
                                spawn_particles(enemies[ei].x, enemies[ei].y,
                                               enemies[ei].color, 12);
                                spawn_item_pickup(enemies[ei].x, enemies[ei].y);
                                if (random_float(0.f, 100.f) < 5.0f) {
                                    health_drops.push_back({enemies[ei].x, enemies[ei].y});
                                }
                                enemies.erase(enemies.begin() + ei);
                            }

                            if (can_parasite_split(bullets[bi])) {
                                enqueue_parasite_hit_splits(bullets[bi], pending_bullets);
                            }

                            bullet_hit = true;
                            break;
                        }
                    }

                    if (bullet_hit) {
                        bullets.erase(bullets.begin() + bi);
                    } else {
                        ++bi;
                    }
                }

                if (!pending_bullets.empty()) {
                    bullets.insert(bullets.end(),
                                   pending_bullets.begin(),
                                   pending_bullets.end());
                }

                // --- 更新道具拾取物 ---
                for (size_t i = 0; i < power_ups.size(); ) {
                    power_ups[i].y += 40.f * dt;  // 道具缓慢下落

                    if (power_ups[i].y > 950.f) {
                        power_ups.erase(power_ups.begin() + i);
                        continue;
                    }

                    // 检测玩家拾取
                    if (check_collision(
                            player.pos.x - 16.f, player.pos.y - 20.f,
                            32.f, 40.f,
                            power_ups[i].x - 12.f, power_ups[i].y - 12.f,
                            24.f, 24.f)) {
                        // 应用道具效果
                        const int itype = power_ups[i].item_index;
                        if (itype >= 0 && itype < ItemRegistry::itemCount()) {
                            std::unique_ptr<Item> dropped(ItemFactory::create(itype));
                            if (dropped) {
                                player.applyItem(dropped.release());
                            }
                        }
                        // 特效粒子
                        spawn_particles(power_ups[i].x, power_ups[i].y,
                                       sf::Color(255, 255, 100), 10);
                        power_ups.erase(power_ups.begin() + i);
                    } else {
                        ++i;
                    }
                }

                // --- 血量拾取物更新与拾取 (v3.3) ---
                for (size_t i = 0; i < health_drops.size(); ) {
                    health_drops[i].y += 30.f * dt;  // 缓慢下落
                    bool picked = check_collision(
                        player.pos.x - 16.f, player.pos.y - 20.f,
                        32.f, 40.f,
                        health_drops[i].x - 10.f, health_drops[i].y - 10.f,
                        20.f, 20.f);
                    if (picked) {
                        player.stats.hp += 1;
                        if (player.stats.hp > player.base_stats.hp + 6)
                            player.stats.hp = player.base_stats.hp + 6;
                        spawn_particles(health_drops[i].x, health_drops[i].y,
                                       sf::Color(255, 80, 80), 6);
                        health_drops.erase(health_drops.begin() + i);
                    } else if (health_drops[i].y > 900.f) {
                        // 掉出屏幕消失
                        health_drops.erase(health_drops.begin() + i);
                    } else {
                        ++i;
                    }
                }

                // --- 更新粒子 ---
                for (size_t i = 0; i < particles.size(); ) {
                    particles[i].x += particles[i].vx * dt;
                    particles[i].y += particles[i].vy * dt;
                    particles[i].life -= dt;
                    if (particles[i].life <= 0.f) {
                        particles.erase(particles.begin() + i);
                    } else {
                        ++i;
                    }
                }

                // --- 更新玩家计时器 ---
                player.update_timers();

                {
                    auto on_laser_kill = [&](const Enemy& dead) {
                        game_score += dead.score;
                        spawn_particles(dead.x, dead.y, dead.color, 12);
                        spawn_item_pickup(dead.x, dead.y);
                        if (random_float(0.f, 100.f) < 5.0f)
                            health_drops.push_back({dead.x, dead.y});
                    };
                    BrimstoneLaser::updateLaser(
                        player, enemies, player.get_damage(),
                        split_lasers, on_laser_kill);

                    SplitLaserSystem::update(
                        split_lasers, enemies,
                        player.stats.has_parasite, on_laser_kill);
                }


                // --- 更新道具掉落冷却 ---
                if (item_drop_timer > 0.f) item_drop_timer -= dt;

                // --- 波次系统 (v3.1 每层1波) ---
                // 检测玩家升级：进入新层时重新初始化波次
                if (player_level != prev_player_level) {
                    prev_player_level = player_level;
                    init_layer_waves();
                }

                if (wave_pause) {
                    // 波间休息：倒计时，到零则开启本层波次
                    wave_pause_timer -= dt;
                    if (wave_pause_timer <= 0.f) {
                        start_layer_wave();
                    }
                } else if (wave_active) {
                    // 当前波进行中：按间隔逐一生成敌人
                    if (enemies_spawned_this_wave < enemies_in_wave) {
                        wave_spawn_timer += dt;
                        float spawn_delay = 0.6f;
                        if (wave_spawn_timer >= spawn_delay) {
                            wave_spawn_timer = 0.f;
                            spawn_enemy();
                            enemies_spawned_this_wave++;
                            // 最后 3 个敌人快速连续生成（"一大波"效果）
                            int remaining = enemies_in_wave - enemies_spawned_this_wave;
                            if (remaining <= 3 && remaining > 0) {
                                spawn_enemy();
                                enemies_spawned_this_wave++;
                            }
                        }
                    } else {
                        // 所有敌人已生成，等待玩家全部清掉
                        if (enemies.empty() && enemy_bullets.empty()) {
                            // 检查是否已升级（升级面板触发时 player_level 可能已变）
                            if (player_level != prev_player_level) {
                                prev_player_level = player_level;
                                init_layer_waves();
                            } else {
                                start_wave_pause();
                            }
                        }
                    }
                }

                // --- 敌人射击（v2.9 独立计时器 + 东方弹幕模式）---
                // 每个敌人有自己的 shoot_timer，按类型执行不同的弹幕模式
                float pad = 6.28318f;  // 2*PI 简写
                for (auto& enemy : enemies) {
                    enemy.shoot_timer -= dt;

                    // 根据敌人类型设定射击间隔
                    float shoot_interval = 2.0f;
                    switch (enemy.enemy_type) {
                        case ENEMY_NORMAL:       shoot_interval = 2.0f; break;
                        case ENEMY_DOUBLE_SHOOT: shoot_interval = 2.5f; break;
                        case ENEMY_TRACKING:     shoot_interval = 2.0f; break;
                        case ENEMY_ELITE:        shoot_interval = 1.5f; break;
                        case ENEMY_BLOOM:        shoot_interval = 1.8f; break;
                        case ENEMY_SPIRAL:       shoot_interval = 0.12f; break;
                        case ENEMY_SPIRAL_ELITE: shoot_interval = 0.15f; break;
                    }

                    // 每层射击间隔加速 5%
                    shoot_interval *= std::max(0.5f, 1.f - float(player_level - 1) * 0.05f);

                    if (enemy.shoot_timer > 0.f) continue;
                    enemy.shoot_timer = shoot_interval;

                    // 基础弹速（随层数递增）
                    float spd = 180.f + float(player_level) * 15.f;
                    float dx = player.pos.x - enemy.x;
                    float dy = player.pos.y - enemy.y;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist < 1.f) dist = 1.f;

                    switch (enemy.enemy_type) {
                        case ENEMY_NORMAL: {
                            // 普通：单发瞄准子弹
                            Bullet eb; eb.x = enemy.x; eb.y = enemy.y + 15.f;
                            eb.vx = dx / dist * spd; eb.vy = dy / dist * spd;
                            eb.life = 3.f; eb.homing = false;
                            eb.bullet_color = sf::Color(255, 150, 50);
                            enemy_bullets.push_back(eb);
                            break;
                        }
                        case ENEMY_DOUBLE_SHOOT: {
                            // 双发弹幕：2 发带 ±10° 扩散
                            float ba = std::atan2(dy, dx);
                            for (int j = -1; j <= 1; j += 2) {
                                float a = ba + float(j) * 0.175f;
                                Bullet eb; eb.x = enemy.x; eb.y = enemy.y + 15.f;
                                eb.vx = std::cos(a) * spd; eb.vy = std::sin(a) * spd;
                                eb.life = 3.f; eb.homing = false;
                                eb.bullet_color = sf::Color(255, 180, 80);
                                enemy_bullets.push_back(eb);
                            }
                            break;
                        }
                        case ENEMY_TRACKING: {
                            // 追踪敌人：单发瞄准
                            Bullet eb; eb.x = enemy.x; eb.y = enemy.y + 15.f;
                            eb.vx = dx / dist * spd; eb.vy = dy / dist * spd;
                            eb.life = 3.f; eb.homing = false;
                            eb.bullet_color = sf::Color(200, 100, 240);
                            enemy_bullets.push_back(eb);
                            break;
                        }
                        case ENEMY_ELITE: {
                            // 精英：3 发带 ±15° 扩散
                            float ba = std::atan2(dy, dx);
                            for (int j = -1; j <= 1; ++j) {
                                float a = ba + float(j) * 0.262f;
                                Bullet eb; eb.x = enemy.x; eb.y = enemy.y + 15.f;
                                eb.vx = std::cos(a) * spd; eb.vy = std::sin(a) * spd;
                                eb.life = 3.f; eb.homing = false;
                                eb.bullet_color = sf::Color(220, 60, 40);
                                enemy_bullets.push_back(eb);
                            }
                            break;
                        }
                        case ENEMY_BLOOM: {
                            // 开花弹：圆形散射（东方风格），子弹数 8+层数×2
                            int cnt = 8 + player_level * 2;
                            float off = random_float(0.f, pad);
                            for (int j = 0; j < cnt; ++j) {
                                float a = off + float(j) * pad / float(cnt);
                                Bullet eb; eb.x = enemy.x; eb.y = enemy.y;
                                eb.vx = std::cos(a) * spd; eb.vy = std::sin(a) * spd;
                                eb.life = 2.5f; eb.homing = false;
                                eb.bullet_color = sf::Color(60, 180, 255, 220);
                                enemy_bullets.push_back(eb);
                            }
                            break;
                        }
                        case ENEMY_SPIRAL: {
                            // 螺旋弹：2 臂旋转，角度持续递增
                            enemy.spiral_angle += 3.0f * dt;
                            for (int arm = 0; arm < 2; ++arm) {
                                float a = enemy.spiral_angle + float(arm) * 3.14159f;
                                Bullet eb; eb.x = enemy.x; eb.y = enemy.y;
                                eb.vx = std::cos(a) * spd * 0.8f;
                                eb.vy = std::sin(a) * spd * 0.8f;
                                eb.life = 2.0f; eb.homing = false;
                                eb.bullet_color = sf::Color(60, 220, 120, 200);
                                enemy_bullets.push_back(eb);
                            }
                            break;
                        }
                        case ENEMY_SPIRAL_ELITE: {
                            // 螺旋精英：3 臂旋转 + 追踪，角度更快
                            enemy.spiral_angle += 4.0f * dt;
                            for (int arm = 0; arm < 3; ++arm) {
                                float a = enemy.spiral_angle + float(arm) * 2.0944f;
                                Bullet eb; eb.x = enemy.x; eb.y = enemy.y;
                                eb.vx = std::cos(a) * spd * 0.9f;
                                eb.vy = std::sin(a) * spd * 0.9f;
                                eb.life = 2.2f; eb.homing = false;
                                eb.bullet_color = sf::Color(30, 200, 80, 220);
                                enemy_bullets.push_back(eb);
                            }
                            break;
                        }
                    }
                }

                // --- 玩家被敌人子弹击中 ---
                for (size_t i = 0; i < enemy_bullets.size(); ) {
                    if (check_collision(
                            player.pos.x - 14.f, player.pos.y - 18.f,
                            28.f, 36.f,
                            enemy_bullets[i].x - 4.f, enemy_bullets[i].y - 4.f,
                            8.f, 8.f)) {
                        bool still_alive = player.take_damage();
                        enemy_bullets.erase(enemy_bullets.begin() + i);
                        if (!still_alive) {
                            game_state = GameState::GAME_OVER;
                        }
                        break;
                    } else {
                        ++i;
                    }
                }

                // --- 检测升级 ---
                if (game_score >= next_level_threshold) {
                    game_state = GameState::LEVEL_UP;
                    level_up_panel.triggerLevelUp();
                }

                break;
            }

            // ==== 升级面板状态 ====
            case GameState::LEVEL_UP: {
                // 检测玩家选择
                int selected = level_up_panel.update(window);
                if (selected >= 0 && selected < ItemRegistry::itemCount()) {
                    std::unique_ptr<Item> picked(ItemFactory::create(selected));
                    if (picked) {
                        player.applyItem(picked.release());
                    }
                    // 记录本等级起始分（用于进度条清零）
                    score_at_level_start = game_score;
                    // 提高下一级阈值（二次增长：前期间隔小、后期显著拉大）
                    next_level_threshold += 500 + player_level * player_level * 80;
                    player_level++;
                    // 恢复游戏
                    game_state = GameState::PLAYING;
                }
                break;
            }

            // ==== 游戏结束状态 ====
            case GameState::GAME_OVER:
                // 结束画面不更新游戏逻辑
                break;
        }

        // ========== 渲染 ==========
        window.clear(sf::Color(20, 10, 30));  // 深紫色背景

        switch (game_state) {
            case GameState::MENU: {
                // 菜单标题
                sf::Text title(L"飞机大战 - 以撒版", game_font, 48);
                title.setFillColor(sf::Color::White);
                sf::FloatRect tb = title.getLocalBounds();
                title.setOrigin(tb.width / 2.f, 0.f);
                title.setPosition(400.f, 200.f);
                window.draw(title);

                // 副标题
                sf::Text subtitle(L"模仿以撒的结合属性机制", game_font, 24);
                subtitle.setFillColor(sf::Color(200, 200, 200));
                sf::FloatRect sb = subtitle.getLocalBounds();
                subtitle.setOrigin(sb.width / 2.f, 0.f);
                subtitle.setPosition(400.f, 280.f);
                window.draw(subtitle);

                // 操作说明
                sf::Text controls(L"WASD / 方向键 → 移动", game_font, 20);
                controls.setFillColor(sf::Color(180, 180, 180));
                sf::FloatRect cb = controls.getLocalBounds();
                controls.setOrigin(cb.width / 2.f, 0.f);
                controls.setPosition(400.f, 400.f);
                window.draw(controls);

                sf::Text controls2(L"空格 / J → 向上射击", game_font, 20);
                controls2.setFillColor(sf::Color(180, 180, 180));
                cb = controls2.getLocalBounds();
                controls2.setOrigin(cb.width / 2.f, 0.f);
                controls2.setPosition(400.f, 430.f);
                window.draw(controls2);

                sf::Text controls3(L"击杀敌人获得分数，分数达标后升级三选一", game_font, 18);
                controls3.setFillColor(sf::Color(150, 150, 150));
                cb = controls3.getLocalBounds();
                controls3.setOrigin(cb.width / 2.f, 0.f);
                controls3.setPosition(400.f, 480.f);
                window.draw(controls3);

                // 开始提示
                sf::Text start(L"按 Enter 开始游戏", game_font, 28);
                start.setFillColor(sf::Color(255, 255, 100));
                sb = start.getLocalBounds();
                start.setOrigin(sb.width / 2.f, 0.f);
                start.setPosition(400.f, 580.f);
                window.draw(start);

                break;
            }

            case GameState::PLAYING:
            case GameState::LEVEL_UP: {
                // --- 绘制背景星星 ---
                static std::vector<sf::Vector2f> stars;
                if (stars.empty()) {
                    for (int i = 0; i < 80; ++i) {
                        stars.push_back(sf::Vector2f(
                            random_float(0.f, 800.f),
                            random_float(0.f, 900.f)
                        ));
                    }
                }
                for (auto& star : stars) {
                    sf::CircleShape s(1.5f);
                    s.setPosition(star);
                    s.setFillColor(sf::Color(180, 180, 200, 100));
                    window.draw(s);
                    star.y += 30.f * dt;
                    if (star.y > 900.f) star.y = 0.f;
                }

                // --- 绘制道具拾取物 ---
                for (auto& pu : power_ups) {
                    if (!pu.active) continue;
                    sf::RectangleShape rect(sf::Vector2f(20.f, 20.f));
                    rect.setOrigin(10.f, 10.f);
                    rect.setPosition(pu.x, pu.y);
                    rect.setFillColor(sf::Color(255, 255, 0, 200));
                    rect.setOutlineThickness(1.f);
                    rect.setOutlineColor(sf::Color::White);
                    window.draw(rect);
                }

                // --- 绘制血量拾取物 (v3.3) ---
                for (auto& hd : health_drops) {
                    // 画一个红色红心
                    sf::CircleShape heart(10.f);
                    heart.setOrigin(10.f, 10.f);
                    heart.setPosition(hd.x, hd.y);
                    heart.setFillColor(sf::Color(255, 40, 40));
                    heart.setOutlineThickness(1.f);
                    heart.setOutlineColor(sf::Color(255, 150, 150));
                    window.draw(heart);
                    // 红心内部白色高光
                    sf::CircleShape shine(3.f);
                    shine.setOrigin(3.f, 3.f);
                    shine.setPosition(hd.x - 2.f, hd.y - 3.f);
                    shine.setFillColor(sf::Color(255, 180, 180, 180));
                    window.draw(shine);
                }

                // --- 绘制玩家子弹（半径由 generation 分支决定）---
                for (auto& b : bullets) {
                    sf::CircleShape circle(b.radius);
                    circle.setOrigin(b.radius, b.radius);
                    circle.setPosition(b.x, b.y);
                    sf::Color c = b.bullet_color;
                    if (b.has_parasite) {
                        c = sf::Color(140, 255, 100, c.a);
                    }
                    circle.setFillColor(c);
                    window.draw(circle);
                }

                // --- 绘制敌人子弹 ---
                for (auto& eb : enemy_bullets) {
                    sf::CircleShape circle(4.f);
                    circle.setOrigin(4.f, 4.f);
                    circle.setPosition(eb.x, eb.y);
                    circle.setFillColor(eb.bullet_color);
                    window.draw(circle);
                }

                // --- 绘制敌人 ---
                for (auto& enemy : enemies) {
                    // === 根据类型绘制不同外观 ===
                    sf::Color outline_c = sf::Color(80, 80, 80);
                    float outline_t = 2.f;

                    switch (enemy.enemy_type) {
                        case ENEMY_BLOOM: {
                            // 开花型：圆形
                            sf::CircleShape body(static_cast<float>(enemy.width) / 2.f);
                            body.setOrigin(static_cast<float>(enemy.width) / 2.f,
                                          static_cast<float>(enemy.width) / 2.f);
                            body.setPosition(enemy.x, enemy.y);
                            body.setFillColor(enemy.color);
                            body.setOutlineThickness(outline_t);
                            body.setOutlineColor(sf::Color(40, 120, 200));
                            window.draw(body);
                            break;
                        }
                        case ENEMY_SPIRAL:
                        case ENEMY_SPIRAL_ELITE: {
                            // 螺旋型：菱形
                            sf::ConvexShape diamond;
                            diamond.setPointCount(4);
                            float hw = static_cast<float>(enemy.width) / 2.f;
                            float hh = static_cast<float>(enemy.height) / 2.f;
                            diamond.setPoint(0, sf::Vector2f(0.f, -hh));
                            diamond.setPoint(1, sf::Vector2f(hw, 0.f));
                            diamond.setPoint(2, sf::Vector2f(0.f, hh));
                            diamond.setPoint(3, sf::Vector2f(-hw, 0.f));
                            diamond.setPosition(enemy.x, enemy.y);
                            diamond.setFillColor(enemy.color);
                            diamond.setOutlineThickness(outline_t);
                            diamond.setOutlineColor(sf::Color(30, 150, 60));
                            window.draw(diamond);
                            break;
                        }
                        default: {
                            // 普通/双发/追踪/精英：矩形
                            sf::RectangleShape body(sf::Vector2f(
                                static_cast<float>(enemy.width),
                                static_cast<float>(enemy.height)));
                            body.setOrigin(
                                static_cast<float>(enemy.width) / 2.f,
                                static_cast<float>(enemy.height) / 2.f);
                            body.setPosition(enemy.x, enemy.y);
                            body.setFillColor(enemy.color);
                            body.setOutlineThickness(outline_t);

                            // 类型特化轮廓色
                            switch (enemy.enemy_type) {
                                case ENEMY_DOUBLE_SHOOT:
                                    outline_c = sf::Color(200, 120, 30); break;
                                case ENEMY_TRACKING:
                                    outline_c = sf::Color(140, 40, 200); break;
                                case ENEMY_ELITE:
                                    outline_c = sf::Color(200, 20, 20); break;
                                default:
                                    outline_c = sf::Color(80, 80, 80); break;
                            }
                            body.setOutlineColor(outline_c);
                            window.draw(body);
                            break;
                        }
                    }

                    // 敌人血条（血量 > 1 时显示）
                    if (enemy.max_hp > 1) {
                        float bar_w = 30.f;
                        float bar_h = 4.f;
                        float hp_ratio = static_cast<float>(enemy.hp) /
                                        static_cast<float>(enemy.max_hp);

                        sf::RectangleShape hp_bar(sf::Vector2f(bar_w, bar_h));
                        hp_bar.setOrigin(bar_w / 2.f, 0.f);
                        hp_bar.setPosition(enemy.x, enemy.y - 22.f);
                        hp_bar.setFillColor(sf::Color(80, 30, 30));
                        window.draw(hp_bar);

                        sf::RectangleShape hp_fill(sf::Vector2f(bar_w * hp_ratio, bar_h));
                        hp_fill.setPosition(enemy.x - bar_w / 2.f, enemy.y - 22.f);
                        hp_fill.setFillColor(sf::Color(255, 50, 50));
                        window.draw(hp_fill);
                    }
                }

                // --- 绘制玩家 ---
                draw_player_shape(window, player);

                BabySystem::render(window, player);
                BrimstoneLaser::render(window, player);
                SplitLaserSystem::render(window, split_lasers);

                // --- 绘制粒子 ---
                for (auto& p : particles) {
                    float alpha = p.life / p.max_life;   // 根据剩余生命计算透明度
                    sf::Color c = p.color;
                    c.a = static_cast<sf::Uint8>(255 * alpha);
                    sf::CircleShape circle(3.f);
                    circle.setOrigin(3.f, 3.f);
                    circle.setPosition(p.x, p.y);
                    circle.setFillColor(c);
                    window.draw(circle);
                }

                // --- 波次公告 (v3.1) ---
                if (game_state == GameState::PLAYING && wave_pause && wave_pause_timer > 0.f) {
                    // 半透明黑色遮罩条
                    sf::RectangleShape banner_bg(sf::Vector2f(500.f, 60.f));
                    banner_bg.setOrigin(250.f, 30.f);
                    banner_bg.setPosition(400.f, 450.f);
                    banner_bg.setFillColor(sf::Color(0, 0, 0, 160));
                    window.draw(banner_bg);

                    // 公告文字
                    std::wstring wave_msg;
                    if (wave_pause_timer > 2.0f) {
                        wave_msg = L"第 " + std::to_wstring(player_level)
                                 + L" 层已清除！";
                    } else {
                        wave_msg = L"第 " + std::to_wstring(player_level)
                                 + L" 层 - 一大波敌人来袭！";
                    }

                    sf::Text wave_text(wave_msg, game_font, 26);
                    wave_text.setFillColor(sf::Color(255, 220, 100));
                    sf::FloatRect wtb = wave_text.getLocalBounds();
                    wave_text.setOrigin(wtb.width / 2.f, wtb.height / 2.f);
                    wave_text.setPosition(400.f, 440.f);
                    window.draw(wave_text);

                    // 倒计时
                    sf::Text count_text(L"下一波倒计时: "
                        + std::to_wstring(static_cast<int>(wave_pause_timer + 0.99f))
                        + L" 秒", game_font, 16);
                    count_text.setFillColor(sf::Color(200, 200, 200));
                    wtb = count_text.getLocalBounds();
                    count_text.setOrigin(wtb.width / 2.f, 0.f);
                    count_text.setPosition(400.f, 462.f);
                    window.draw(count_text);
                }

                // --- 绘制 UI ---
                // 血量（红心）
                draw_player_hp_hearts(window, player, 12.f, 52.f);

                // 分数
                sf::Text score_text(L"分数: " + std::to_wstring(game_score), game_font, 18);
                score_text.setFillColor(sf::Color::White);
                score_text.setPosition(12.f, 12.f);
                window.draw(score_text);

                // 等级
                sf::Text level_text(L"等级: " + std::to_wstring(player_level), game_font, 18);
                level_text.setFillColor(sf::Color(255, 255, 100));
                level_text.setPosition(12.f, 32.f);
                window.draw(level_text);

                // 升级进度（本等级内分数 / 本等级所需分数）
                float level_needed = static_cast<float>(next_level_threshold - score_at_level_start);
                float level_progress = static_cast<float>(game_score - score_at_level_start);
                float progress = level_needed > 0.f ? level_progress / level_needed : 0.f;
                if (progress > 1.f) progress = 1.f;
                if (progress < 0.f) progress = 0.f;
                sf::Text next_text(
                    L"下一级: " + std::to_wstring(next_level_threshold), game_font, 14);
                next_text.setFillColor(sf::Color(150, 150, 150));
                next_text.setPosition(12.f, 75.f);
                window.draw(next_text);

                // 进度条
                sf::RectangleShape prog_bar(sf::Vector2f(150.f, 6.f));
                prog_bar.setPosition(12.f, 95.f);
                prog_bar.setFillColor(sf::Color(40, 40, 40));
                window.draw(prog_bar);
                sf::RectangleShape prog_fill(sf::Vector2f(150.f * progress, 6.f));
                prog_fill.setPosition(12.f, 95.f);
                prog_fill.setFillColor(sf::Color(255, 200, 50));
                window.draw(prog_fill);

                // 道具计数
                sf::Text items_text(
                    L"道具: " + std::to_wstring(player.item_count), game_font, 14);
                items_text.setFillColor(sf::Color(200, 200, 200));
                items_text.setPosition(12.f, 108.f);
                window.draw(items_text);

                std::wstring held_str = L"层数: ";
                if (player.stats.brimstone_level > 0)
                    held_str += L"硫磺" + std::to_wstring(player.stats.brimstone_level) + L" ";
                if (player.stats.tracking_level > 0)
                    held_str += L"弯勺" + std::to_wstring(player.stats.tracking_level) + L" ";
                if (player.stats.extra_bullets > 0)
                    held_str += L"20/20+" + std::to_wstring(player.stats.extra_bullets) + L" ";
                if (player.stats.baby_count > 0)
                    held_str += L"宝宝" + std::to_wstring(player.stats.baby_count) + L" ";
                if (player.stats.has_parasite)
                    held_str += L"寄生虫 ";
                const AttackProfile hud_profile = buildAttackProfile(player);
                if (hud_profile.usesBrimstone()) {
                    held_str += L"[激光";
                    if (hud_profile.brimstone_level >= 2) held_str += L"粗";
                    if (hud_profile.usesHoming()) held_str += L"+追踪";
                    if (hud_profile.parallel_lanes > 1) held_str += L"+多道";
                    held_str += L"]";
                } else if (hud_profile.usesHoming() || hud_profile.parallel_lanes > 1) {
                    held_str += L"[";
                    if (hud_profile.parallel_lanes > 1) held_str += L"多弹道";
                    if (hud_profile.usesHoming()) held_str += L"+追踪";
                    held_str += L"]";
                }
                sf::Text held_text(held_str, game_font, 14);
                held_text.setFillColor(sf::Color(255, 220, 100));
                held_text.setPosition(12.f, 870.f);
                window.draw(held_text);

                // 伤害显示
                sf::Text dmg_text(
                    L"伤害: " + std::to_wstring(player.stats.damage).substr(0, 4),
                    game_font, 14);
                dmg_text.setFillColor(sf::Color(255, 150, 100));
                dmg_text.setPosition(12.f, 126.f);
                window.draw(dmg_text);

                // 升级进度条
                sf::RectangleShape exp_bar_bg(sf::Vector2f(200.f, 10.f));
                exp_bar_bg.setPosition(300.f, 10.f);
                exp_bar_bg.setFillColor(sf::Color(40, 40, 40));
                window.draw(exp_bar_bg);

                float fill_ratio = level_progress / (level_needed > 0.f ? level_needed : 1.f);
                if (fill_ratio > 1.f) fill_ratio = 1.f;
                if (fill_ratio < 0.f) fill_ratio = 0.f;
                sf::RectangleShape exp_bar_fill(sf::Vector2f(200.f * fill_ratio, 10.f));
                exp_bar_fill.setPosition(300.f, 10.f);
                exp_bar_fill.setFillColor(sf::Color(255, 200, 0));
                window.draw(exp_bar_fill);

                break;
            }

            case GameState::GAME_OVER: {
                // 游戏结束画面
                sf::Text over_text(L"游戏结束", game_font, 48);
                over_text.setFillColor(sf::Color(255, 80, 80));
                sf::FloatRect ob = over_text.getLocalBounds();
                over_text.setOrigin(ob.width / 2.f, 0.f);
                over_text.setPosition(400.f, 250.f);
                window.draw(over_text);

                sf::Text final_score(
                    L"最终分数: " + std::to_wstring(game_score), game_font, 28);
                final_score.setFillColor(sf::Color::White);
                sf::FloatRect fb = final_score.getLocalBounds();
                final_score.setOrigin(fb.width / 2.f, 0.f);
                final_score.setPosition(400.f, 330.f);
                window.draw(final_score);

                sf::Text final_level(
                    L"最高等级: " + std::to_wstring(player_level), game_font, 24);
                final_level.setFillColor(sf::Color(255, 255, 150));
                fb = final_level.getLocalBounds();
                final_level.setOrigin(fb.width / 2.f, 0.f);
                final_level.setPosition(400.f, 370.f);
                window.draw(final_level);

                sf::Text final_items(
                    L"收集道具: " + std::to_wstring(player.item_count) + L" 个",
                    game_font, 24);
                final_items.setFillColor(sf::Color(200, 200, 200));
                fb = final_items.getLocalBounds();
                final_items.setOrigin(fb.width / 2.f, 0.f);
                final_items.setPosition(400.f, 410.f);
                window.draw(final_items);

                sf::Text restart(L"按 Enter 重新开始", game_font, 28);
                restart.setFillColor(sf::Color(255, 255, 100));
                fb = restart.getLocalBounds();
                restart.setOrigin(fb.width / 2.f, 0.f);
                restart.setPosition(400.f, 500.f);
                window.draw(restart);

                break;
            }
        }

        // 如果处于升级面板状态，将面板盖在最上面
        if (game_state == GameState::LEVEL_UP) {
            level_up_panel.render(window);
        }

        // 交换缓冲区（显示画面）
        window.display();
    }

    return 0;
}
