/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/offensive_cast_context.hpp"

#include "creatures/combat/combat.hpp"
#include "creatures/combat/offensive_spell_definition.hpp"
#include "creatures/combat/spells.hpp"
#include "creatures/players/components/player_character_stats.hpp"
#include "creatures/players/player.hpp"
#include "game/game.hpp"
#include "items/cylinder.hpp"
#include "items/item.hpp"

namespace {
	SpellTagSet resolveEffectiveTags(const OffensiveSpellDefinition &definition, OffensiveProfile profile) {
		std::vector tags(definition.baseTags().values().begin(), definition.baseTags().values().end());
		switch (profile) {
			case OffensiveProfile::Sword:
				tags.insert(tags.end(), { SpellTag::ExecutionContact, SpellTag::WeaponSword });
				break;
			case OffensiveProfile::Axe:
				tags.insert(tags.end(), { SpellTag::ExecutionArea, SpellTag::ExecutionContact, SpellTag::WeaponAxe });
				break;
			case OffensiveProfile::Club:
				tags.insert(tags.end(), { SpellTag::ExecutionArea, SpellTag::ExecutionContact, SpellTag::WeaponClub });
				break;
			case OffensiveProfile::Bow:
				tags.insert(tags.end(), { SpellTag::ExecutionProjectile, SpellTag::WeaponBow });
				break;
			case OffensiveProfile::Crossbow:
				tags.insert(tags.end(), { SpellTag::ExecutionProjectile, SpellTag::WeaponCrossbow });
				break;
			case OffensiveProfile::Shield:
				tags.insert(tags.end(), { SpellTag::EquipmentShield, SpellTag::ExecutionContact, SpellTag::FunctionControl, SpellTag::MechanicKnockback });
				break;
		}
		const auto &configuredProfileTags = definition.profileTags(profile).values();
		tags.insert(tags.end(), configuredProfileTags.begin(), configuredProfileTags.end());
		return normalizeSpellTags(std::move(tags));
	}

	OffensiveCastContextResult mapEquipmentResult(EquipmentResolutionResult result) {
		switch (result) {
			case EquipmentResolutionResult::Resolved:
				return OffensiveCastContextResult::Created;
			case EquipmentResolutionResult::UnsupportedEquipment:
				return OffensiveCastContextResult::UnsupportedEquipment;
			case EquipmentResolutionResult::MissingAmmunition:
				return OffensiveCastContextResult::MissingAmmunition;
		}
		return OffensiveCastContextResult::UnsupportedEquipment;
	}

	bool validHostileTarget(const std::shared_ptr<Player> &caster, const std::shared_ptr<Creature> &target) {
		return caster && target && caster != target && !target->isRemoved() && !target->isLifeless() && target->isAttackable() && caster->getTile() && target->getTile();
	}
}

std::string_view offensiveCastContextReason(OffensiveCastContextResult result) {
	switch (result) {
		case OffensiveCastContextResult::Created:
			return "created";
		case OffensiveCastContextResult::MissingOffensiveDefinition:
			return "missing_offensive_definition";
		case OffensiveCastContextResult::UnsupportedEquipment:
			return "unsupported_equipment";
		case OffensiveCastContextResult::MissingAmmunition:
			return "missing_ammunition";
		case OffensiveCastContextResult::InvalidPrimaryTarget:
			return "invalid_primary_target";
		case OffensiveCastContextResult::OutOfRange:
			return "out_of_range";
		case OffensiveCastContextResult::LineOfSightBlocked:
			return "line_of_sight_blocked";
		case OffensiveCastContextResult::CombatDenied:
			return "combat_denied";
		case OffensiveCastContextResult::ContextAlreadyCommitted:
			return "context_already_committed";
	}
	return "invalid_primary_target";
}

bool OffensiveCastResult::success() const {
	return result == OffensiveCastContextResult::Created && combatResult == RETURNVALUE_NOERROR;
}

bool OffensiveContextResult::success() const {
	return result == OffensiveCastContextResult::Created && context != nullptr;
}

OffensiveContextResult OffensiveCastContext::create(const Spell &spell, const Player &player) {
	const auto &definition = spell.getOffensiveDefinition();
	if (!definition.has_value()) {
		return { nullptr, OffensiveCastContextResult::MissingOffensiveDefinition };
	}

	auto equipmentResolution = OffensiveEquipmentResolver::resolve(player);
	if (!equipmentResolution.success()) {
		return { nullptr, mapEquipmentResult(equipmentResolution.result) };
	}

	auto caster = std::const_pointer_cast<Player>(player.getPlayer());
	const auto stats = player.characterStats().stats();
	const auto physicalAttack = stats.at(static_cast<size_t>(DerivedStat::PhysicalAttack));
	const auto magicalAttack = stats.at(static_cast<size_t>(DerivedStat::MagicalAttack));
	auto equipment = std::move(*equipmentResolution.snapshot);
	const OffensivePowerInputs inputs {
		.physicalAttack = physicalAttack,
		.magicalAttack = magicalAttack,
		.equipmentPower = equipment.equipmentPower,
	};
	auto tags = resolveEffectiveTags(*definition, equipment.profile);
	const auto primaryDamage = definition->calculateBaseDamage(inputs, OffensiveTarget::Primary, spell.getName(), player.getID());
	const auto secondaryDamage = definition->calculateBaseDamage(inputs, OffensiveTarget::Secondary, spell.getName(), player.getID());

	return {
		std::shared_ptr<OffensiveCastContext>(new OffensiveCastContext(
			caster,
			player.getID(),
			physicalAttack,
			magicalAttack,
			std::move(equipment),
			std::move(tags),
			primaryDamage,
			secondaryDamage
		)),
		OffensiveCastContextResult::Created,
	};
}

