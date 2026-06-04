#include "character_roster.h"
#include "isaac_character.h"

#include <algorithm>

CharacterRoster& CharacterRoster::instance() {
    static CharacterRoster roster;
    return roster;
}

void CharacterRoster::initialize() {
    if (!characters_.empty()) {
        return;
    }
    characters_.push_back(std::make_unique<IsaacCharacter>());
    selected_index_ = 0;
}

int CharacterRoster::count() const {
    return static_cast<int>(characters_.size());
}

int CharacterRoster::selected_index() const {
    return selected_index_;
}

CharacterId CharacterRoster::selected_id() const {
    if (characters_.empty()) {
        return CharacterId::Isaac;
    }
    return characters_[static_cast<size_t>(selected_index_)]->id();
}

GameCharacter& CharacterRoster::selected() {
    return *characters_[static_cast<size_t>(selected_index_)];
}

const GameCharacter& CharacterRoster::selected() const {
    return *characters_[static_cast<size_t>(selected_index_)];
}

const GameCharacter& CharacterRoster::at(int index) const {
    return *characters_[static_cast<size_t>(index)];
}

void CharacterRoster::set_selected_index(int index) {
    if (characters_.empty()) {
        return;
    }
    const int n = count();
    selected_index_ = ((index % n) + n) % n;
}

bool CharacterRoster::cycle(int delta) {
    if (characters_.empty() || delta == 0) {
        return false;
    }
    const int prev = selected_index_;
    set_selected_index(selected_index_ + delta);
    return selected_index_ != prev;
}
