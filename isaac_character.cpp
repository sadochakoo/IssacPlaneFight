#include "isaac_character.h"

CharacterId IsaacCharacter::id() const {
    return CharacterId::Isaac;
}

const wchar_t* IsaacCharacter::display_name() const {
    return L"以撒";
}

const char* IsaacCharacter::select_portrait_path() const {
    return "gfx/ui/isaac_portrait.png";
}

const char* IsaacCharacter::battle_anim_pattern() const {
    return "gfx/player/%d.png";
}

int IsaacCharacter::battle_frame_count() const {
    return 6;
}

CharacterStatDisplay IsaacCharacter::stat_display() const {
    return {3, 2, 2};
}
