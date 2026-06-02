/*
 * main.cpp - 飞机大战：以撒的结合 风格重制
 * 版本：v2.3 (2026-06-02)
 * 技术栈：C++17 / SFML 2.6.x
 * 平台：Windows x64 (Visual Studio 2022)
 *
 * 更新日志：
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
 */

#include <SFML/Graphics.hpp>      // 图形渲染（窗口、形状、文字）
#include <vector>                  // 动态数组（子弹/敌人/粒子列表）
#include <cmath>                   // 数学函数（sin/cos/sqrt）
#include <random>                  // 随机数引擎
#include <cstdlib>                 // rand/srand
#include <ctime>                   // time
#include <string>                  // 字符串

#include "player_stats.h"          // 玩家属性系统
#include "level_up_panel.h"        // 三选一 UI 面板

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

// 粒子特效列表
std::vector<Particle> particles;

// 游戏分数
int game_score = 0;

// 升级阈值（分数达到阈值时触发升级）
int next_level_threshold = 500;
int player_level = 1;

// 敌人生成计时器
float enemy_spawn_timer = 0.f;
float enemy_spawn_interval = 2.5f;  // 基础生成间隔（随分数连续递减）

// 敌人射击计时器
float enemy_shoot_timer = 0.f;
float enemy_shoot_interval = 2.0f;  // 基础射击间隔（随分数连续递减）

// 道具掉落冷却（防止短时间内连续掉落）
float item_drop_cooldown = 20.f;     // 两次掉落最少间隔 20 秒
float item_drop_timer = 0.f;         // 当前冷却计时器
int   next_drop_score = 4000;        // 达到此分数后才允许掉落，掉落后递增

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

// ==================== 敌人生成系统 ====================

/*
 * spawn_enemy() - 生成一个敌人
 *
 * 敌人在屏幕上方随机位置生成，向下移动
 * 难度随分数提升：
 * - 基础 HP 1~3
 * - 每 500 分 +1 HP
 */
