/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once

#include "creatures/players/stats/character_stats.hpp"

class Player;

class PlayerCharacterStats {
public:
	explicit PlayerCharacterStats(Player &player);

	[[nodiscard]] AttributeTotals attributes() const;
	[[nodiscard]] DerivedStatTotals stats() const;
	[[nodiscard]] uint64_t attribute(CharacterAttribute attribute) const;
	[[nodiscard]] uint64_t stat(DerivedStat stat) const;

private:
	Player &player;
};
