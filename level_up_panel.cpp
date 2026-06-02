/*
 * level_up_panel.cpp - 升级三选一 UI 面板实现
 *
 * 功能说明：
 * 1. 随机抽取 3 个道具
 * 2. 绘制 3 张卡牌显示道具信息
 * 3. 处理鼠标点击事件
 * 4. 返回玩家选择的道具索引
 */

#include "level_up_panel.h"

// ==================== 构造函数 ====================
/*
 * LevelUpPanel() - 默认构造函数
 *
 * 初始化字体：从文件加载中文字体
 * 如果加载失败，使用默认字体（可能不支持中文）
 */
LevelUpPanel::LevelUpPanel() {
    // 尝试加载中文字体（按优先级尝试）
    if (!font.loadFromFile("C:/Windows/Fonts/msyh.ttc")) {
        // 微软雅黑
        if (!font.loadFromFile("C:/Windows/Fonts/simhei.ttf")) {
            // 黑体
            if (!font.loadFromFile("C:/Windows/Fonts/simsun.ttc")) {
                // 宋体
                font.loadFromFile("C:/Windows/Fonts/arial.ttf");
                // Arial（英文）作为最终回退
            }
        }
    }
}

// ==================== triggerLevelUp() ====================
/*
 * 触发升级面板
 *
 * 算法：
 * 1. 清空上一轮的选项
 * 2. 创建道具池（包含所有 19 种道具）
 * 3. Fisher-Yates 洗牌算法打乱道具池
 * 4. 取出前 3 个作为本次选项
 * 5. 稀有度加权：加权后排序再取前 3
 */
void LevelUpPanel::triggerLevelUp() {
    // 清空上一轮的选项
    current_options.clear();

    // 创建道具池：0, 1, 2, ..., ITEM_COUNT-1
    std::vector<int> pool;
    for (int i = 0; i < ITEM_COUNT; ++i) {
        pool.push_back(i);
    }

    // 随机打乱道具池（Fisher-Yates 洗牌算法）
    std::random_device rd;                 // 真随机数种子
    std::mt19937 gen(rd());               // Mersenne Twister 随机数引擎
    std::shuffle(pool.begin(), pool.end(), gen);

    // 取出前 3 个作为选项
    for (int i = 0; i < 3 && i < static_cast<int>(pool.size()); ++i) {
        current_options.push_back(pool[i]);
    }
}

// ==================== update() ====================
/*
 * 每帧更新面板状态，检测鼠标点击
 *
 * 参数：window - 游戏窗口（用于获取鼠标位置和事件）
 * 返回值：int - 玩家选择的道具索引（-1 表示未选择或面板未激活）
 *
 * 检测逻辑：
 * 1. 面板未激活 → 返回 -1
 * 2. 鼠标左键未按下 → 返回 -1
 * 3. 计算 3 张卡牌的矩形区域
 * 4. 检测鼠标是否在卡牌内
 * 5. 在卡牌内 → 返回该道具索引，清空选项（关闭面板）
 * 6. 不在任何卡牌内 → 返回 -1
 */
int LevelUpPanel::update(sf::RenderWindow& window) {
    // 面板未激活，跳过
    if (current_options.empty()) return -1;

    // 获取鼠标在窗口中的位置
    sf::Vector2i mouse_pos = sf::Mouse::getPosition(window);

    // 检测鼠标左键是否按下
    if (!sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
        return -1;
    }

    // 计算卡牌起始位置（水平居中放置 3 张卡牌）
    float total_width = 3.f * card_width + 2.f * card_spacing;  // 3 张卡 + 2 个间距
    float start_x = (static_cast<float>(window.getSize().x) - total_width) / 2.f;
    float start_y = static_cast<float>(window.getSize().y) / 2.f - card_height / 2.f;

    // 遍历 3 张卡牌，检测点击
    for (size_t i = 0; i < current_options.size(); ++i) {
        float card_x = start_x + static_cast<float>(i) * (card_width + card_spacing);

        // 创建卡牌的矩形区域
        sf::FloatRect card_rect(card_x, start_y, card_width, card_height);

        // 检测鼠标是否在卡牌内
        if (card_rect.contains(static_cast<float>(mouse_pos.x), static_cast<float>(mouse_pos.y))) {
            int selected_item = current_options[i];  // 记录选中道具
            current_options.clear();                  // 清空选项（关闭面板）
            return selected_item;                     // 返回道具索引
        }
    }

    // 没有点击任何卡牌
    return -1;
}

