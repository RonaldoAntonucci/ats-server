/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/stats/character_stats.hpp"

#include <cmath>

namespace {
	constexpr std::array<std::string_view, static_cast<size_t>(CharacterAttribute::Last)> attributeIds = {
		"pot",
		"tec",
		"vig",
		"sin",
		"esp",
	};

	constexpr std::array<std::string_view, static_cast<size_t>(DerivedStat::Last)> statIds = {
		"physicalAttack",
		"magicalAttack",
		"precision",
		"physicalDefense",
		"magicalDefense",
		"maximumHealth",
		"maximumMana",
	};

	[[nodiscard]] constexpr size_t index(CharacterAttribute attribute) {
		return static_cast<size_t>(attribute);
	}

	[[nodiscard]] constexpr size_t index(DerivedStat stat) {
		return static_cast<size_t>(stat);
	}

	void assign(DerivedStatCalculation &result, DerivedStat stat, long double value) {
		const auto statIndex = index(stat);
		if (value > static_cast<long double>(maxPublicDerivedStat)) {
			result.totals[statIndex] = maxPublicDerivedStat;
			result.saturated[statIndex] = true;
			return;
		}

		result.totals[statIndex] = static_cast<uint64_t>(std::ceil(value));
	}
} // namespace

std::string_view characterAttributeId(CharacterAttribute attribute) {
	return attributeIds.at(index(attribute));
}

std::string_view derivedStatId(DerivedStat stat) {
	return statIds.at(index(stat));
}

DerivedStatCalculation calculateDerivedStats(const AttributeTotals &attributes, const DerivedStatMultipliers &multipliers) {
	const auto potency = static_cast<long double>(attributes[index(CharacterAttribute::Potency)]);
	const auto technique = static_cast<long double>(attributes[index(CharacterAttribute::Technique)]);
	const auto vigor = static_cast<long double>(attributes[index(CharacterAttribute::Vigor)]);
	const auto attunement = static_cast<long double>(attributes[index(CharacterAttribute::Attunement)]);
	const auto spirit = static_cast<long double>(attributes[index(CharacterAttribute::Spirit)]);

	DerivedStatCalculation result;
	assign(result, DerivedStat::PhysicalAttack, potency * static_cast<long double>(multipliers.potToPhysicalAttack));
	assign(result, DerivedStat::MagicalAttack, attunement * static_cast<long double>(multipliers.sinToMagicalAttack));
	assign(result, DerivedStat::Precision, technique * static_cast<long double>(multipliers.tecToPrecision));
	assign(result, DerivedStat::PhysicalDefense, potency * static_cast<long double>(multipliers.potToPhysicalDefense) + vigor * static_cast<long double>(multipliers.vigToPhysicalDefense));
	assign(result, DerivedStat::MagicalDefense, attunement * static_cast<long double>(multipliers.sinToMagicalDefense) + spirit * static_cast<long double>(multipliers.espToMagicalDefense));
	assign(result, DerivedStat::MaximumHealth, vigor * static_cast<long double>(multipliers.vigToMaximumHealth));
	assign(result, DerivedStat::MaximumMana, spirit * static_cast<long double>(multipliers.espToMaximumMana));
	return result;
}
