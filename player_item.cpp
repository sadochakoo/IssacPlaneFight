#include "passive_item.h"

void Player::applyItem(Item* item) {
    if (!item) return;

    collected_items.push_back(item->getId());
    item->applyTo(*this);
    clampStats();
    base_stats = stats;
    ++item_count;

    delete item;
}
