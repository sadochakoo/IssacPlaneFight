#include "tear_profile.h"

TearTextureId resolve_player_tear_texture(const Player& player, const Bullet& tear) {
    if (tear.is_haemolacria_orb) {
        return TearTextureId::HaemolacriaOrb;
    }
    if (tear.is_haemolacria_shard) {
        return TearTextureId::HaemolacriaShard;
    }
    if (tear.has_parasite) {
        return TearTextureId::Parasite;
    }
    // 道具驱动测试：魔术弯勺（追踪）≥1 层 → 切换贴图
    if (player.stats.tracking_level >= 1) {
        return TearTextureId::Tracking;
    }
    return TearTextureId::Basic;
}

const char* tear_texture_asset_path(TearTextureId id) {
    switch (id) {
    case TearTextureId::Tracking:
        return "gfx/tears/toxic.png";
    case TearTextureId::Parasite:
        return "gfx/tears/bob_rage.png";
    case TearTextureId::HaemolacriaOrb:
        return "gfx/tears/holy_water.png";
    case TearTextureId::HaemolacriaShard:
        return "gfx/tears/brimstone.png";
    case TearTextureId::Basic:
    default:
        return "gfx/tears/basic.png";
    }
}

void apply_tear_visual_from_player(Bullet& tear, const Player& player) {
    tear.texture_id = static_cast<int>(resolve_player_tear_texture(player, tear));
}