void spawn_enemy() {
    Enemy e;
    e.x = random_float(50.f, 750.f);
    e.y = -30.f;

    // 难度递增：每 250 分敌人全面增强
    int bonus_hp = game_score / 250;

    // HP：基础 1~3 + 分数加成
    e.max_hp = 1 + bonus_hp + random_int(1, 3);
    e.hp = e.max_hp;

    // 下落速度随分数递增（更快更危险）
    e.vy = random_float(100.f, 150.f) + bonus_hp * 15.f;

    // 横向飘移也随分数加大（更难躲避）
    float drift = 40.f + bonus_hp * 6.f;
    e.vx = random_float(-drift, drift);

    e.width = 30;
    e.height = 30;
    e.score = 50 + bonus_hp * 30;

    // 根据血量设置颜色（颜色越深越危险）
    if (e.max_hp <= 2) {
        e.color = sf::Color(255, 100, 100);     // 红色（简单）
    } else if (e.max_hp <= 5) {
        e.color = sf::Color(200, 50, 150);       // 紫色（中等）
    } else if (e.max_hp <= 10) {
        e.color = sf::Color(180, 30, 100);       // 深紫（困难）
    } else {
        e.color = sf::Color(150, 20, 30);        // 暗红（地狱）
    }

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
    if (random_float(0.f, 100.f) > 15.0f) return;  // 15% 掉落率

    // 触发冷却计时 + 提高下次掉落门槛
    item_drop_timer = item_drop_cooldown;
    next_drop_score = game_score + 4000;  // 下次掉落需要再得 4000 分

    PowerUp p;
    p.x = x;
    p.y = y;
    p.active = true;

    // 随机道具（按稀有度加权）
    int roll = random_int(0, 99);
    if      (roll < 10) p.item_type = ITEM_MAGIC_MUSHROOM;     // 10%
    else if (roll < 18) p.item_type = ITEM_LUNCH;              // 8%
    else if (roll < 25) p.item_type = ITEM_BLOOD_BAG;          // 7%
    else if (roll < 32) p.item_type = ITEM_SAD_ONION;          // 7%
    else if (roll < 38) p.item_type = ITEM_CRICKETS_HEAD;      // 6%
    else if (roll < 44) p.item_type = ITEM_SPEED_BALL;         // 6%
    else if (roll < 50) p.item_type = ITEM_CAT_O_NINE_TAILS;   // 6%
    else if (roll < 56) p.item_type = ITEM_MOMS_HEELS;         // 6%
    else if (roll < 62) p.item_type = ITEM_BOOK_OF_BELIAL;     // 6%
    else if (roll < 69) p.item_type = ITEM_SPOON_BENDER;       // 7%
    else if (roll < 74) p.item_type = ITEM_HALO;               // 5%
    else if (roll < 79) p.item_type = ITEM_WOODEN_SPOON;       // 5%
    else if (roll < 84) p.item_type = ITEM_MAXS_HEAD;          // 5%
    else if (roll < 89) p.item_type = ITEM_CUPIDS_ARROW;       // 5%
    else if (roll < 93) p.item_type = ITEM_GAMEKID;            // 4%
    else if (roll < 97) p.item_type = ITEM_HOLY_MANTLE;        // 4%
    else if (roll < 99) p.item_type = ITEM_NECRONOMICON;       // 2%
    else                p.item_type = ITEM_SUCCUBUS;            // 1%

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

    // 重置全局变量
    game_score = 0;
    next_level_threshold = 500;
    player_level = 1;
    enemy_spawn_timer = 0.f;
    enemy_shoot_timer = 0.f;
    enemy_spawn_interval = 2.5f;
    enemy_shoot_interval = 2.0f;
    item_drop_timer = 0.f;           // 重置掉落冷却
    next_drop_score = 4000;          // 重置掉落分数门槛

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
        body_color = sf::Color(100, 100, 255, 128);  // 隐形 → 半透明蓝色
    } else if (player.shield_timer != 0) {
        body_color = sf::Color(255, 255, 150);        // 护盾 → 黄色
    } else if (player.damage_boost_timer > 0) {
        body_color = sf::Color(255, 80, 80);          // 伤害增强 → 红色
    } else {
        body_color = sf::Color(255, 255, 255);        // 正常 → 白色
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

                // --- 玩家射击 ---
                // 空格键或 J 键发射子弹
                if (player.fire_cooldown > 0) {
                    player.fire_cooldown--;  // 冷却减少
                }
                if ((sf::Keyboard::isKeyPressed(sf::Keyboard::Space) ||
                     sf::Keyboard::isKeyPressed(sf::Keyboard::J)) &&
                    player.fire_cooldown <= 0) {

                    // 重置冷却（从 stats.tear_rate 读取射速）
                    player.fire_cooldown = player.stats.tear_rate;

                    // 主弹道 + 额外弹道
                    int total_bullets = 1 + player.stats.extra_bullets;

                    for (int b = 0; b < total_bullets; ++b) {
                        Bullet bullet;
                        bullet.x = player.pos.x + static_cast<float>(b - total_bullets / 2) * 8.f;
                        bullet.y = player.pos.y - 20.f;
                        bullet.vx = 0.f;
                        // 弹速从 stats.shot_speed 读取
                        bullet.vy = -static_cast<float>(player.stats.shot_speed);
                        // 射程从 stats.range 读取
                        bullet.life = static_cast<float>(player.stats.range);
                        // 检测追踪状态
                        bullet.tracking = (player.tracking_timer > 0);
                        bullet.target_enemy = -1;
                        bullets.push_back(bullet);
                    }

                    // 分身射击（如果有分身）
                    if (player.has_clone) {
                        for (int b = 0; b < total_bullets; ++b) {
                            Bullet bullet;
                            bullet.x = player.pos.x + 50.f + static_cast<float>(b) * 8.f;
                            bullet.y = player.pos.y - 15.f;
                            bullet.vx = 0.f;
                            bullet.vy = -static_cast<float>(player.stats.shot_speed);
                            bullet.life = static_cast<float>(player.stats.range);
                            bullet.tracking = false;
                            bullet.target_enemy = -1;
                            bullets.push_back(bullet);

                            bullet.x = player.pos.x - 50.f;
                            bullets.push_back(bullet);
                        }
                    }
                }

                // --- 更新子弹 ---
                for (size_t i = 0; i < bullets.size(); ) {
                    // 追踪子弹：自动锁定最近敌人
                    if (bullets[i].tracking) {
                        int target = find_nearest_enemy(bullets[i].x, bullets[i].y);
                        if (target >= 0) {
                            float dx = enemies[target].x - bullets[i].x;
                            float dy = enemies[target].y - bullets[i].y;
                            float dist = std::sqrt(dx * dx + dy * dy);
                            if (dist > 1.f) {
                                float speed = static_cast<float>(player.stats.shot_speed);
                                bullets[i].vx = dx / dist * speed;
                                bullets[i].vy = dy / dist * speed;
                            }
                        }
                    }

                    // 移动子弹
                    bullets[i].x += bullets[i].vx * dt;
                    bullets[i].y += bullets[i].vy * dt;
                    bullets[i].life -= dt;

                    // 到达射程或飞出屏幕 → 删除
                    if (bullets[i].life <= 0.f ||
                        bullets[i].x < -50.f || bullets[i].x > 850.f ||
                        bullets[i].y < -50.f || bullets[i].y > 950.f) {
                        bullets.erase(bullets.begin() + i);
                    } else {
                        ++i;
                    }
                }

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

                // --- 子弹与敌人碰撞检测 ---
                for (size_t bi = 0; bi < bullets.size(); ) {
                    bool bullet_hit = false;
                    for (size_t ei = 0; ei < enemies.size(); ++ei) {
                        if (check_collision(
                                bullets[bi].x - 4.f, bullets[bi].y - 8.f,
                                8.f, 16.f,
                                enemies[ei].x - 15.f, enemies[ei].y - 15.f,
                                30.f, 30.f)) {
                            // 子弹命中敌人
                            int dmg = player.get_damage();  // 从 stats 获取伤害
                            enemies[ei].hp -= dmg;

                            if (enemies[ei].hp <= 0) {
                                // 敌人死亡
                                game_score += enemies[ei].score;
                                spawn_particles(enemies[ei].x, enemies[ei].y,
                                               enemies[ei].color, 12);
                                // 概率掉落道具
                                spawn_item_pickup(enemies[ei].x, enemies[ei].y);
                                enemies.erase(enemies.begin() + ei);
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
                        player.apply_item(ITEM_POOL[power_ups[i].item_type].effect);
                        // 特效粒子
                        spawn_particles(power_ups[i].x, power_ups[i].y,
                                       sf::Color(255, 255, 100), 10);
                        power_ups.erase(power_ups.begin() + i);
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

                // --- 更新道具掉落冷却 ---
                if (item_drop_timer > 0.f) item_drop_timer -= dt;

                // --- 敌人生成（间隔随分数连续递减）---
                enemy_spawn_timer += dt;
                // 生成间隔：从 2.5 秒线性递减到最低 0.35 秒
                float dynamic_spawn_interval = std::max(0.35f, 2.5f - game_score * 0.0004f);
                if (enemy_spawn_timer >= dynamic_spawn_interval) {
                    enemy_spawn_timer = 0.f;
                    spawn_enemy();
                    // 高分时一次生成多个敌人
                    if (game_score > 3000 && random_float(0.f, 1.f) < 0.3f) spawn_enemy();
                    if (game_score > 6000 && random_float(0.f, 1.f) < 0.2f) spawn_enemy();
                }

                // --- 敌人射击（间隔 + 弹速均随分数递增）---
                enemy_shoot_timer += dt;
                float dynamic_shoot_interval = std::max(0.3f, 2.0f - game_score * 0.0003f);
                if (enemy_shoot_timer >= dynamic_shoot_interval) {
                    enemy_shoot_timer = 0.f;
                    for (auto& enemy : enemies) {
                        Bullet eb;
                        eb.x = enemy.x;
                        eb.y = enemy.y + 15.f;
                        // 瞄准玩家方向
                        float dx = player.pos.x - enemy.x;
                        float dy = player.pos.y - enemy.y;
                        float dist = std::sqrt(dx * dx + dy * dy);
                        // 弹速随分数递增：最快 550
                        float speed = 200.f + std::min(350.f, game_score * 0.08f);
                        if (dist > 1.f) {
                            eb.vx = dx / dist * speed;
                            eb.vy = dy / dist * speed;
                        } else {
                            eb.vx = 0.f;
                            eb.vy = speed;
                        }
                        eb.life = 3.f;  // 敌人子弹存活 3 秒
                        eb.tracking = false;
                        enemy_bullets.push_back(eb);
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
                if (selected >= 0) {
                    // 玩家选择了道具，应用效果
                    player.apply_item(ITEM_POOL[selected].effect);
                    // 提高下一级阈值（每级显著递增）
                    next_level_threshold += 500 + player_level * 150;
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

                sf::Text controls2(L"空格 / J → 射击", game_font, 20);
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

                // --- 绘制玩家子弹 ---
                for (auto& b : bullets) {
                    sf::RectangleShape rect(sf::Vector2f(6.f, 14.f));
                    rect.setOrigin(3.f, 7.f);
                    rect.setPosition(b.x, b.y);
                    if (b.tracking) {
                        rect.setFillColor(sf::Color(255, 100, 255));  // 追踪 → 粉色
                    } else {
                        rect.setFillColor(sf::Color(100, 200, 255));  // 普通 → 淡蓝
                    }
                    window.draw(rect);
                }

                // --- 绘制敌人子弹 ---
                for (auto& eb : enemy_bullets) {
                    sf::CircleShape circle(4.f);
                    circle.setOrigin(4.f, 4.f);
                    circle.setPosition(eb.x, eb.y);
                    circle.setFillColor(sf::Color(255, 150, 50));  // 橙色
                    window.draw(circle);
                }

                // --- 绘制敌人 ---
                for (auto& enemy : enemies) {
                    sf::RectangleShape body(sf::Vector2f(
                        static_cast<float>(enemy.width),
                        static_cast<float>(enemy.height)));
                    body.setOrigin(
                        static_cast<float>(enemy.width) / 2.f,
                        static_cast<float>(enemy.height) / 2.f);
                    body.setPosition(enemy.x, enemy.y);
                    body.setFillColor(enemy.color);
                    body.setOutlineThickness(2.f);
                    body.setOutlineColor(sf::Color(80, 80, 80));
                    window.draw(body);

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

                // --- 绘制分身 ---
                if (player.has_clone) {
                    draw_clone_shape(window, player);
                }

                // --- 绘制玩家 ---
                draw_player_shape(window, player);

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

                // 升级进度
                float progress = static_cast<float>(game_score) /
                                static_cast<float>(next_level_threshold);
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

                float fill_ratio = static_cast<float>(game_score % next_level_threshold) /
                                  static_cast<float>(next_level_threshold);
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
