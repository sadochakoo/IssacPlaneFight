/*
 * item_registry.h - 道具池注册表（UI 索引 ↔ 展示元数据 ↔ 贴图路径）
 */

#ifndef ITEM_REGISTRY_H
#define ITEM_REGISTRY_H

#include "passive_item.h"

/** 注册表一行：显示名（中文）+ 纯英文贴图路径 */
struct ItemRegistryEntry {
    const wchar_t* name;
    const wchar_t* description;
    sf::Color      color;
    const char*    icon_path; /**< 仅 ASCII，如 gfx/items/brimstone.png */
};

class ItemRegistry {
public:
    static int itemCount();
    static ItemDisplay getDisplay(int index);
    static const ItemRegistryEntry& getEntry(int index);
    /** 道具图标路径；越界时返回默认糖心图标 */
    static const char* iconPath(int index);
};

#endif
