#include "item_test_loader.h"
#include "baby_system.h"
#include "brimstone_laser.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

const char* test_items_config_path() {
    return "test_items.json";
}

bool load_test_case(Player& player, const std::string& target_case_id) {
    std::ifstream config_file(test_items_config_path());
    if (!config_file.is_open()) {
        std::cerr << "未找到 test_items.json 配置文件！\n";
        return false;
    }

    json json_data;
    try {
        config_file >> json_data;
    } catch (const json::parse_error& ex) {
        std::cerr << "test_items.json 解析失败: " << ex.what() << "\n";
        return false;
    }

    if (!json_data.contains("test_cases") || !json_data["test_cases"].is_array()) {
        std::cerr << "test_items.json 缺少 test_cases 数组\n";
        return false;
    }

    for (const auto& test_case : json_data["test_cases"]) {
        if (!test_case.contains("case_id")) continue;

        const std::string case_id = test_case["case_id"].get<std::string>();
        if (case_id != target_case_id) continue;

        if (test_case.contains("name")) {
            std::cout << "正在加载测试用例: "
                      << test_case["name"].get<std::string>() << "\n";
        }
        if (test_case.contains("description")) {
            std::cout << "  " << test_case["description"].get<std::string>() << "\n";
        }

        const auto& mods = test_case["stats_modifier"];
        player.stats.brimstone_level = mods.value("brimstone_level", 0);
        player.stats.tracking_level  = mods.value("tracking_level", 0);
        player.stats.extra_bullets   = mods.value("extra_bullets", 0);
        player.stats.baby_count      = mods.value("baby_count", 0);
        if (mods.contains("has_parasite")) {
            player.stats.has_parasite = mods["has_parasite"].get<bool>();
        }

        BrimstoneLaser::reset(player);
        BabySystem::syncCount(player);

        std::cout << "  brimstone=" << player.stats.brimstone_level
                  << " tracking=" << player.stats.tracking_level
                  << " extra_bullets=" << player.stats.extra_bullets
                  << " babies=" << player.stats.baby_count << "\n";
        return true;
    }

    std::cerr << "未找到对应的 case_id: " << target_case_id << "\n";
    return false;
}
