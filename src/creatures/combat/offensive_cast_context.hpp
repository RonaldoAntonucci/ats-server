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
#include "items/items_definitions.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <cstdint>
	#include <memory>
	#include <string_view>
#endif

class Creature;
class Player;
class Spell;
class OffensiveCastContext;

enum class OffensiveCastContextResult : uint8_t {
	Created,
	MissingOffensiveDefinition,
	UnsupportedEquipment,
	MissingAmmunition,
	InvalidPrimaryTarget,
	OutOfRange,
	LineOfSightBlocked,
	CombatDenied,
	ContextAlreadyCommitted,
};

[[nodiscard]] std::string_view offensiveCastContextReason(OffensiveCastContextResult result);

struct OffensiveCastResult {
	OffensiveCastContextResult result = OffensiveCastContextResult::InvalidPrimaryTarget;
	ReturnValue combatResult = RETURNVALUE_NOERROR;

	[[nodiscard]] bool success() const;
};

struct OffensiveContextResult {
	std::shared_ptr<OffensiveCastContext> context;
	OffensiveCastContextResult result = OffensiveCastContextResult::MissingOffensiveDefinition;

	[[nodiscard]] bool success() const;
};

class OffensiveCastContext {
public:
	[[nodiscard]] static OffensiveContextResult create(const Spell &spell, const Player &player);

	[[nodiscard]] uint32_t casterId() const;
	[[nodiscard]] uint64_t physicalAttack() const;
	[[nodiscard]] uint64_t magicalAttack() const;
	[[nodiscard]] const OffensiveEquipmentSnapshot &equipment() const;
	[[nodiscard]] OffensiveProfile profile() const;
	[[nodiscard]] int32_t equipmentPower() const;
	[[nodiscard]] uint8_t range() const;
	[[nodiscard]] bool requiresAmmunition() const;
	[[nodiscard]] const SpellTagSet &effectiveTags() const;
	[[nodiscard]] int32_t primaryBaseDamage() const;
	[[nodiscard]] int32_t secondaryBaseDamage() const;
	[[nodiscard]] bool committed() const;

	[[nodiscard]] OffensiveCastResult validatePrimaryTarget(const std::shared_ptr<Creature> &target) const;
	[[nodiscard]] bool canAffect(const std::shared_ptr<Creature> &target) const;
	[[nodiscard]] OffensiveCastResult commit(const std::shared_ptr<Creature> &target);

private:
	OffensiveCastContext(
		std::weak_ptr<Player> caster,
		uint32_t casterId,
		uint64_t physicalAttack,
		uint64_t magicalAttack,
		OffensiveEquipmentSnapshot equipment,
		SpellTagSet effectiveTags,
		int32_t primaryBaseDamage,
		int32_t secondaryBaseDamage
	);

	[[nodiscard]] bool ammunitionStillReserved(const std::shared_ptr<Player> &caster) const;

	std::weak_ptr<Player> casterReference;
	const uint32_t frozenCasterId;
	const uint64_t frozenPhysicalAttack;
	const uint64_t frozenMagicalAttack;
	const OffensiveEquipmentSnapshot frozenEquipment;
	const SpellTagSet frozenEffectiveTags;
	const int32_t frozenPrimaryBaseDamage;
	const int32_t frozenSecondaryBaseDamage;
	bool isCommitted = false;
};
