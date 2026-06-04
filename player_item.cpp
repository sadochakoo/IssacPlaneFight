#include "passive_item.h"

#include "item_system.h"

#include "module3_tears.h"



#include <vector>



namespace {

std::vector<item_ui::pickup_toast> g_item_pickup_toasts;

}



std::vector<item_ui::pickup_toast>& item_pickup_toast_queue() {

    return g_item_pickup_toasts;

}



void Player::applyItem(Item* item) {

    if (!item) {

        return;

    }



    const std::string item_id = item->getId();

    collected_items.push_back(item_id);

    item->applyTo(*this);

    clampStats();

    base_stats = stats;

    ++item_count;



    if (const wchar_t* module3_msg =

            module3::pickup_message_for_extension(item_id.c_str())) {

        item_ui::push_pickup_toast(g_item_pickup_toasts, item_id.c_str(), module3_msg);

    } else {

        item_ui::push_pickup_toast(g_item_pickup_toasts, item_id.c_str(), nullptr);

    }



    delete item;

}

