/*
 * passive_items.cpp - 以撒经典被动道具实现
 */

#include "passive_item.h"

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

// ---------- 工厂 ----------
std::unique_ptr<Item> ItemFactory::create(int registry_index) {
    switch (registry_index) {
        case 0: return std::make_unique<SpoonBenderItem>();
        case 1: return std::make_unique<BrimstoneItem>();
        case 2: return std::make_unique<TwentyTwentyItem>();
        default: return nullptr;
    }
}

std::unique_ptr<Item> ItemFactory::createById(const std::string& id) {
    if (id == ItemIds::SPOON_BENDER)  return std::make_unique<SpoonBenderItem>();
    if (id == ItemIds::BRIMSTONE)     return std::make_unique<BrimstoneItem>();
    if (id == ItemIds::TWENTY_TWENTY) return std::make_unique<TwentyTwentyItem>();
    return nullptr;
}
