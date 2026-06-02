/*
 * player_stats.h - 以撒的结合风格属性系统核心头文件
 *
 * 本文件定义了完整的以撒风格属性系统，包括：
 * 1. PlayerStats - 玩家基础属性结构体
 * 2. ItemEffect - 道具效果结构体
 * 3. Item - 道具定义结构体
 * 4. Player - 玩家类，包含 apply_item() 核心函数
 *
 * 命名规范：全小写下划线命名法 (snake_case)
 * 编码：UTF-8 with BOM（VS 原生支持）
 * 依赖库：SFML 2.6.x
 */

#ifndef PLAYER_STATS_H
#define PLAYER_STATS_H

// ==================== 头文件包含 ====================
#include <SFML/Graphics.hpp>   // SFML 图形库（Color, RenderWindow 等）
#include <string>               // STL 字符串
#include <cmath>                // C 标准数学库（ceil, sqrt 等）

// ==================== 基础属性结构体 ====================
/*
 * PlayerStats - 玩家基础属性结构体
 *
 * 完全仿照《以撒的结合》底层机制设计
 * 所有属性支持浮点数值（以撒特色：伤害可以是 3.50）
 *
 * 属性说明：
 * - hp: 当前生命值（红心），初始 3.0
 * - max_hp: 最大红心容器数，上限 12
 * - damage: 伤害值（浮点叠加），初始 3.50
 * - tear_rate: 射速（帧间隔，越小越快），范围 [2, 30]
 * - shot_speed: 子弹速度（像素/秒），初始 600.0
 * - range: 射程（子弹存活秒数），初始 1.2 秒
 * - speed: 移动速度（像素/秒），初始 320.0
 * - extra_bullets: 额外弹道数，上限 3
 */
struct PlayerStats {
    double hp           = 3.0;     // 当前红血
    int    max_hp       = 3;       // 最大红血容器
    double damage       = 3.50;    // 伤害值
    int    tear_rate    = 12;      // 射速（帧间隔）
    double shot_speed   = 600.0;   // 弹速
    double range        = 1.2;     // 射程（秒）
    double speed        = 320.0;   // 移速
    int    extra_bullets = 0;      // 额外弹道数
};

// ==================== 道具效果结构体 ====================
/*
 * ItemEffect - 道具效果结构体
 *
 * 每种道具对 PlayerStats 的修改量
 * 一个道具可以同时修改多个属性
 *
 * 设计思路：
 * - 永久属性修改：直接叠加到 stats 上
 * - 定时效果：启动计时器，到期恢复
 * - 即时效果：立即生效（清屏、满血等）
 *
 * 注意：使用默认成员初始化器（而不是自定义构造函数），
 *       这样 ItemEffect 保持为聚合类型，
 *       可以被 ITEM_POOL 的初始化列表直接初始化。
 */
struct ItemEffect {
    // ---- 永久属性修改（可叠加）----
    double hp_mod            = 0.0;  // HP 治疗量（正=回血，负=扣血）
    int    max_hp_mod        = 0;    // 最大红血容器变化（正=增加容器）
    double damage_mod        = 0.0;  // 伤害加成（叠加到 stats.damage）
    int    tear_rate_mod     = 0;    // 射速修改（负=减间隔→更快）
    double shot_speed_mod    = 0.0;  // 弹速修改（像素/秒）
    double range_mod         = 0.0;  // 射程修改（秒）
    double speed_mod         = 0.0;  // 移速修改（像素/秒）
    int    extra_bullets_mod = 0;    // 额外弹道数修改

    // ---- 特殊效果（布尔）----
    bool has_timed_effect = false;  // 是否有持续时间效果
    int  duration_frames  = 0;      // 持续时间（帧），-1 表示永久

    bool tracking    = false;       // 追踪子弹（子弹自动追踪敌人）
    bool invisible   = false;       // 隐形（免疫伤害）
    bool shield      = false;       // 护盾（吸收一次伤害）
    bool bomb_effect = false;       // 清屏炸弹（立即生效，摧毁所有敌人）
    bool clone_effect= false;       // 分身（召唤分身协助攻击）
    bool full_heal   = false;       // 满血恢复（立即回满血）
};

// ==================== 道具类型枚举 ====================
/*
 * ItemType - 道具类型枚举
 *
 * 19 种以撒风格道具，分为三类：
 * - 属性类：永久修改 PlayerStats 中的属性值
 * - 定时类：在一定时间内提供特殊效果
 * - 特殊类：即时效果（清屏、分身、满血）
 */
