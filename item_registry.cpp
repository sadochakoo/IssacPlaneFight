#include "item_registry.h"


//static const ItemDisplay DISPLAY_TABLE[] = 0; //清空道具池

static const ItemDisplay DISPLAY_TABLE[] = {
    { L"魔术弯勺", L"子弹变紫并轻微追踪最近敌机", sf::Color(200, 100, 255) },
    { L"硫磺火",   L"蓄力发射贯穿屏幕的红色激光", sf::Color(220, 40, 40) },
    { L"20/20",    L"向上平行发射两发子弹",       sf::Color(180, 220, 255) },
    { L"寄生虫",   L"命中后 V 字分裂，伤害逐代减半", sf::Color(180, 255, 120) },
    { L"泪血症",   L"慢速血球落地 360° 爆裂",       sf::Color(140, 0, 30) },
    { L"八寸钉",   L"八向尖刺命中：径向击退+残影+冲击波", sf::Color(200, 200, 220) },
    { L"背叛",     L"命中魅惑敌人并倒戈，转而攻击敌方单位", sf::Color(140, 120, 255) },
    { L"苹果刀片", L"命中后八向分裂银白小刀片", sf::Color(220, 230, 245) },
    { L"神性",     L"追踪金弹 + 光环持续伤害", sf::Color(255, 220, 80) },
    { L"玻璃碎片", L"贴脸伤害更高，远距衰减", sf::Color(120, 240, 255) },
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
