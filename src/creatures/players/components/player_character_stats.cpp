/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/components/player_character_stats.hpp"

#include "config/configmanager.hpp"
#include "creatures/players/player.hpp"
#include "lib/logging/logger.hpp"

PlayerCharacterStats::PlayerCharacterStats(Player &player) :
	player(player) { }

AttributeTotals PlayerCharacterStats::attributes() const {
	const auto contribution = player.disciplines().snapshot(player.getLevel());
	AttributeTotals totals {};
	for (size_t index = 0; index < totals.size(); ++index) {
		totals[index] = contribution.attributes[index];
	}
	return totals;
}

DerivedStatTotals PlayerCharacterStats::stats() const {
	const auto currentAttributes = attributes();
	const auto multipliers = g_configManager().getDerivedStatMultipliers();
	const auto calculation = calculateDerivedStats(currentAttributes, *multipliers);
	for (size_t index = 0; index < calculation.saturated.size(); ++index) {
		if (calculation.saturated[index]) {
			g_logger().error("[CharacterDerivedStats] player={} status={} reason=derived status overflow", player.getName(), derivedStatId(static_cast<DerivedStat>(index)));
		}
	}
	return calculation.totals;
}

uint64_t PlayerCharacterStats::attribute(CharacterAttribute attribute) const {
	return attributes().at(static_cast<size_t>(attribute));
}

uint64_t PlayerCharacterStats::stat(DerivedStat stat) const {
	return stats().at(static_cast<size_t>(stat));
}