enum ItemType {
    // === 属性类道具 ===
    ITEM_SAD_ONION       = 0,   // 伤心洋葱：伤害+0.7
    ITEM_INNER_EYE       = 1,   // 内眼：额外弹道+1
    ITEM_SPOON_BENDER    = 2,   // 弯勺魔法：追踪子弹
    ITEM_CRICKETS_HEAD   = 3,   // 蟋蟀头：伤害+0.5 弹速+120
    ITEM_MAGIC_MUSHROOM  = 4,   // 魔法蘑菇：全属性提升+满血
    ITEM_HALO            = 5,   // 光环：全属性微量提升
    ITEM_MOMS_HEELS      = 6,   // 妈妈的高跟鞋：伤害+0.5 移速+100
    ITEM_SPEED_BALL      = 7,   // 速度球：移速+160
    ITEM_CAT_O_NINE_TAILS= 8,   // 九尾猫：射速-4（大幅提升）
    ITEM_BLOOD_BAG       = 9,   // 血袋：最大红血+1 回血+1
    ITEM_LUNCH           = 10,  // 午餐：最大红血+1 回血+3
    ITEM_WOODEN_SPOON    = 11,  // 木勺：移速+100
    ITEM_MAXS_HEAD       = 12,  // 马克思的头：伤害+0.5 射速-2
    ITEM_CUPIDS_ARROW    = 13,  // 丘比特之箭：弹速+300 射程+0.5

    // === 定时类道具 ===
    ITEM_BOOK_OF_BELIAL  = 14,  // 彼列之书：伤害+2.0（持续 360 帧）
    ITEM_GAMEKID         = 15,  // 游戏小子：护盾（持续 600 帧）

    // === 特殊类道具 ===
    ITEM_HOLY_MANTLE     = 16,  // 神圣斗篷：一次性护盾
    ITEM_NECRONOMICON    = 17,  // 死灵书：清屏炸弹（立即）
    ITEM_SUCCUBUS        = 18,  // 淫魔：召唤分身（永久）

    ITEM_COUNT           = 19   // 道具总数（用于数组大小）
};

// ==================== 道具定义结构体 ====================
/*
 * Item - 道具定义结构体
 *
 * 包含：
 * - name: 道具中文名称
 * - description: 道具描述
 * - color: SFML 颜色（卡牌显示用）
 * - effect: 道具效果数据
 */
struct Item {
    const wchar_t*  name;         // 道具中文名称（宽字符）
    const wchar_t*  description;  // 道具描述（宽字符）
    sf::Color       color;        // 卡牌颜色
    ItemEffect      effect;       // 道具效果
};

// ==================== 全局道具池 ====================
/*
 * ITEM_POOL - 所有 19 种道具的数据表
 *
 * 排列顺序：按 ItemType 枚举的顺序
 * 初始化方式：C++11 聚合初始化列表
 *   每个 ItemEffect 的 16 个字段依次为：
 *   { hp_mod, max_hp_mod, damage_mod, tear_rate_mod,
 *     shot_speed_mod, range_mod, speed_mod, extra_bullets_mod,
 *     has_timed_effect, duration_frames,
 *     tracking, invisible, shield, bomb_effect, clone_effect, full_heal }
 */
