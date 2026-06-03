/*
 * weapon_profile.h - 武器攻击模式（融合结果的数据载体）
 */

#ifndef WEAPON_PROFILE_H
#define WEAPON_PROFILE_H

#include <SFML/Graphics.hpp>

enum class WeaponMode {
    NormalUp,           // 默认：向上单发
    DoubleUp,           // 20/20：向上双发
    HomingUp,           // 魔术弯勺：向上 + vx 追踪
    BrimstoneLaser,     // 硫磺火：蓄力激光
    BrimstoneLaserDouble,   // 硫磺火 + 20/20
    BrimstoneLaserHoming    // 硫磺火 + 魔术弯勺 → 追踪激光
};

struct WeaponProfile {
    WeaponMode mode = WeaponMode::NormalUp;

    int   parallel_shots = 1;
    float parallel_spacing = 22.f;

    bool  bullet_homing = false;
    float homing_strength = 420.f;

    sf::Color bullet_color = sf::Color(100, 200, 255);

    bool  use_laser = false;
    bool  laser_homing = false;
    float laser_width = 20.f;
    float laser_duration = 0.7f;
    float laser_charge_time = 0.45f;
    float laser_damage_interval = 0.08f;
};

#endif
