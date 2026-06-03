/*
 * passive_items.cpp - 以撒经典被动道具实现
 */

#include "passive_item.h"
#include "haemolacria.h"

// ---------- 魔术弯勺 (Spoon Bender) ----------
class SpoonBenderItem : public Item {
public:
    std::string getId() const override { return ItemIds::SPOON_BENDER; }

    ItemDisplay getDisplay() const override {
        return { L"魔术弯勺", L"子弹变紫并轻微追踪最近敌机", sf::Color(200, 100, 255) };
    }

    void applyTo(Player& player) override {
        ++player.stats.tracking_level;
    }
};

// ---------- 硫磺火 (Brimstone) ----------
class BrimstoneItem : public Item {
public:
    std::string getId() const override { return ItemIds::BRIMSTONE; }

    ItemDisplay getDisplay() const override {
        return { L"硫磺火", L"蓄力发射贯穿屏幕的红色激光", sf::Color(220, 40, 40) };
    }

    void applyTo(Player& player) override {
        ++player.stats.brimstone_level;
        player.stats.damage += 1.0;
        player.stats.tear_rate += 3;
        player.clampStats();
    }
};

// ---------- 20/20 ----------
class TwentyTwentyItem : public Item {
public:
    std::string getId() const override { return ItemIds::TWENTY_TWENTY; }

    ItemDisplay getDisplay() const override {
        return { L"20/20", L"向上平行发射两发子弹", sf::Color(180, 220, 255) };
    }

    void applyTo(Player& player) override {
        ++player.stats.extra_bullets;
    }
};

// ---------- 寄生虫 (Parasite) ----------
class ParasiteItem : public Item {
public:
    std::string getId() const override { return ItemIds::PARASITE; }

    ItemDisplay getDisplay() const override {
        return { L"寄生虫", L"命中后 V 字分裂，伤害逐代减半", sf::Color(180, 255, 120) };
    }

    void applyTo(Player& player) override {
        player.stats.has_parasite = true;
    }
};

// ---------- 泪血症 (Haemolacria) ----------
class HaemolacriaItem : public Item {
public:
    std::string getId() const override { return ItemIds::HAEMOLACRIA; }

    ItemDisplay getDisplay() const override {
        return { L"泪血症", L"慢速发射血球，落地 360° 爆裂", sf::Color(140, 0, 30) };
    }

    void applyTo(Player& player) override {
        player.stats.has_haemolacria = true;
        player.stats.damage *= HaemolacriaSystem::k_damage_multiplier;
        player.stats.tear_rate *= 3;
        if (player.stats.tear_rate < HaemolacriaSystem::k_tear_rate_after_pickup) {
            player.stats.tear_rate = HaemolacriaSystem::k_tear_rate_after_pickup;
        }
        player.clampStats();
    }
};

// ---------- 工厂 ----------
std::unique_ptr<Item> ItemFactory::create(int registry_index) {
    switch (registry_index) {
        case 0: return std::make_unique<SpoonBenderItem>();
        case 1: return std::make_unique<BrimstoneItem>();
        case 2: return std::make_unique<TwentyTwentyItem>();
        case 3: return std::make_unique<ParasiteItem>();
        case 4: return std::make_unique<HaemolacriaItem>();
        default: return nullptr;
    }
}

std::unique_ptr<Item> ItemFactory::createById(const std::string& id) {
    if (id == ItemIds::SPOON_BENDER)  return std::make_unique<SpoonBenderItem>();
    if (id == ItemIds::BRIMSTONE)     return std::make_unique<BrimstoneItem>();
    if (id == ItemIds::TWENTY_TWENTY) return std::make_unique<TwentyTwentyItem>();
    if (id == ItemIds::PARASITE)      return std::make_unique<ParasiteItem>();
    if (id == ItemIds::HAEMOLACRIA)   return std::make_unique<HaemolacriaItem>();
    return nullptr;
}
