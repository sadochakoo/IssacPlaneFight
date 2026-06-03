/*
 * passive_item.h - 道具多态基类 + 工厂
 */

#ifndef PASSIVE_ITEM_H
#define PASSIVE_ITEM_H

#include "player_stats.h"
#include <memory>
#include <string>

// ==================== 道具 ID ====================
namespace ItemIds {
    inline const char* SPOON_BENDER  = "spoon_bender";
    inline const char* BRIMSTONE     = "brimstone";
    inline const char* TWENTY_TWENTY = "20/20";
}

// ==================== UI 展示元数据 ====================
struct ItemDisplay {
    const wchar_t* name;
    const wchar_t* description;
    sf::Color      color;
};

// ==================== 多态道具基类 ====================
class Item {
public:
    virtual ~Item() = default;

    virtual std::string getId() const = 0;
    virtual ItemDisplay getDisplay() const = 0;

    // 修改 PlayerStats / 玩家状态（被动永久效果）
    virtual void applyTo(Player& player) = 0;
};

// ==================== 工厂 ====================
class ItemFactory {
public:
    static std::unique_ptr<Item> create(int registry_index);
    static std::unique_ptr<Item> createById(const std::string& id);
};

#endif