const Item ITEM_POOL[ITEM_COUNT] = {
    // ===== 属性类 =====
    { L"伤心洋葱",      L"伤害 +0.7",                              sf::Color(200, 200, 100),
        {0, 0, 0.70, 0,    0,   0,   0,   0,   false,0,   false,false,false,false,false,false} },

    { L"内眼",          L"额外弹道 +1",                           sf::Color(0, 255, 100),
        {0, 0, 0,    0,    0,   0,   0,   1,   false,0,   false,false,false,false,false,false} },

    { L"弯勺魔法",      L"追踪子弹（持续 300 帧）",               sf::Color(255, 100, 255),
        {0, 0, 0,    0,    0,   0,   0,   0,   true,300, true, false,false,false,false,false} },

    { L"蟋蟀头",        L"伤害 +0.5, 弹速 +120",                  sf::Color(180, 100, 50),
        {0, 0, 0.50, 0,   120,  0,   0,   0,   false,0,   false,false,false,false,false,false} },

    { L"魔法蘑菇",      L"全属性提升 + 满血恢复",                 sf::Color(255, 50, 50),
        {9, 1, 1.00, -2,  150,  0.3, 100, 1,   false,0,   false,false,false,false,false,true} },

    { L"光环",          L"全属性微量提升",                         sf::Color(255, 255, 150),
        {0, 0, 0.30, -1,  50,   0.1, 50,  0,   false,0,   false,false,false,false,false,false} },

    { L"妈妈的高跟鞋",  L"伤害 +0.5, 移速 +100",                  sf::Color(255, 80, 80),
        {0, 0, 0.50, 0,    0,   0,   100, 0,   false,0,   false,false,false,false,false,false} },

    { L"速度球",        L"移速 +160",                             sf::Color(100, 200, 255),
        {0, 0, 0,    0,    0,   0,   160, 0,   false,0,   false,false,false,false,false,false} },

    { L"九尾猫",        L"射速大幅提升(-4)",                      sf::Color(200, 200, 200),
        {0, 0, 0,   -4,    0,   0,   0,   0,   false,0,   false,false,false,false,false,false} },

    { L"血袋",          L"最大红血 +1, 回血 +1",                  sf::Color(200, 0, 0),
        {1, 1, 0,    0,    0,   0,   0,   0,   false,0,   false,false,false,false,false,false} },

    { L"午餐",          L"最大红血 +1, 回血 +3",                  sf::Color(255, 180, 100),
        {3, 1, 0,    0,    0,   0,   0,   0,   false,0,   false,false,false,false,false,false} },

    { L"木勺",          L"移速 +100",                             sf::Color(180, 140, 100),
        {0, 0, 0,    0,    0,   0,   100, 0,   false,0,   false,false,false,false,false,false} },

    { L"马克思的头",    L"伤害 +0.5, 射速 -2",                    sf::Color(100, 100, 100),
        {0, 0, 0.50, -2,    0,   0,   0,   0,   false,0,   false,false,false,false,false,false} },

    { L"丘比特之箭",    L"弹速 +300, 射程 +0.5",                  sf::Color(255, 150, 200),
        {0, 0, 0,    0,   300,  0.5, 0,   0,   false,0,   false,false,false,false,false,false} },

    // ===== 定时类 =====
    { L"彼列之书",      L"伤害 +2.0（持续 360 帧 / 6 秒）",      sf::Color(200, 50, 50),
        {0, 0, 2.00, 0,    0,   0,   0,   0,   true,360,  false,false,false,false,false,false} },

    { L"游戏小子",      L"护盾（持续 600 帧 / 10 秒）",           sf::Color(50, 200, 255),
        {0, 0, 0,    0,    0,   0,   0,   0,   true,600,  false,false,true, false,false,false} },

    // ===== 特殊类 =====
    { L"神圣斗篷",      L"一次性护盾（永久存在直到被打破）",       sf::Color(255, 230, 150),
        {0, 0, 0,    0,    0,   0,   0,   0,   false,-1,  false,false,true, false,false,false} },

    { L"死灵书",        L"消灭屏幕上所有敌人",                     sf::Color(80, 20, 80),
        {0, 0, 0,    0,    0,   0,   0,   0,   false,0,   false,false,false,true, false,false} },

    { L"淫魔",          L"召唤永久分身协助攻击",                   sf::Color(255, 100, 100),
        {0, 0, 0,    0,    0,   0,   0,   0,   false,-1,  false,false,false,false,true, false} },
};

// ==================== 子弹结构体 ====================
/*
 * Bullet - 子弹结构体
 *
 * 子弹由玩家发射，飞行一段距离后消失
 * 追踪子弹会自动追踪最近的敌人
 */
struct Bullet {
    float x, y;            // 子弹当前位置（像素）
    float vx, vy;          // 子弹速度向量（像素/秒）
    float life;            // 剩余生命（秒），到期销毁
    bool  tracking;        // 是否为追踪子弹
    int   target_enemy;    // 追踪目标敌人索引，-1 表示无目标
};

// ==================== 道具拾取物结构体 ====================
/*
 * PowerUp - 地图上的道具拾取物
 *
 * 敌人被击杀时有一定概率掉落
 * 玩家碰到后拾取并应用道具效果
 */
struct PowerUp {
    float x, y;           // 位置（像素）
    int   item_type;      // 道具类型（ITEM_POOL 索引）
    bool  active;         // 是否仍在地图上
};

// ==================== 敌人结构体 ====================
/*
 * Enemy - 敌人结构体
 *
 * 敌人从屏幕上方生成，向下移动
 * 被玩家子弹击中后扣血，找到 0 后死亡
 */
