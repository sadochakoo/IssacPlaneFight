/*
 * level_up_panel.h - 升级三选一 UI 面板头文件
 *
 * 功能说明：
 * 1. 玩家升级时暂停游戏
 * 2. 随机抽取 3 个道具显示在屏幕上
 * 3. 玩家点击选择其中一个道具
 * 4. 应用道具效果到玩家属性
 *
 * 仿照以撒的结合"升级选道具"机制
 */

#ifndef LEVEL_UP_PANEL_H
#define LEVEL_UP_PANEL_H

// ==================== 头文件包含 ====================
#include <SFML/Graphics.hpp>   // SFML 图形库
#include <vector>               // STL 容器：vector
#include <string>               // STL 字符串
#include <algorithm>            // STL 算法：shuffle
#include <random>               // C++11 随机数库

// ==================== 自定义头文件 ====================
#include "item_registry.h"

// ==================== LevelUpPanel 类定义 ====================
/*
 * LevelUpPanel - 升级选择面板类
 *
 * 功能：
 * 1. 随机抽取 3 个道具显示在卡牌上
 * 2. 处理鼠标点击选择
 * 3. 返回玩家选择的道具索引
 *
 * 使用方式（在游戏主循环中）：
 *   if (panel.isActive()) {
 *       int selected = panel.update(window);
 *       if (selected != -1) {
 *           player.applyItem(ItemFactory::create(selected).release());
 *       }
 *       panel.render(window);
 *   }
 */
class LevelUpPanel {
public:
    // 构造函数：初始化字体
    LevelUpPanel();

    // 触发升级面板（随机抽取 3 个道具）
    void triggerLevelUp();

    // 每帧更新：检测鼠标点击，返回选中的道具索引（-1 表示未选择）
    int update(sf::RenderWindow& window);

    // 绘制面板：半透明背景 + 3 张道具卡牌
    void render(sf::RenderWindow& window);

    // 判断面板是否激活
    bool isActive() const;

private:
    std::vector<int> current_options;   // 当前 3 个选项（ITEM_POOL 索引）
    sf::Font font;                       // 字体对象（加载中文字体）

    // UI 布局常量
    const float card_width  = 200.f;    // 卡牌宽度（像素）
    const float card_height = 280.f;    // 卡牌高度（像素）
    const float card_spacing = 60.f;    // 卡牌间距（像素）
};

#endif // LEVEL_UP_PANEL_H
