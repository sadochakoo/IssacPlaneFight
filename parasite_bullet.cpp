#include "parasite_bullet.h"

#include "module3_tears.h"
#include "tear_profile.h"

#include <cmath>



#ifndef M_PI

#define M_PI 3.14159265358979323846

#endif



namespace {



float bullet_speed(const Bullet& bullet) {

    float speed = std::sqrt(bullet.vx * bullet.vx + bullet.vy * bullet.vy);

    if (speed < 1.f) {

        speed = static_cast<float>(bullet.vy < 0.f ? -bullet.vy : bullet.vy);

    }

    if (speed < 1.f) {

        speed = 600.f;

    }

    return speed;

}



void push_child_from_angle(const Bullet& parent,

                           float angle,

                           float speed,

                           float child_damage,

                           int child_generation,

                           std::vector<Bullet>& pending_bullets)

{

    Bullet child;

    child.x = parent.x;

    child.y = parent.y;

    child.vx = speed * std::sin(angle);

    child.vy = -speed * std::cos(angle);

    child.life = parent.life;

    child.homing_strength = parent.homing_strength;

    child.bullet_color = parent.bullet_color;

    child.has_parasite = true;

    child.is_baby_tear = parent.is_baby_tear;

    child.homing = parent.homing && !parent.is_baby_tear;

    child.generation = child_generation;

    child.damage = child_damage;

    child.bounce_split_cooldown = 6;

    child.update_radius_from_generation();

    pending_bullets.push_back(child);

}



} // namespace



void setup_player_bullet(Bullet& bullet, const Player& player, bool is_baby_tear) {

    bullet.has_parasite = player.stats.has_parasite;

    bullet.is_baby_tear = is_baby_tear;

    bullet.generation   = 0;

    bullet.damage       = static_cast<float>(player.stats.damage);

    bullet.bounce_split_cooldown = 0;

    bullet.update_radius_from_generation();

    module3::configure_player_bullet(bullet, player);
    apply_tear_visual_from_player(bullet, player);

}



bool can_parasite_split(const Bullet& bullet) {

    if (bullet.is_haemolacria_shard || bullet.is_haemolacria_orb) {

        return false;

    }

    if (bullet.generation > 0) {

        return false;

    }

    if (!bullet.has_parasite) {

        return false;

    }

    if (bullet.generation >= k_parasite_max_generation) {

        return false;

    }

    const float child_damage = bullet.damage * k_parasite_damage_decay;

    return child_damage >= k_parasite_min_damage;

}



void enqueue_parasite_hit_splits(const Bullet& parent,

                                 std::vector<Bullet>& pending_bullets)

{

    if (!can_parasite_split(parent)) {

        return;

    }



    const float child_damage = parent.damage * k_parasite_damage_decay;

    const int child_generation = parent.generation + 1;

    const float speed = bullet_speed(parent);



    // 四向十字：左斜前、右斜前、左斜后、右斜后（angle：0 = 向上）

    const float angles[4] = {

        -static_cast<float>(M_PI) * 0.25f,

         static_cast<float>(M_PI) * 0.25f,

        -static_cast<float>(M_PI) * 0.75f,

         static_cast<float>(M_PI) * 0.75f

    };



    for (float angle : angles) {

        push_child_from_angle(

            parent, angle, speed, child_damage, child_generation, pending_bullets);

    }

}



void enqueue_parasite_wall_splits(const Bullet& parent,

                                  std::vector<Bullet>& pending_bullets)

{

    if (!can_parasite_split(parent)) {

        return;

    }



    const float child_damage = parent.damage * k_parasite_damage_decay;

    const int child_generation = parent.generation + 1;

    const float speed = bullet_speed(parent);



    const float base_angle = std::atan2(parent.vx, -parent.vy);

    const float spread = static_cast<float>(M_PI) * 0.35f;

    const float angles[2] = { base_angle - spread, base_angle + spread };



    for (float angle : angles) {

        push_child_from_angle(

            parent, angle, speed, child_damage, child_generation, pending_bullets);

    }

}