struct Enemy {
    float x, y;            // 敌人位置（像素）
    float vx, vy;          // 移动速度（像素/秒）
    int   hp;              // 当前生命值
    int   max_hp;          // 最大生命值
    int   width, height;   // 碰撞检测宽高（像素）
    int   score;           // 击杀分数
    sf::Color color;       // 敌人颜色（区分类型）
};

// ==================== 粒子结构体 ====================
/*
 * Particle - 粒子特效结构体
 *
 * 用于敌人死亡、道具拾取等视觉效果
 */
struct Particle {
    float x, y;            // 粒子位置
    float vx, vy;          // 速度
    float life;            // 剩余生命（秒）
    float max_life;        // 最大生命（用于透明度计算）
    sf::Color color;       // 粒子颜色
};

// ==================== 玩家类 ====================
/*
 * Player - 玩家类
 *
 * 核心功能：
 * 1. 维护 PlayerStats（当前属性）和 base_stats（永久属性快照）
 * 2. apply_item() 动态修改属性（以撒风格）
 * 3. 管理定时效果计时器（追踪、隐形、护盾、伤害增强等）
 * 4. shoot() / take_damage() 战斗逻辑
 *
 * 属性路线：
 *   ITEM_POOL → apply_item(ItemEffect) → stats → 游戏逻辑读取
 *                                        ↓
 *                                   base_stats（备份）
 */
class Player {
public:
    // === 玩家位置 ===
    sf::Vector2f pos;          // 玩家当前位置
    PlayerStats  stats;        // 当前属性（游戏逻辑读取）
    PlayerStats  base_stats;   // 永久属性快照（定时效果到期时恢复）

    // === 射击冷却 ===
    int fire_cooldown;         // 射击冷却剩余帧数（每帧 -1，到 0 才可射击）

    // === 定时效果计时器 ===
    int  tracking_timer;       // 追踪子弹剩余帧数
    int  invisible_timer;      // 隐形剩余帧数
    int  shield_timer;         // 护盾剩余帧数（-1=一次性护盾）
    int  damage_boost_timer;   // 伤害增强剩余帧数
    bool has_clone;            // 是否有分身

    // === 道具收集历史 ===
    int  item_count;           // 已收集道具总数

    // === 构造函数 ===
    Player() {
        // 初始化位置：屏幕底部中间
        pos = sf::Vector2f(400.f, 700.f);

        // stats 使用默认值（PlayerStats 的类内初始化器）
        // base_stats 初始与 stats 相同
        base_stats = stats;

        // 射击冷却从 0 开始（可以立即射击）
        fire_cooldown = 0;

        // 所有计时器从 0 开始（无激活效果）
        tracking_timer    = 0;
        invisible_timer   = 0;
        shield_timer      = 0;
        damage_boost_timer = 0;
        has_clone = false;

        item_count = 0;
    }

