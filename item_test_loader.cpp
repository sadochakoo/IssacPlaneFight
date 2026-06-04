#include "item_test_loader.h"
#include "baby_system.h"
#include "brimstone_laser.h"
#include "item_system.h"
#include "module3_tears.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

const char* k_test_items_paths[] = {
    "test_items.json",
    "bin/Debug/test_items.json",
    "bin/Release/test_items.json",
    nullptr
};

const char* k_test_combat_paths[] = {
    "test_combat_status.json",
    "bin/Debug/test_combat_status.json",
    "bin/Release/test_combat_status.json",
    nullptr
};

bool apply_test_case(Player& player, const json& test_case) {
    if (test_case.contains("name")) {
        std::cout << "正在加载测试用例: "
                  << test_case["name"].get<std::string>() << "\n";
    }
    if (test_case.contains("description")) {
        std::cout << "  " << test_case["description"].get<std::string>() << "\n";
    }

    if (test_case.contains("stats_modifier")) {
        const auto& mods = test_case["stats_modifier"];
        player.stats.brimstone_level = mods.value("brimstone_level", 0);
        player.stats.tracking_level  = mods.value("tracking_level", 0);
        player.stats.extra_bullets   = mods.value("extra_bullets", 0);
        player.stats.baby_count      = mods.value("baby_count", 0);
        player.stats.has_parasite    = mods.value("has_parasite", false);
        player.stats.has_haemolacria = mods.value("has_haemolacria", false);
        if (mods.contains("speed")) {
            player.stats.speed = mods["speed"].get<double>();
        }
    }

    player.stats_ext = PlayerStatsExtension{};
    player.stats_ext.base_speed = static_cast<float>(player.stats.speed);

    if (test_case.contains("item_extension")) {
        const auto& ext = test_case["item_extension"];
        player.stats_ext.mirror_clone_level = ext.value("mirror_clone_level", 0);
        player.stats_ext.has_glass_shard    = ext.value("has_glass_shard", false);
        player.stats_ext.has_tiny_planet    = ext.value("has_tiny_planet", false);
        player.stats_ext.has_godhead        = ext.value("has_godhead", false);
        player.stats_ext.has_apple          = ext.value("has_apple", false);
        player.stats_ext.has_spike_nail     = ext.value("has_spike_nail", false);
        player.stats_ext.has_ice_baby       = ext.value("has_ice_baby", false);
        player.stats_ext.has_betrayal       = ext.value("has_betrayal", false);
        if (ext.contains("base_speed")) {
            player.stats_ext.base_speed = ext["base_speed"].get<float>();
        }
        player.stats_ext.current_speed_multiplier =
            ext.value("current_speed_multiplier", 1.f);
        if (player.stats_ext.current_speed_multiplier < 0.999f) {
            player.stats.speed =
                player.stats_ext.base_speed
                * player.stats_ext.current_speed_multiplier;
        }
    }

    if (test_case.contains("manual_checks") && test_case["manual_checks"].is_array()) {
        std::cout << "  [manual_checks]\n";
        for (const auto& line : test_case["manual_checks"]) {
            std::cout << "    - " << line.get<std::string>() << "\n";
        }
    }

    if (test_case.contains("pickup_toast_item_id")) {
        const std::string toast_id =
            test_case["pickup_toast_item_id"].get<std::string>();
        const wchar_t* custom =
            module3::pickup_message_for_extension(toast_id.c_str());
        item_ui::push_pickup_toast(
            item_pickup_toast_queue(),
            toast_id.c_str(),
            custom);
    }

    BrimstoneLaser::reset(player);
    BabySystem::syncCount(player);
    player.clampStats();

    std::cout << "  brimstone=" << player.stats.brimstone_level
              << " tracking=" << player.stats.tracking_level
              << " speed=" << player.stats.speed
              << " babies=" << player.stats.baby_count << "\n";
    std::cout << "  ext: mirror=" << player.stats_ext.mirror_clone_level
              << " glass=" << player.stats_ext.has_glass_shard
              << " planet=" << player.stats_ext.has_tiny_planet
              << " godhead=" << player.stats_ext.has_godhead
              << " apple=" << player.stats_ext.has_apple
              << " spike=" << player.stats_ext.has_spike_nail
              << " ice_baby=" << player.stats_ext.has_ice_baby
              << " betrayal=" << player.stats_ext.has_betrayal
              << " base_speed=" << player.stats_ext.base_speed << "\n";
    return true;
}

bool find_case_in_file(
    Player& player,
    const char* config_path,
    const std::string& target_case_id)
{
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        return false;
    }

    json json_data;
    try {
        config_file >> json_data;
    } catch (const json::parse_error& ex) {
        std::cerr << config_path << " 解析失败: " << ex.what() << "\n";
        return false;
    }

    if (!json_data.contains("test_cases") || !json_data["test_cases"].is_array()) {
        std::cerr << config_path << " 缺少 test_cases 数组\n";
        return false;
    }

    for (const auto& test_case : json_data["test_cases"]) {
        if (!test_case.contains("case_id")) {
            continue;
        }
        if (test_case["case_id"].get<std::string>() == target_case_id) {
            std::cout << "从 " << config_path << " 加载\n";
            return apply_test_case(player, test_case);
        }
    }

    return false;
}

bool try_paths(
    Player& player,
    const char* const* paths,
    const std::string& target_case_id)
{
    for (int i = 0; paths[i] != nullptr; ++i) {
        if (find_case_in_file(player, paths[i], target_case_id)) {
            return true;
        }
    }
    return false;
}

} // namespace

const char* test_items_config_path() {
    return k_test_items_paths[0];
}

const char* test_combat_status_config_path() {
    return k_test_combat_paths[0];
}

bool load_test_case(Player& player, const std::string& target_case_id) {
    if (try_paths(player, k_test_items_paths, target_case_id)) {
        return true;
    }
    if (try_paths(player, k_test_combat_paths, target_case_id)) {
        return true;
    }
    std::cerr << "未找到用例: " << target_case_id << "\n";
    return false;
}
