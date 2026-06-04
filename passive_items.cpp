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

// ---------- 八寸钉 (Spike Nail) ----------
class SpikeNailItem : public Item {
public:
    std::string getId() const override { return ItemIds::SPIKE_NAIL; }

    ItemDisplay getDisplay() const override {
        return {
            L"八寸钉",
            L"八向尖刺命中：径向击退+残影+冲击波",
            sf::Color(200, 200, 220)};
    }

    void applyTo(Player& player) override {
        player.stats_ext.has_spike_nail = true;
    }
};

// ---------- 背叛 (Betrayal) ----------
class BetrayalItem : public Item {
public:
    std::string getId() const override { return ItemIds::BETRAYAL; }

    ItemDisplay getDisplay() const override {
        return {
            L"背叛",
            L"命中魅惑敌人并倒戈，转而攻击敌方单位",
            sf::Color(140, 120, 255)};
    }

    void applyTo(Player& player) override {
        player.stats_ext.has_betrayal = true;
    }
};

// ---------- 苹果刀片 (模块三) ----------
class AppleRazorItem : public Item {
public:
    std::string getId() const override { return ItemIds::APPLE_RAZOR; }

    ItemDisplay getDisplay() const override {
        return {
            L"苹果刀片",
            L"银白旋转刀片，命中后八向分裂小刀片",
            sf::Color(220, 230, 245)};
    }

    void applyTo(Player& player) override {
        player.stats_ext.has_apple = true;
    }
};

// ---------- 神性 (模块三) ----------
class GodheadItem : public Item {
public:
    std::string getId() const override { return ItemIds::GODHEAD; }

    ItemDisplay getDisplay() const override {
        return {
            L"神性",
            L"金色追踪泪弹 + 光环持续烫伤",
            sf::Color(255, 220, 80)};
    }

    void applyTo(Player& player) override {
        player.stats_ext.has_godhead = true;
        if (player.stats.tracking_level < 1) {
            ++player.stats.tracking_level;
        }
    }
};

// ---------- 玻璃碎片 (模块三) ----------
class GlassShardTearItem : public Item {
public:
    std::string getId() const override { return ItemIds::GLASS_SHARD; }

    ItemDisplay getDisplay() const override {
        return {
            L"玻璃碎片",
            L"半透明棱镜弹，越近伤害越高",
            sf::Color(120, 240, 255)};
    }

    void applyTo(Player& player) override {
        player.stats_ext.has_glass_shard = true;
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
        case 5: return std::make_unique<SpikeNailItem>();
        case 6: return std::make_unique<BetrayalItem>();
        case 7: return std::make_unique<AppleRazorItem>();
        case 8: return std::make_unique<GodheadItem>();
        case 9: return std::make_unique<GlassShardTearItem>();
        default: return nullptr;
    }
}

std::unique_ptr<Item> ItemFactory::createById(const std::string& id) {
    if (id == ItemIds::SPOON_BENDER)  return std::make_unique<SpoonBenderItem>();
    if (id == ItemIds::BRIMSTONE)     return std::make_unique<BrimstoneItem>();
    if (id == ItemIds::TWENTY_TWENTY) return std::make_unique<TwentyTwentyItem>();
    if (id == ItemIds::PARASITE)      return std::make_unique<ParasiteItem>();
    if (id == ItemIds::HAEMOLACRIA)   return std::make_unique<HaemolacriaItem>();
    if (id == ItemIds::SPIKE_NAIL)    return std::make_unique<SpikeNailItem>();
    if (id == ItemIds::BETRAYAL)      return std::make_unique<BetrayalItem>();
    if (id == ItemIds::APPLE_RAZOR)   return std::make_unique<AppleRazorItem>();
    if (id == ItemIds::GODHEAD)       return std::make_unique<GodheadItem>();
    if (id == ItemIds::GLASS_SHARD)   return std::make_unique<GlassShardTearItem>();
    return nullptr;
}
