#include "item_registry.h"


//static const ItemDisplay DISPLAY_TABLE[] = 0; //清空道具池

static const ItemDisplay DISPLAY_TABLE[] = {
    { L"魔术弯勺", L"子弹变紫并轻微追踪最近敌机", sf::Color(200, 100, 255) },
    { L"硫磺火",   L"蓄力发射贯穿屏幕的红色激光", sf::Color(220, 40, 40) },
    { L"20/20",    L"向上平行发射两发子弹",       sf::Color(180, 220, 255) },
    { L"寄生虫",   L"命中后 V 字分裂，伤害逐代减半", sf::Color(180, 255, 120) },
    { L"泪血症",   L"慢速血球落地 360° 爆裂",       sf::Color(140, 0, 30) },
};

static const int DISPLAY_COUNT = static_cast<int>(sizeof(DISPLAY_TABLE) / sizeof(DISPLAY_TABLE[0]));

int ItemRegistry::itemCount() {
    return DISPLAY_COUNT;
}

ItemDisplay ItemRegistry::getDisplay(int index) {
    if (index < 0 || index >= DISPLAY_COUNT) {
        return { L"?", L"", sf::Color::White };
    }
    return DISPLAY_TABLE[index];
}
