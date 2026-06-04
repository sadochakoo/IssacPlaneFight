#ifndef ISAAC_CHARACTER_H
#define ISAAC_CHARACTER_H

#include "game_character.h"

/** 以撒：当前唯一完整实现的可玩角色 */
class IsaacCharacter : public GameCharacter {
public:
    CharacterId id() const override;
    const wchar_t* display_name() const override;
    const char* select_portrait_path() const override;
    const char* battle_anim_pattern() const override;
    int battle_frame_count() const override;
    CharacterStatDisplay stat_display() const override;
};

#endif
