/*
 * item_registry.h - 道具池注册表（UI 索引 ↔ 工厂）
 */

#ifndef ITEM_REGISTRY_H
#define ITEM_REGISTRY_H

#include "passive_item.h"

class ItemRegistry {
public:
    static int itemCount();
    static ItemDisplay getDisplay(int index);
};

#endif
