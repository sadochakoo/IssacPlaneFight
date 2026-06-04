#include "item_registry.h"

static const ItemRegistryEntry REGISTRY_TABLE[] = {
    { L"魔术弯勺", L"子弹变紫并轻微追踪最近敌机",
      sf::Color(200, 100, 255), "gfx/items/spoon_bender.png" },
    { L"硫磺火", L"蓄力发射贯穿屏幕的红色激光",
      sf::Color(220, 40, 40), "gfx/items/brimstone.png" },
    { L"20/20", L"向上平行发射两发子弹",
      sf::Color(180, 220, 255), "gfx/items/twenty_twenty.png" },
    { L"寄生虫", L"命中后 V 字分裂，伤害逐代减半",
      sf::Color(180, 255, 120), "gfx/items/parasite.png" },
    { L"泪血症", L"慢速血球落地 360° 爆裂",
      sf::Color(140, 0, 30), "gfx/items/haemolacria.png" },
    { L"八寸钉", L"八向尖刺命中：径向击退+残影+冲击波",
      sf::Color(200, 200, 220), "gfx/items/spike_nail.png" },
    { L"背叛", L"命中魅惑敌人并倒戈，转而攻击敌方单位",
      sf::Color(140, 120, 255), "gfx/items/betrayal.png" },
    { L"苹果刀片", L"命中后八向分裂银白小刀片",
      sf::Color(220, 230, 245), "gfx/items/apple_razor.png" },
    { L"神性", L"追踪金弹 + 光环持续伤害",
      sf::Color(255, 220, 80), "gfx/items/godhead.png" },
    { L"玻璃碎片", L"贴脸伤害更高，远距衰减",
      sf::Color(120, 240, 255), "gfx/items/glass_shard.png" },
};

static const int REGISTRY_COUNT =
    static_cast<int>(sizeof(REGISTRY_TABLE) / sizeof(REGISTRY_TABLE[0]));

static const char* k_default_icon = "gfx/items/spoon_bender.png";

int ItemRegistry::itemCount() {
    return REGISTRY_COUNT;
}

ItemDisplay ItemRegistry::getDisplay(int index) {
    if (index < 0 || index >= REGISTRY_COUNT) {
        return { L"?", L"", sf::Color::White };
    }
    const ItemRegistryEntry& e = REGISTRY_TABLE[index];
    return { e.name, e.description, e.color };
}

const ItemRegistryEntry& ItemRegistry::getEntry(int index) {
    if (index < 0 || index >= REGISTRY_COUNT) {
        static const ItemRegistryEntry k_fallback = {
            L"?", L"", sf::Color::White, k_default_icon };
        return k_fallback;
    }
    return REGISTRY_TABLE[index];
}

const char* ItemRegistry::iconPath(int index) {
    if (index < 0 || index >= REGISTRY_COUNT) {
        return k_default_icon;
    }
    return REGISTRY_TABLE[index].icon_path;
}