    /*
     * apply_item() - 应用道具效果（核心函数）
     *
     * 参数：const ItemEffect& e - 道具效果数据（引用传递，避免拷贝）
     * 返回值：无
     *
     * 处理流程（5 步）：
     * 1. 检查即时效果（bomb_effect / clone_effect / full_heal）→ 立即生效
     * 2. 叠加永久属性修改（hp / max_hp / damage 等）→ 修改 stats
     * 3. 属性边界检查（防止属性超过合理范围）
     * 4. 同步 base_stats = stats（快照当前永久属性）
     * 5. 激活定时效果（tracking / invisible / shield / 伤害增强）→ 启动计时器
     */
    void apply_item(const ItemEffect& e) {
        // ---- 步骤 1：即时效果 ----
        // 清屏炸弹：标记为炸弹效果（由主循环处理）
        // 分身：标记为有分身（由主循环处理）
        // 满血恢复：立即回满血
        if (e.full_heal) {
            stats.hp = static_cast<double>(stats.max_hp);
        }
        if (e.clone_effect) {
            has_clone = true;
        }

        // ---- 步骤 2：永久属性修改 ----
        stats.hp            += e.hp_mod;
        stats.max_hp        += e.max_hp_mod;
        stats.damage        += e.damage_mod;
        stats.tear_rate     += e.tear_rate_mod;
        stats.shot_speed    += e.shot_speed_mod;
        stats.range         += e.range_mod;
        stats.speed         += e.speed_mod;
        stats.extra_bullets += e.extra_bullets_mod;

        // ---- 步骤 3：属性边界检查 ----
        // HP 不能超过最大 HP
        if (stats.hp > static_cast<double>(stats.max_hp)) {
            stats.hp = static_cast<double>(stats.max_hp);
        }
        if (stats.hp < 0.0) {
            stats.hp = 0.0;
        }
        // max_hp 上限 12（以撒中最大红心数）
        if (stats.max_hp > 12) {
            stats.max_hp = 12;
        }
        if (stats.max_hp < 1) {
            stats.max_hp = 1;
        }
        // tear_rate 范围 [2, 30]（帧）
        if (stats.tear_rate < 2) {
            stats.tear_rate = 2;
        }
        if (stats.tear_rate > 30) {
            stats.tear_rate = 30;
        }
        // speed 范围 [150, 800]
        if (stats.speed < 150.0) {
            stats.speed = 150.0;
        }
        if (stats.speed > 800.0) {
            stats.speed = 800.0;
        }
        // range 范围 [0.3, 3.0] 秒
        if (stats.range < 0.3) {
            stats.range = 0.3;
        }
        if (stats.range > 3.0) {
            stats.range = 3.0;
        }
        // extra_bullets 上限 3
        if (stats.extra_bullets > 3) {
            stats.extra_bullets = 3;
        }

        // ---- 步骤 4：同步 base_stats（永久属性快照）----
        // 定时效果到期时，从此处恢复
        base_stats = stats;

        // ---- 步骤 5：定时效果 ----
        // 追踪子弹
        if (e.tracking && e.duration_frames > 0) {
            tracking_timer = e.duration_frames;
        }
        // 隐形
        if (e.invisible && e.duration_frames > 0) {
            invisible_timer = e.duration_frames;
        }
        // 护盾（duration_frames=-1 表示一次性护盾，一次性打开）
        if (e.shield) {
            shield_timer = e.duration_frames;
        }
        // 伤害增强（通过 has_timed_effect 且 damage_mod > 0 且不是 tracking/invisible/shield 来检测）
        if (e.has_timed_effect && e.duration_frames > 0
            && e.damage_mod > 0.0 && !e.tracking && !e.invisible && !e.shield) {
            damage_boost_timer = e.duration_frames;
        }

        // 计数器 +1
        item_count++;
    }

    /*
     * update_timers() - 每帧更新计时器
     *
     * 功能：所有激活的定时效果倒计时 -1
     *       到期时自动恢复对应的属性
     */
    void update_timers() {
        // 追踪子弹计时器
        if (tracking_timer > 0) {
            tracking_timer--;
        }
        // 隐形计时器
        if (invisible_timer > 0) {
            invisible_timer--;
        }
        // 护盾计时器（> 0 才递减，-1 表示一次性不递减）
        if (shield_timer > 0) {
            shield_timer--;
        }
        // 伤害增强计时器：到期时恢复伤害
        if (damage_boost_timer > 0) {
            damage_boost_timer--;
            if (damage_boost_timer == 0) {
                // 恢复到永久属性值
                stats.damage = base_stats.damage;
            }
        }
    }

    /*
     * get_effective_damage() - 获取当前实际伤害（向上取整）
     *
     * 返回值：int - 实际伤害值
     *
     * 以撒中伤害用浮点计算，但扣血用整数
     * 使用 ceil() 向上取整，保证伤害感觉
     */
    int get_damage() const {
        return static_cast<int>(std::ceil(stats.damage));
    }

    /*
     * is_alive() - 判断玩家是否存活
     */
    bool is_alive() const {
        return stats.hp > 0.0;
    }

    /*
     * take_damage() - 玩家受伤
     *
     * 优先级：
     * 1. 护盾：吸收伤害，护盾消失
     * 2. 隐形：完全免疫
     * 3. 正常：HP -= 1.0
     *
     * 返回值：true=玩家仍存活，false=玩家死亡
     */
    bool take_damage() {
        // 优先级 1：护盾吸收伤害
        if (shield_timer != 0) {
            shield_timer = 0;   // 护盾消失
            return true;        // 无伤
        }
        // 优先级 2：隐形免疫
        if (invisible_timer > 0) {
            return true;        // 无伤
        }
        // 优先级 3：正常受伤
        stats.hp -= 1.0;
        return stats.hp > 0.0;
    }
};

// ==================== 游戏常量 ====================
/*
 * 窗口尺寸和基本参数
 */
const int   SCREEN_WIDTH   = 800;    // 屏幕宽度（像素）
const int   SCREEN_HEIGHT  = 900;    // 屏幕高度（像素）
const int   PLAYER_WIDTH   = 32;     // 玩家宽度
const int   PLAYER_HEIGHT  = 32;     // 玩家高度
const float FRAME_TIME     = 1.f / 60.f;  // 每帧时长（秒），60 FPS

#endif // PLAYER_STATS_H
