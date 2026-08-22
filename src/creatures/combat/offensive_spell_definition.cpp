/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/offensive_spell_definition.hpp"

#include "lib/logging/log_with_spd_log.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <cmath>
	#include <limits>
#endif

namespace {
	std::string_view parameterFieldName(OffensiveParameterField field) {
		switch (field) {
			case OffensiveParameterField::BasePower:
				return "basePower";
			case OffensiveParameterField::PhysicalCoefficient:
				return "physicalCoefficient";
			case OffensiveParameterField::MagicalCoefficient:
				return "magicalCoefficient";
			case OffensiveParameterField::EquipmentCoefficient:
				return "equipmentCoefficient";
			case OffensiveParameterField::SecondaryMultiplier:
				return "secondaryMultiplier";
			case OffensiveParameterField::CooldownMilliseconds:
				return "cooldownMilliseconds";
		}
		return "unknown";
	}

	std::string_view updateResultReason(OffensiveDefinitionUpdateResult result) {
		switch (result) {
			case OffensiveDefinitionUpdateResult::Updated:
				return "updated";
			case OffensiveDefinitionUpdateResult::WrongType:
				return "wrong_type";
			case OffensiveDefinitionUpdateResult::NonFinite:
				return "non_finite";
			case OffensiveDefinitionUpdateResult::BelowMinimum:
				return "below_minimum";
			case OffensiveDefinitionUpdateResult::AboveMaximum:
				return "above_maximum";
			case OffensiveDefinitionUpdateResult::NotInteger:
				return "not_integer";
			case OffensiveDefinitionUpdateResult::Frozen:
				return "frozen";
		}
		return "unknown";
	}

	OffensiveDefinitionUpdateResult validateParameter(OffensiveParameterField field, double value) {
		if (!std::isfinite(value)) {
			return OffensiveDefinitionUpdateResult::NonFinite;
		}
		if (value < 0.0L) {
			return OffensiveDefinitionUpdateResult::BelowMinimum;
		}
		if (field == OffensiveParameterField::SecondaryMultiplier && value > 1.0L) {
			return OffensiveDefinitionUpdateResult::AboveMaximum;
		}
		if (field == OffensiveParameterField::CooldownMilliseconds) {
			if (value > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
				return OffensiveDefinitionUpdateResult::AboveMaximum;
			}
			if (std::floor(value) != value) {
				return OffensiveDefinitionUpdateResult::NotInteger;
			}
		}
		return OffensiveDefinitionUpdateResult::Updated;
	}

	void logValidationFailure(std::string_view spellName, OffensiveParameterField field, OffensiveDefinitionUpdateResult result) {
		g_logger().error(
			"[OffensiveSpellDefinition::validate] spell={} field={} reason={}",
			spellName,
			parameterFieldName(field),
			updateResultReason(result)
		);
	}
}

const OffensivePowerParameters &OffensiveSpellDefinition::parameters() const {
	return powerParameters;
}

const SpellTagSet &OffensiveSpellDefinition::baseTags() const {
	return tags;
}

OffensiveDefinitionUpdateResult OffensiveSpellDefinition::setParameters(const OffensivePowerParameters &newParameters, std::string_view spellName) {
	if (isFrozen) {
		return OffensiveDefinitionUpdateResult::Frozen;
	}

	const std::array values {
		std::pair { OffensiveParameterField::BasePower, newParameters.basePower },
		std::pair { OffensiveParameterField::PhysicalCoefficient, newParameters.physicalCoefficient },
		std::pair { OffensiveParameterField::MagicalCoefficient, newParameters.magicalCoefficient },
		std::pair { OffensiveParameterField::EquipmentCoefficient, newParameters.equipmentCoefficient },
		std::pair { OffensiveParameterField::SecondaryMultiplier, newParameters.secondaryMultiplier },
		std::pair { OffensiveParameterField::CooldownMilliseconds, static_cast<double>(newParameters.cooldownMilliseconds) },
	};
	for (const auto &[field, value] : values) {
		const auto result = validateParameter(field, value);
		if (result != OffensiveDefinitionUpdateResult::Updated) {
			logValidationFailure(spellName, field, result);
			return result;
		}
	}

	powerParameters = newParameters;
	return OffensiveDefinitionUpdateResult::Updated;
}

OffensiveDefinitionUpdateResult OffensiveSpellDefinition::setParameter(OffensiveParameterField field, OffensiveParameterInput input, std::string_view spellName) {
	if (isFrozen) {
		return OffensiveDefinitionUpdateResult::Frozen;
	}
	const auto value = std::get_if<double>(&input);
	if (!value) {
		logValidationFailure(spellName, field, OffensiveDefinitionUpdateResult::WrongType);
		return OffensiveDefinitionUpdateResult::WrongType;
	}

	const auto result = validateParameter(field, *value);
	if (result != OffensiveDefinitionUpdateResult::Updated) {
		logValidationFailure(spellName, field, result);
		return result;
	}

	switch (field) {
		case OffensiveParameterField::BasePower:
			powerParameters.basePower = *value;
			break;
		case OffensiveParameterField::PhysicalCoefficient:
			powerParameters.physicalCoefficient = *value;
			break;
		case OffensiveParameterField::MagicalCoefficient:
			powerParameters.magicalCoefficient = *value;
			break;
		case OffensiveParameterField::EquipmentCoefficient:
			powerParameters.equipmentCoefficient = *value;
			break;
		case OffensiveParameterField::SecondaryMultiplier:
			powerParameters.secondaryMultiplier = *value;
			break;
		case OffensiveParameterField::CooldownMilliseconds:
			powerParameters.cooldownMilliseconds = static_cast<uint32_t>(*value);
			break;
	}
	return OffensiveDefinitionUpdateResult::Updated;
}

OffensiveDefinitionUpdateResult OffensiveSpellDefinition::setBaseTags(SpellTagSet newTags) {
	if (isFrozen) {
		return OffensiveDefinitionUpdateResult::Frozen;
	}
	tags = std::move(newTags);
	return OffensiveDefinitionUpdateResult::Updated;
}

int32_t OffensiveSpellDefinition::calculateBaseDamage(const OffensivePowerInputs &inputs, OffensiveTarget target, std::string_view spellName, uint32_t casterId) const {
	const auto baseDamage = static_cast<long double>(powerParameters.basePower)
		+ static_cast<long double>(inputs.physicalAttack) * static_cast<long double>(powerParameters.physicalCoefficient)
		+ static_cast<long double>(inputs.magicalAttack) * static_cast<long double>(powerParameters.magicalCoefficient)
		+ static_cast<long double>(inputs.equipmentPower) * static_cast<long double>(powerParameters.equipmentCoefficient);
	const auto multiplier = target == OffensiveTarget::Primary ? 1.0L : powerParameters.secondaryMultiplier;
	const auto originalDamage = baseDamage * multiplier;

	if (std::isnan(originalDamage) || originalDamage <= 0.0L) {
		return 0;
	}
	if (!std::isfinite(originalDamage) || originalDamage > static_cast<long double>(std::numeric_limits<int32_t>::max())) {
		g_logger().warn(
			"[OffensiveSpellDefinition::calculateBaseDamage] spell={} caster={} original={} reason=overflow",
			spellName,
			casterId,
			originalDamage
		);
		return std::numeric_limits<int32_t>::max();
	}
	return static_cast<int32_t>(std::floor(originalDamage));
}

void OffensiveSpellDefinition::freeze() {
	isFrozen = true;
}

bool OffensiveSpellDefinition::frozen() const {
	return isFrozen;
}
