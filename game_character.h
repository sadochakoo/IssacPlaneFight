#ifndef GAME_CHARACTER_H
#define GAME_CHARACTER_H

#include <memory>
#include <vector>

/** 可玩角色标识（扩展时在 Count 前追加） */
enum class CharacterId {
    Isaac = 0,
    Count
};

/** 选角界面展示的数值条（竖线数量） */
struct CharacterStatDisplay {
    int health_pips = 0;
    int speed_pips  = 0;
    int damage_pips = 0;
};

/**
 * 角色基类：选角立绘、战斗动画路径、展示名等由子类实现。
 * 后续新角色继承此类并在 CharacterRoster 中注册即可。
 */
class GameCharacter {
public:
    virtual ~GameCharacter() = default;

    virtual CharacterId id() const = 0;
    virtual const wchar_t* display_name() const = 0;
    /** 选角界面中央立绘（如 gfx/ui/以撒.png） */
    virtual const char* select_portrait_path() const = 0;
    /** 战斗中逐帧动画，如 gfx/player/%d.png */
    virtual const char* battle_anim_pattern() const = 0;
    virtual int battle_frame_count() const = 0;
    virtual CharacterStatDisplay stat_display() const = 0;
};

/** 全局角色表：管理可选角色与当前选中项 */
class CharacterRoster {
public:
    static CharacterRoster& instance();

    void initialize();
    int count() const;
    int selected_index() const;
    CharacterId selected_id() const;

    GameCharacter& selected();
    const GameCharacter& selected() const;
    const GameCharacter& at(int index) const;

    void set_selected_index(int index);
    /** @return 选中索引是否变化 */
    bool cycle(int delta);

private:
    CharacterRoster() = default;

    std::vector<std::unique_ptr<GameCharacter>> characters_;
    int selected_index_ = 0;
};

#endif
