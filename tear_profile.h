/*
 * tear_profile.h - 泪弹贴图 ID 与道具驱动解析（可扩展）
 *
 * 测试规则（当前写死）：
 * - 默认：basic.png
 * - 持有魔术弯勺（tracking_level >= 1）：toxic.png
 * - 寄生虫 / 泪血症子类：独立贴图
 */

#ifndef TEAR_PROFILE_H
#define TEAR_PROFILE_H

#include "player_stats.h"

enum class TearTextureId {
    Basic = 0,
    Tracking,        // 魔术弯勺 → 测试切换
    Parasite,
    HaemolacriaOrb,
    HaemolacriaShard,
    Count
};

/** 根据玩家道具与泪弹状态决定贴图 */
TearTextureId resolve_player_tear_texture(const Player& player, const Bullet& tear);

/** 资源相对路径（ASCII，避免中文路径加载失败） */
const char* tear_texture_asset_path(TearTextureId id);

/** 写入 tear.texture_id，供渲染层使用 */
void apply_tear_visual_from_player(Bullet& tear, const Player& player);

#endif
