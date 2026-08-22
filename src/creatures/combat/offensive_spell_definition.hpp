/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once

#include "creatures/combat/offensive_equipment_resolver.hpp"
#include "creatures/combat/spell_tags.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <array>
	#include <cstdint>
	#include <string>
	#include <string_view>
	#include <variant>
#endif

struct OffensivePowerParameters {
	double basePower = 10.0;
	double physicalCoefficient = 1.0;
	double magicalCoefficient = 0.0;
	double equipmentCoefficient = 1.0;
	double secondaryMultiplier = 0.5;
	uint32_t cooldownMilliseconds = 1000;
};

struct OffensivePowerInputs {
	uint64_t physicalAttack = 0;
	uint64_t magicalAttack = 0;
	int32_t equipmentPower = 0;
};

enum class OffensiveTarget : uint8_t {
	Primary,
	Secondary,
};

enum class OffensiveParameterField : uint8_t {
	BasePower,
	PhysicalCoefficient,
	MagicalCoefficient,
	EquipmentCoefficient,
	SecondaryMultiplier,
	CooldownMilliseconds,
};

enum class OffensiveDefinitionUpdateResult : uint8_t {
	Updated,
	WrongType,
	NonFinite,
	BelowMinimum,
	AboveMaximum,
	NotInteger,
	Frozen,
};

using OffensiveParameterInput = std::variant<double, bool, std::string>;

class OffensiveSpellDefinition {
public:
	[[nodiscard]] const OffensivePowerParameters &parameters() const;
	[[nodiscard]] const SpellTagSet &baseTags() const;
	[[nodiscard]] const SpellTagSet &profileTags(OffensiveProfile profile) const;

	[[nodiscard]] OffensiveDefinitionUpdateResult setParameters(const OffensivePowerParameters &newParameters, std::string_view spellName);
	[[nodiscard]] OffensiveDefinitionUpdateResult setParameter(OffensiveParameterField field, OffensiveParameterInput input, std::string_view spellName);
	[[nodiscard]] OffensiveDefinitionUpdateResult setBaseTags(SpellTagSet tags);
	[[nodiscard]] OffensiveDefinitionUpdateResult setProfileTags(OffensiveProfile profile, SpellTagSet tags);

	[[nodiscard]] int32_t calculateBaseDamage(const OffensivePowerInputs &inputs, OffensiveTarget target, std::string_view spellName, uint32_t casterId) const;

	void freeze();
	[[nodiscard]] bool frozen() const;
	void invalidate();
	[[nodiscard]] bool valid() const;

private:
	OffensivePowerParameters powerParameters;
	SpellTagSet tags;
	std::array<SpellTagSet, static_cast<size_t>(OffensiveProfile::Shield) + 1> tagsByProfile;
	bool isFrozen = false;
	bool isValid = true;
};
