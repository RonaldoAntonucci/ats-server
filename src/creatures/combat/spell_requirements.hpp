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
	#include <string_view>
	#include <vector>
#endif

class Player;

struct DisciplineRequirement {
	uint16_t disciplineId = 0;
	uint32_t minimumRank = 0;

	bool operator==(const DisciplineRequirement &) const = default;
};

enum class SpellRequirementDefinitionResult : uint8_t {
	Added,
	Duplicate,
	InvalidDisciplineId,
	InvalidMinimumRank,
	UnknownDiscipline,
	ConflictingDuplicate,
};

enum class SpellRequirementResult : uint8_t {
	Allowed,
	DisciplineRequirementNotMet,
};

class SpellRequirementSet {
public:
	[[nodiscard]] SpellRequirementDefinitionResult addDiscipline(uint16_t disciplineId, uint32_t minimumRank);
	[[nodiscard]] SpellRequirementResult evaluate(const Player &player) const;
	[[nodiscard]] bool empty() const;
	[[nodiscard]] const std::vector<DisciplineRequirement> &disciplines() const;

private:
	std::vector<DisciplineRequirement> disciplineRequirements;
};

[[nodiscard]] constexpr std::string_view spellRequirementReason(SpellRequirementResult result) {
	switch (result) {
		case SpellRequirementResult::Allowed:
			return "allowed";
		case SpellRequirementResult::DisciplineRequirementNotMet:
			return "discipline_requirement_not_met";
	}

	return "discipline_requirement_not_met";
}
