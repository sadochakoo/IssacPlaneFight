/*
 * item_test_loader.h - 从 test_items.json / test_combat_status.json 加载调试用例
 */

#ifndef ITEM_TEST_LOADER_H
#define ITEM_TEST_LOADER_H

#include "player_stats.h"
#include <string>

/** 按 case_id 搜索 test_items.json，再搜 test_combat_status.json */
bool load_test_case(Player& player, const std::string& target_case_id);

const char* test_items_config_path();
const char* test_combat_status_config_path();

#endif