OffensiveCastContext::OffensiveCastContext(
	std::weak_ptr<Player> caster,
	uint32_t casterId,
	uint64_t physicalAttack,
	uint64_t magicalAttack,
	OffensiveEquipmentSnapshot equipment,
	SpellTagSet effectiveTags,
	int32_t primaryBaseDamage,
	int32_t secondaryBaseDamage
) :
	casterReference(std::move(caster)),
	frozenCasterId(casterId),
	frozenPhysicalAttack(physicalAttack),
	frozenMagicalAttack(magicalAttack),
	frozenEquipment(std::move(equipment)),
	frozenEffectiveTags(std::move(effectiveTags)),
	frozenPrimaryBaseDamage(primaryBaseDamage),
	frozenSecondaryBaseDamage(secondaryBaseDamage) { }

uint32_t OffensiveCastContext::casterId() const {
	return frozenCasterId;
}

uint64_t OffensiveCastContext::physicalAttack() const {
	return frozenPhysicalAttack;
}

uint64_t OffensiveCastContext::magicalAttack() const {
	return frozenMagicalAttack;
}

const OffensiveEquipmentSnapshot &OffensiveCastContext::equipment() const {
	return frozenEquipment;
}

OffensiveProfile OffensiveCastContext::profile() const {
	return frozenEquipment.profile;
}

int32_t OffensiveCastContext::equipmentPower() const {
	return frozenEquipment.equipmentPower;
}

uint8_t OffensiveCastContext::range() const {
	return frozenEquipment.range;
}

bool OffensiveCastContext::requiresAmmunition() const {
	return frozenEquipment.requiresAmmunition;
}

const SpellTagSet &OffensiveCastContext::effectiveTags() const {
	return frozenEffectiveTags;
}

int32_t OffensiveCastContext::primaryBaseDamage() const {
	return frozenPrimaryBaseDamage;
}

int32_t OffensiveCastContext::secondaryBaseDamage() const {
	return frozenSecondaryBaseDamage;
}

bool OffensiveCastContext::committed() const {
	return isCommitted;
}

OffensiveCastResult OffensiveCastContext::validatePrimaryTarget(const std::shared_ptr<Creature> &target) const {
	const auto caster = casterReference.lock();
	if (!validHostileTarget(caster, target)) {
		return { OffensiveCastContextResult::InvalidPrimaryTarget, RETURNVALUE_NOERROR };
	}

	const auto &from = caster->getPosition();
	const auto &to = target->getPosition();
	if (from.z != to.z || Position::getDistanceX(from, to) > frozenEquipment.range || Position::getDistanceY(from, to) > frozenEquipment.range) {
		return { OffensiveCastContextResult::OutOfRange, RETURNVALUE_NOERROR };
	}
	if (!g_game().canThrowObjectTo(from, to, SightLine_CheckSightLineAndFloor, frozenEquipment.range, frozenEquipment.range)) {
		return { OffensiveCastContextResult::LineOfSightBlocked, RETURNVALUE_CANNOTTHROW };
	}

	const auto combatResult = Combat::canTargetCreature(caster, target);
	if (combatResult != RETURNVALUE_NOERROR) {
		return { OffensiveCastContextResult::CombatDenied, combatResult };
	}
	return { OffensiveCastContextResult::Created, RETURNVALUE_NOERROR };
}

bool OffensiveCastContext::canAffect(const std::shared_ptr<Creature> &target) const {
	const auto caster = casterReference.lock();
	return validHostileTarget(caster, target) && Combat::canTargetCreature(caster, target) == RETURNVALUE_NOERROR;
}

bool OffensiveCastContext::ammunitionStillReserved(const std::shared_ptr<Player> &caster) const {
	if (!frozenEquipment.requiresAmmunition) {
		return true;
	}
	const auto &ammunition = frozenEquipment.ammunition;
	const auto &parent = frozenEquipment.ammunitionParent;
	return ammunition && parent && ammunition->getParent() == parent && parent->getThingIndex(ammunition) != -1 && ammunition->getHoldingPlayer() == caster && ammunition->getItemCount() >= 1;
}

OffensiveCastResult OffensiveCastContext::commit(const std::shared_ptr<Creature> &target) {
	if (isCommitted) {
		return { OffensiveCastContextResult::ContextAlreadyCommitted, RETURNVALUE_NOERROR };
	}
	const auto validation = validatePrimaryTarget(target);
	if (!validation.success()) {
		return validation;
	}

	const auto caster = casterReference.lock();
	if (!ammunitionStillReserved(caster)) {
		return { OffensiveCastContextResult::MissingAmmunition, RETURNVALUE_NOERROR };
	}
	if (frozenEquipment.requiresAmmunition) {
		if (g_game().internalRemoveItem(frozenEquipment.ammunition, 1, true) != RETURNVALUE_NOERROR) {
			return { OffensiveCastContextResult::MissingAmmunition, RETURNVALUE_NOERROR };
		}
		if (g_game().internalRemoveItem(frozenEquipment.ammunition, 1) != RETURNVALUE_NOERROR) {
			return { OffensiveCastContextResult::MissingAmmunition, RETURNVALUE_NOERROR };
		}
	}
	isCommitted = true;
	return { OffensiveCastContextResult::Created, RETURNVALUE_NOERROR };
}
