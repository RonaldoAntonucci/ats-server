/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/spell_requirements.hpp"

#include "creatures/players/components/player_disciplines.hpp"
#include "creatures/players/disciplines/discipline.hpp"
#include "creatures/players/player.hpp"

SpellRequirementDefinitionResult SpellRequirementSet::addDiscipline(uint16_t disciplineId, uint32_t minimumRank) {
	if (disciplineId == 0) {
		return SpellRequirementDefinitionResult::InvalidDisciplineId;
	}
	if (minimumRank == 0) {
		return SpellRequirementDefinitionResult::InvalidMinimumRank;
	}
	if (!g_disciplines().get(disciplineId)) {
		return SpellRequirementDefinitionResult::UnknownDiscipline;
	}

	const auto existing = std::ranges::find(disciplineRequirements, disciplineId, &DisciplineRequirement::disciplineId);
	if (existing != disciplineRequirements.end()) {
		return existing->minimumRank == minimumRank ? SpellRequirementDefinitionResult::Duplicate : SpellRequirementDefinitionResult::ConflictingDuplicate;
	}

	disciplineRequirements.emplace_back(disciplineId, minimumRank);
	return SpellRequirementDefinitionResult::Added;
}

SpellRequirementResult SpellRequirementSet::evaluate(const Player &player) const {
	const auto &ranks = player.disciplines().ranks();
	for (const auto &requirement : disciplineRequirements) {
		const auto rank = ranks.find(requirement.disciplineId);
		if (rank == ranks.end() || rank->second < requirement.minimumRank) {
			return SpellRequirementResult::DisciplineRequirementNotMet;
		}
	}

	return SpellRequirementResult::Allowed;
}

bool SpellRequirementSet::empty() const {
	return disciplineRequirements.empty();
}

const std::vector<DisciplineRequirement> &SpellRequirementSet::disciplines() const {
	return disciplineRequirements;
}
