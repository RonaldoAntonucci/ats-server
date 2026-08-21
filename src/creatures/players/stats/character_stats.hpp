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
	#include <array>
	#include <cstdint>
	#include <string_view>
#endif

enum class CharacterAttribute : uint8_t {
	Potency,
	Technique,
	Vigor,
	Attunement,
	Spirit,
	Last,
};

enum class DerivedStat : uint8_t {
	PhysicalAttack,
	MagicalAttack,
	Precision,
	PhysicalDefense,
	MagicalDefense,
	MaximumHealth,
	MaximumMana,
	Last,
};

using AttributeContributions = std::array<uint32_t, static_cast<size_t>(CharacterAttribute::Last)>;
using AttributeTotals = std::array<uint64_t, static_cast<size_t>(CharacterAttribute::Last)>;
using DerivedStatTotals = std::array<uint64_t, static_cast<size_t>(DerivedStat::Last)>;

inline constexpr uint64_t maxPublicDerivedStat = (uint64_t { 1 } << 53) - 1;

struct DerivedStatMultipliers {
	double potToPhysicalAttack = 1.0;
	double potToPhysicalDefense = 0.3;
	double tecToPrecision = 1.0;
	double vigToMaximumHealth = 5.0;
	double vigToPhysicalDefense = 0.7;
	double sinToMagicalAttack = 1.0;
	double sinToMagicalDefense = 0.3;
	double espToMaximumMana = 5.0;
	double espToMagicalDefense = 0.7;
};

struct DerivedStatCalculation {
	DerivedStatTotals totals {};
	std::array<bool, static_cast<size_t>(DerivedStat::Last)> saturated {};
};

[[nodiscard]] std::string_view characterAttributeId(CharacterAttribute attribute);
[[nodiscard]] std::string_view derivedStatId(DerivedStat stat);
[[nodiscard]] DerivedStatCalculation calculateDerivedStats(const AttributeTotals &attributes, const DerivedStatMultipliers &multipliers);
