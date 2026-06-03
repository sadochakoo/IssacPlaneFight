/*
 * item_test_loader.h - 从 test_items.json 加载调试用例（nlohmann/json）
 */

#ifndef ITEM_TEST_LOADER_H
#define ITEM_TEST_LOADER_H

#include "player_stats.h"
#include <string>

// 按 case_id 覆盖 player.stats 层数，并同步宝宝/激光状态
bool load_test_case(Player& player, const std::string& target_case_id);

// 配置文件路径（与 exe 同目录的 test_items.json）
const char* test_items_config_path();

#endif