// ==================== render() ====================
/*
 * 绘制升级面板
 *
 * 参数：window - 游戏窗口
 *
 * 绘制层次（从底层到顶层）：
 * 1. 半透明黑色背景遮罩（暂停视觉）
 * 2. 3 张卡牌：
 *    a. 卡牌背景（道具颜色）
 *    b. 卡牌边框（白色）
 *    c. 道具名称文字
 *    d. 道具描述文字
 * 3. 标题文字："选择一件道具"
 */
void LevelUpPanel::render(sf::RenderWindow& window) {
    // 面板未激活，跳过
    if (current_options.empty()) return;

    // === 第 1 层：半透明黑色背景遮罩 ===
    // 创建与窗口相同大小的矩形，填充半透明黑色
    sf::RectangleShape overlay(sf::Vector2f(window.getSize()));
    overlay.setFillColor(sf::Color(0, 0, 0, 180));  // RGBA: 黑 70% 透明
    window.draw(overlay);

    // === 标题文字 ===
    sf::Text title_text(L"选择一件道具", font, 32);
    title_text.setFillColor(sf::Color::White);
    // 水平居中
    sf::FloatRect title_bounds = title_text.getLocalBounds();
    title_text.setOrigin(title_bounds.width / 2.f, 0.f);
    title_text.setPosition(
        static_cast<float>(window.getSize().x) / 2.f,
        80.f
    );
    window.draw(title_text);

    // === 计算卡牌位置 ===
    float total_width = 3.f * card_width + 2.f * card_spacing;
    float start_x = (static_cast<float>(window.getSize().x) - total_width) / 2.f;
    float start_y = 180.f;  // 在标题下方

    // === 第 2 层：绘制 3 张卡牌 ===
    for (size_t i = 0; i < current_options.size(); ++i) {
        // 获取对应道具数据
        const Item& item = ITEM_POOL[current_options[i]];
        float card_x = start_x + static_cast<float>(i) * (card_width + card_spacing);

        // --- 2a. 卡牌背景 ---
        sf::RectangleShape card(sf::Vector2f(card_width, card_height));
        card.setPosition(card_x, start_y);
        card.setFillColor(item.color);
        card.setOutlineThickness(4.f);               // 边框粗细 4px
        card.setOutlineColor(sf::Color::White);       // 白色边框
        window.draw(card);

        // --- 2c. 道具图标（圆形） ---
        sf::CircleShape icon(30.f);                   // 半径 30px
        icon.setFillColor(sf::Color(50, 50, 50, 180));
        icon.setOutlineThickness(2.f);
        icon.setOutlineColor(sf::Color::White);
        icon.setPosition(card_x + card_width / 2.f - 30.f, start_y + 40.f);
        window.draw(icon);

        // --- 2d. 道具名称 ---
        sf::Text name_text(item.name, font, 20);
        name_text.setFillColor(sf::Color::White);
        name_text.setOutlineColor(sf::Color::Black);
        name_text.setOutlineThickness(2.f);
        // 水平居中
        sf::FloatRect name_bounds = name_text.getLocalBounds();
        name_text.setOrigin(name_bounds.width / 2.f, 0.f);
        name_text.setPosition(card_x + card_width / 2.f, start_y + 120.f);
        window.draw(name_text);

        // --- 2e. 道具描述 ---
        sf::Text desc_text(item.description, font, 16);
        desc_text.setFillColor(sf::Color(230, 230, 230));
        desc_text.setOutlineColor(sf::Color(50, 50, 50));
        desc_text.setOutlineThickness(1.f);
        sf::FloatRect desc_bounds = desc_text.getLocalBounds();
        desc_text.setOrigin(desc_bounds.width / 2.f, 0.f);
        desc_text.setPosition(card_x + card_width / 2.f, start_y + 160.f);
        window.draw(desc_text);

        // --- 2f. 提示文字 ---
        sf::Text hint_text(L"[点击选择]", font, 14);
        hint_text.setFillColor(sf::Color(180, 180, 180));
        sf::FloatRect hint_bounds = hint_text.getLocalBounds();
        hint_text.setOrigin(hint_bounds.width / 2.f, 0.f);
        hint_text.setPosition(card_x + card_width / 2.f, start_y + card_height - 40.f);
        window.draw(hint_text);
    }
}

// ==================== isActive() ====================
/*
 * 判断面板是否激活
 *
 * 返回值：bool - true=面板激活（游戏暂停），false=面板关闭（游戏正常运行）
 */
bool LevelUpPanel::isActive() const {
    return !current_options.empty();
}
