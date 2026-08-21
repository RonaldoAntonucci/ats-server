/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once

#ifndef USE_PRECOMPILED_HEADERS
	#include <cstdint>
	#include <map>
	#include <mutex>
	#include <string>
	#include <vector>
#endif

#include "creatures/players/disciplines/discipline.hpp"

enum class DisciplineMutationResult : uint8_t {
	Success,
	UnknownDiscipline,
	NotOwned,
	RankLimit,
	InvalidId,
};

struct DisciplineMutation {
	DisciplineMutationResult result;
	uint32_t before = 0;
	uint32_t after = 0;

	[[nodiscard]] bool success() const {
		return result == DisciplineMutationResult::Success;
	}
};

struct DisciplineSnapshotEntry {
	uint16_t id = 0;
	std::string name;
	uint32_t rank = 0;
	AttributeContributions perLevel {};
};

struct DisciplineContributionSnapshot {
	AttributeTotals attributes {};
	std::vector<DisciplineSnapshotEntry> disciplines;
};

class Player;

class PlayerDisciplines {
public:
	explicit PlayerDisciplines(Player &player);

	[[nodiscard]] const std::map<uint16_t, uint32_t> &ranks() const;
	[[nodiscard]] DisciplineContributionSnapshot snapshot(uint32_t level) const;
	[[nodiscard]] DisciplineMutation addRank(uint16_t id);
	[[nodiscard]] DisciplineMutation removeRank(uint16_t id);

private:
	void loadLocked() const;
	void persistLocked(const std::map<uint16_t, uint32_t> &nextRanks) const;

	Player &player;
	mutable std::mutex mutex;
	mutable bool loaded = false;
	mutable std::map<uint16_t, uint32_t> storedRanks;
};
