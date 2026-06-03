/*
 * weapon_synergy_resolver.h - 融合判定（策略表，避免 if-else 地狱）
 */

#ifndef WEAPON_SYNERGY_RESOLVER_H
#define WEAPON_SYNERGY_RESOLVER_H

#include "weapon_profile.h"
#include <string>
#include <vector>
#include <functional>

class WeaponSynergyResolver {
public:
    WeaponProfile resolve(const std::vector<std::string>& collected) const;

private:
    using Rule = std::function<bool(const std::vector<std::string>&)>;
    struct SynergyRule {
        Rule         match;
        WeaponProfile profile;
    };

    static bool has(const std::vector<std::string>& items, const char* id);
    static std::vector<SynergyRule> buildRuleTable();
};

#endif
