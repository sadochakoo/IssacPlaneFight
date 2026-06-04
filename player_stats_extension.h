/*
 * player_stats_extension.h - 模块一被动/场控状态（属性面板、test_combat_status.json）
 */

#ifndef PLAYER_STATS_EXTENSION_H
#define PLAYER_STATS_EXTENSION_H

struct PlayerStatsExtension {
    float base_speed = 320.f;
    float current_speed_multiplier = 1.f;

    int  mirror_clone_level = 0;
    bool has_glass_shard    = false;
    bool has_tiny_planet    = false;
    bool has_godhead        = false;
    bool has_apple          = false;
    bool has_spike_nail     = false; // 八寸钉：径向击退 + 残影/冲击波
    bool has_ice_baby       = false; // 冰块宝宝：弹跳冰弹 → 冰冻敌人
    bool has_betrayal       = false; // 背叛道具：命中时魅惑敌人并倒戈

    bool is_speed_debuff_active(float current_speed) const {
        return current_speed_multiplier < 0.999f
               || current_speed < base_speed - 0.5f;
    }
};

#endif
