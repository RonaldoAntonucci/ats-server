/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "lua/functions/creatures/combat/spell_functions.hpp"

#include "creatures/combat/offensive_cast_context.hpp"
#include "creatures/combat/spells.hpp"
#include "creatures/players/player.hpp"
#include "creatures/players/vocations/vocation.hpp"
#include "items/item.hpp"
#include "utils/tools.hpp"
#include "lua/functions/lua_functions_loader.hpp"

namespace {
	std::string_view disciplineDefinitionReason(SpellRequirementDefinitionResult result) {
		switch (result) {
			case SpellRequirementDefinitionResult::Added:
				return "added";
			case SpellRequirementDefinitionResult::Duplicate:
				return "duplicate";
			case SpellRequirementDefinitionResult::InvalidDisciplineId:
				return "invalid_discipline_id";
			case SpellRequirementDefinitionResult::InvalidMinimumRank:
				return "invalid_minimum_rank";
			case SpellRequirementDefinitionResult::UnknownDiscipline:
				return "unknown_discipline";
			case SpellRequirementDefinitionResult::ConflictingDuplicate:
				return "conflicting_duplicate";
		}
		return "invalid_definition";
	}

	std::string_view offensiveUpdateReason(OffensiveDefinitionUpdateResult result) {
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
		return "invalid_definition";
	}

	std::optional<OffensiveProfile> parseOffensiveProfile(std::string_view profile) {
		if (profile == "sword") {
			return OffensiveProfile::Sword;
		}
		if (profile == "axe") {
			return OffensiveProfile::Axe;
		}
		if (profile == "club") {
			return OffensiveProfile::Club;
		}
		if (profile == "bow") {
			return OffensiveProfile::Bow;
		}
		if (profile == "crossbow") {
			return OffensiveProfile::Crossbow;
		}
		if (profile == "shield") {
			return OffensiveProfile::Shield;
		}
		return std::nullopt;
	}

	std::string_view offensiveProfileName(OffensiveProfile profile) {
		switch (profile) {
			case OffensiveProfile::Sword:
				return "sword";
			case OffensiveProfile::Axe:
				return "axe";
			case OffensiveProfile::Club:
				return "club";
			case OffensiveProfile::Bow:
				return "bow";
			case OffensiveProfile::Crossbow:
				return "crossbow";
			case OffensiveProfile::Shield:
				return "shield";
		}
		return "";
	}

	void pushBooleanReason(lua_State* L, bool success, std::string_view reason) {
		Lua::pushBoolean(L, success);
		Lua::pushString(L, std::string(reason));
	}

	std::optional<std::vector<std::string>> readTagNames(lua_State* L, int32_t index) {
		if (!Lua::isTable(L, index)) {
			return std::nullopt;
		}
		const auto absoluteIndex = index > 0 ? index : lua_gettop(L) + index + 1;
		const auto count = lua_objlen(L, absoluteIndex);
		std::vector<std::string> names;
		names.reserve(count);
		for (size_t entry = 1; entry <= count; ++entry) {
			lua_rawgeti(L, absoluteIndex, static_cast<int>(entry));
			if (lua_type(L, -1) != LUA_TSTRING) {
				lua_pop(L, 1);
				return std::nullopt;
			}
			names.emplace_back(Lua::getString(L, -1));
			lua_pop(L, 1);
		}
		return names;
	}

	SpellTagSetBuildResult buildTags(const std::vector<std::string> &names) {
		std::vector<std::string_view> views;
		views.reserve(names.size());
		for (const auto &name : names) {
			views.emplace_back(name);
		}
		return buildSpellTagSet(views);
	}

}

void SpellFunctions::init(lua_State* L) {
	Lua::registerSharedClass<Spell>(L, "", SpellFunctions::luaSpellCreate);
	Lua::registerMetaMethod(L, "Spell", "__eq", Lua::luaUserdataCompare);
	Lua::registerSharedClass<OffensiveCastContext>(L, "");

	/***
	 * @function Spell:onCastSpell
	 * @param callback fun(creature: Creature, variant: Variant, isHotkey?: boolean): boolean
	 * @return boolean
	 */
	Lua::registerMethod(L, "Spell", "onCastSpell", SpellFunctions::luaSpellOnCastSpell);
	Lua::registerMethod(L, "Spell", "register", SpellFunctions::luaSpellRegister);
	Lua::registerMethod(L, "Spell", "name", SpellFunctions::luaSpellName);
	Lua::registerMethod(L, "Spell", "id", SpellFunctions::luaSpellId);
	Lua::registerMethod(L, "Spell", "group", SpellFunctions::luaSpellGroup);
	Lua::registerMethod(L, "Spell", "cooldown", SpellFunctions::luaSpellCooldown);
	Lua::registerMethod(L, "Spell", "groupCooldown", SpellFunctions::luaSpellGroupCooldown);
	Lua::registerMethod(L, "Spell", "level", SpellFunctions::luaSpellLevel);
	Lua::registerMethod(L, "Spell", "magicLevel", SpellFunctions::luaSpellMagicLevel);
	Lua::registerMethod(L, "Spell", "mana", SpellFunctions::luaSpellMana);
	Lua::registerMethod(L, "Spell", "manaPercent", SpellFunctions::luaSpellManaPercent);
	Lua::registerMethod(L, "Spell", "soul", SpellFunctions::luaSpellSoul);
	Lua::registerMethod(L, "Spell", "range", SpellFunctions::luaSpellRange);
	Lua::registerMethod(L, "Spell", "isPremium", SpellFunctions::luaSpellPremium);
	Lua::registerMethod(L, "Spell", "isEnabled", SpellFunctions::luaSpellEnabled);
	Lua::registerMethod(L, "Spell", "needTarget", SpellFunctions::luaSpellNeedTarget);
	Lua::registerMethod(L, "Spell", "needWeapon", SpellFunctions::luaSpellNeedWeapon);
	Lua::registerMethod(L, "Spell", "needLearn", SpellFunctions::luaSpellNeedLearn);
	Lua::registerMethod(L, "Spell", "allowOnSelf", SpellFunctions::luaSpellAllowOnSelf);
	Lua::registerMethod(L, "Spell", "setPzLocked", SpellFunctions::luaSpellPzLocked);
	Lua::registerMethod(L, "Spell", "isSelfTarget", SpellFunctions::luaSpellSelfTarget);
	Lua::registerMethod(L, "Spell", "isBlocking", SpellFunctions::luaSpellBlocking);
	Lua::registerMethod(L, "Spell", "isAggressive", SpellFunctions::luaSpellAggressive);
	Lua::registerMethod(L, "Spell", "vocation", SpellFunctions::luaSpellVocation);
	Lua::registerMethod(L, "Spell", "disciplineRequirement", SpellFunctions::luaSpellDisciplineRequirement);
	Lua::registerMethod(L, "Spell", "tag", SpellFunctions::luaSpellTag);
	Lua::registerMethod(L, "Spell", "hasTag", SpellFunctions::luaSpellHasTag);
	Lua::registerMethod(L, "Spell", "getTags", SpellFunctions::luaSpellGetTags);
	Lua::registerMethod(L, "Spell", "offensiveParameters", SpellFunctions::luaSpellOffensiveParameters);
	Lua::registerMethod(L, "Spell", "baseTags", SpellFunctions::luaSpellBaseTags);
	Lua::registerMethod(L, "Spell", "profileTags", SpellFunctions::luaSpellProfileTags);
	Lua::registerMethod(L, "Spell", "createOffensiveContext", SpellFunctions::luaSpellCreateOffensiveContext);

	Lua::registerMethod(L, "OffensiveCastContext", "getProfile", SpellFunctions::luaOffensiveContextProfile);
	Lua::registerMethod(L, "OffensiveCastContext", "getEquipmentPower", SpellFunctions::luaOffensiveContextEquipmentPower);
	Lua::registerMethod(L, "OffensiveCastContext", "getRange", SpellFunctions::luaOffensiveContextRange);
	Lua::registerMethod(L, "OffensiveCastContext", "requiresAmmunition", SpellFunctions::luaOffensiveContextRequiresAmmunition);
	Lua::registerMethod(L, "OffensiveCastContext", "getTags", SpellFunctions::luaOffensiveContextTags);
	Lua::registerMethod(L, "OffensiveCastContext", "getPrimaryBaseDamage", SpellFunctions::luaOffensiveContextPrimaryBaseDamage);
	Lua::registerMethod(L, "OffensiveCastContext", "getSecondaryBaseDamage", SpellFunctions::luaOffensiveContextSecondaryBaseDamage);
	Lua::registerMethod(L, "OffensiveCastContext", "validatePrimaryTarget", SpellFunctions::luaOffensiveContextValidatePrimaryTarget);
	Lua::registerMethod(L, "OffensiveCastContext", "canAffect", SpellFunctions::luaOffensiveContextCanAffect);
	Lua::registerMethod(L, "OffensiveCastContext", "commit", SpellFunctions::luaOffensiveContextCommit);

	Lua::registerMethod(L, "Spell", "castSound", SpellFunctions::luaSpellCastSound);
	Lua::registerMethod(L, "Spell", "impactSound", SpellFunctions::luaSpellImpactSound);

	// Only for InstantSpell.
	Lua::registerMethod(L, "Spell", "words", SpellFunctions::luaSpellWords);
	Lua::registerMethod(L, "Spell", "needDirection", SpellFunctions::luaSpellNeedDirection);
	Lua::registerMethod(L, "Spell", "hasParams", SpellFunctions::luaSpellHasParams);
	Lua::registerMethod(L, "Spell", "hasPlayerNameParam", SpellFunctions::luaSpellHasPlayerNameParam);
	Lua::registerMethod(L, "Spell", "needCasterTargetOrDirection", SpellFunctions::luaSpellNeedCasterTargetOrDirection);
	Lua::registerMethod(L, "Spell", "isBlockingWalls", SpellFunctions::luaSpellIsBlockingWalls);

	// Only for RuneSpells.
	Lua::registerMethod(L, "Spell", "runeId", SpellFunctions::luaSpellRuneId);
	Lua::registerMethod(L, "Spell", "charges", SpellFunctions::luaSpellCharges);
	Lua::registerMethod(L, "Spell", "allowFarUse", SpellFunctions::luaSpellAllowFarUse);
	Lua::registerMethod(L, "Spell", "blockWalls", SpellFunctions::luaSpellBlockWalls);
	Lua::registerMethod(L, "Spell", "checkFloor", SpellFunctions::luaSpellCheckFloor);
	Lua::registerMethod(L, "Spell", "monkSpellType", SpellFunctions::luaSpellMonkSpellType);
}

/***
 * @class Spell
 * @overload fun(nameOrTypeOrId: string|integer): Spell?
 */
int SpellFunctions::luaSpellCreate(lua_State* L) {
	// Spell(words, name or id) to get an existing spell
	// Spell(type) ex: Spell(SPELL_INSTANT) or Spell(SPELL_RUNE) to create a new spell
	if (lua_gettop(L) == 1) {
		g_logger().error("[SpellFunctions::luaSpellCreate] - "
		                 "There is no parameter set!");
		lua_pushnil(L);
		return 1;
	}

	SpellType_t spellType = SPELL_UNDEFINED;

	if (Lua::isNumber(L, 2)) {
		uint16_t id = Lua::getNumber<uint16_t>(L, 2);
		const auto &rune = g_spells().getRuneSpell(id);

		if (rune) {
			Lua::pushSharedUserdata<Spell>(L, rune);
			return 1;
		}

		spellType = static_cast<SpellType_t>(id);
	} else if (Lua::isString(L, 2)) {
		const std::string arg = Lua::getString(L, 2);
		auto instant = g_spells().getInstantSpellByName(arg);
		if (instant) {
			Lua::pushSharedUserdata<Spell>(L, instant);
			return 1;
		}
		instant = g_spells().getInstantSpell(arg);
		if (instant) {
			Lua::pushSharedUserdata<Spell>(L, instant);
			return 1;
		}
		const auto &rune = g_spells().getRuneSpellByName(arg);
		if (rune) {
			Lua::pushSharedUserdata<Spell>(L, rune);
			return 1;
		}

		const std::string tmp = asLowerCaseString(arg);
		if (tmp == "instant") {
			spellType = SPELL_INSTANT;
		} else if (tmp == "rune") {
			spellType = SPELL_RUNE;
		}
	}

	if (spellType == SPELL_INSTANT) {
		const auto &spell = std::make_shared<InstantSpell>();
		Lua::pushSharedUserdata<Spell>(L, spell);
		spell->spellType = SPELL_INSTANT;
		return 1;
	} else if (spellType == SPELL_RUNE) {
		const auto &runeSpell = std::make_shared<RuneSpell>();
		Lua::pushSharedUserdata<Spell>(L, runeSpell);
		runeSpell->spellType = SPELL_RUNE;
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

int SpellFunctions::luaSpellOnCastSpell(lua_State* L) {
	// spell:onCastSpell(callback)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (spell->spellType == SPELL_INSTANT) {
			const auto &instant = std::static_pointer_cast<InstantSpell>(spell);
			if (!instant->loadScriptId()) {
				Lua::pushBoolean(L, false);
				return 1;
			}
			Lua::pushBoolean(L, true);
		} else if (spell->spellType == SPELL_RUNE) {
			const auto &rune = std::static_pointer_cast<RuneSpell>(spell);
			if (!rune->loadRuneSpellScriptId()) {
				Lua::pushBoolean(L, false);
				return 1;
			}
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellRegister(lua_State* L) {
	// spell:register()
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (!spell) {
		Lua::reportErrorFunc(Lua::getErrorDesc(LUA_ERROR_SPELL_NOT_FOUND));
		Lua::pushBoolean(L, false);
		return 1;
	}
	if (spell->getOffensiveDefinition().has_value() && !spell->getOffensiveDefinition()->valid()) {
		g_logger().error("[SpellFunctions::luaSpellRegister] spell={} reason=invalid_offensive_definition", spell->getName());
		Lua::pushBoolean(L, false);
		return 1;
	}

	if (spell->spellType == SPELL_INSTANT) {
		const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
		const auto &instant = std::static_pointer_cast<InstantSpell>(spellBase);
		if (!instant->isLoadedScriptId()) {
			Lua::pushBoolean(L, false);
			return 1;
		}
		const auto registered = g_spells().registerInstantLuaEvent(instant);
		if (registered && spell->getOffensiveDefinition().has_value()) {
			spell->getOrCreateOffensiveDefinition().freeze();
		}
		Lua::pushBoolean(L, registered);
	} else if (spell->spellType == SPELL_RUNE) {
		const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
		const auto &rune = std::static_pointer_cast<RuneSpell>(spellBase);
		if (rune->getMagicLevel() != 0 || rune->getLevel() != 0) {
			// Change information in the ItemType to get accurate description
			ItemType &iType = Item::items.getItemType(rune->getRuneItemId());
			// If the item is not registered in items.xml then we will register it by rune name
			if (iType.name.empty()) {
				iType.name = rune->getName();
			}
			iType.runeMagLevel = rune->getMagicLevel();
			iType.runeLevel = rune->getLevel();
			iType.charges = rune->getCharges();
		}
		if (!rune->isRuneSpellLoadedScriptId()) {
			Lua::pushBoolean(L, false);
			return 1;
		}
		const auto registered = g_spells().registerRuneLuaEvent(rune);
		if (registered && spell->getOffensiveDefinition().has_value()) {
			spell->getOrCreateOffensiveDefinition().freeze();
		}
		Lua::pushBoolean(L, registered);
	}
	return 1;
}

int SpellFunctions::luaSpellName(lua_State* L) {

	// spell:name(name)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			Lua::pushString(L, spell->getName());
		} else {
			spell->setName(Lua::getString(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellId(lua_State* L) {
	// spell:id(id)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (spell->spellType != SPELL_INSTANT && spell->spellType != SPELL_RUNE) {
			Lua::reportErrorFunc("The method: 'spell:id(id)' is only for use of instant spells and rune spells");
			Lua::pushBoolean(L, false);
			return 1;
		}
		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, spell->getSpellId());
		} else {
			spell->setSpellId(Lua::getNumber<uint16_t>(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellGroup(lua_State* L) {
	// spell:group(primaryGroup[, secondaryGroup])
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, spell->getGroup());
			lua_pushnumber(L, spell->getSecondaryGroup());
			return 2;
		} else if (lua_gettop(L) == 2) {
			auto group = Lua::getNumber<SpellGroup_t>(L, 2);
			if (group) {
				spell->setGroup(group);
				Lua::pushBoolean(L, true);
			} else if (Lua::isString(L, 2)) {
				group = stringToSpellGroup(Lua::getString(L, 2));
				if (group != SPELLGROUP_NONE) {
					spell->setGroup(group);
				} else {
					g_logger().warn("[SpellFunctions::luaSpellGroup] - "
					                "Unknown group: {}",
					                Lua::getString(L, 2));
					Lua::pushBoolean(L, false);
					return 1;
				}
				Lua::pushBoolean(L, true);
			} else {
				g_logger().warn("[SpellFunctions::luaSpellGroup] - "
				                "Unknown group: {}",
				                Lua::getString(L, 2));
				Lua::pushBoolean(L, false);
				return 1;
			}
		} else {
			auto primaryGroup = Lua::getNumber<SpellGroup_t>(L, 2);
			auto secondaryGroup = Lua::getNumber<SpellGroup_t>(L, 2);
			if (primaryGroup && secondaryGroup) {
				spell->setGroup(primaryGroup);
				spell->setSecondaryGroup(secondaryGroup);
				Lua::pushBoolean(L, true);
			} else if (Lua::isString(L, 2) && Lua::isString(L, 3)) {
				primaryGroup = stringToSpellGroup(Lua::getString(L, 2));
				if (primaryGroup != SPELLGROUP_NONE) {
					spell->setGroup(primaryGroup);
				} else {
					g_logger().warn("[SpellFunctions::luaSpellGroup] - "
					                "Unknown primaryGroup: {}",
					                Lua::getString(L, 2));
					Lua::pushBoolean(L, false);
					return 1;
				}
				secondaryGroup = stringToSpellGroup(Lua::getString(L, 3));
				if (secondaryGroup != SPELLGROUP_NONE) {
					spell->setSecondaryGroup(secondaryGroup);
				} else {
					g_logger().warn("[SpellFunctions::luaSpellGroup] - "
					                "Unknown secondaryGroup: {}",
					                Lua::getString(L, 3));
					Lua::pushBoolean(L, false);
					return 1;
				}
				Lua::pushBoolean(L, true);
			} else {
				g_logger().warn("[SpellFunctions::luaSpellGroup] - "
				                "Unknown primaryGroup: {} or secondaryGroup: {}",
				                Lua::getString(L, 2), Lua::getString(L, 3));
				Lua::pushBoolean(L, false);
				return 1;
			}
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellCastSound(lua_State* L) {
	// get: spell:castSound() set: spell:castSound(effect)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, static_cast<uint16_t>(spell->soundCastEffect));
		} else {
			spell->soundCastEffect = static_cast<SoundEffect_t>(Lua::getNumber<uint16_t>(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellImpactSound(lua_State* L) {
	// get: spell:impactSound() set: spell:impactSound(effect)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, static_cast<uint16_t>(spell->soundImpactEffect));
		} else {
			spell->soundImpactEffect = static_cast<SoundEffect_t>(Lua::getNumber<uint16_t>(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellCooldown(lua_State* L) {
	// spell:cooldown(cooldown)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, spell->getCooldown());
		} else {
			spell->setCooldown(Lua::getNumber<uint32_t>(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellGroupCooldown(lua_State* L) {
	// spell:groupCooldown(primaryGroupCd[, secondaryGroupCd])
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, spell->getGroupCooldown());
			lua_pushnumber(L, spell->getSecondaryCooldown());
			return 2;
		} else if (lua_gettop(L) == 2) {
			spell->setGroupCooldown(Lua::getNumber<uint32_t>(L, 2));
			Lua::pushBoolean(L, true);
		} else {
			spell->setGroupCooldown(Lua::getNumber<uint32_t>(L, 2));
			spell->setSecondaryCooldown(Lua::getNumber<uint32_t>(L, 3));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellLevel(lua_State* L) {
	// spell:level(lvl)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, spell->getLevel());
		} else {
			spell->setLevel(Lua::getNumber<uint32_t>(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellMagicLevel(lua_State* L) {
	// spell:magicLevel(lvl)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, spell->getMagicLevel());
		} else {
			spell->setMagicLevel(Lua::getNumber<uint32_t>(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellMana(lua_State* L) {
	// spell:mana(mana)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, spell->getMana());
		} else {
			spell->setMana(Lua::getNumber<uint32_t>(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellManaPercent(lua_State* L) {
	// spell:manaPercent(percent)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, spell->getManaPercent());
		} else {
			spell->setManaPercent(Lua::getNumber<uint32_t>(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellSoul(lua_State* L) {
	// spell:soul(soul)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, spell->getSoulCost());
		} else {
			spell->setSoulCost(Lua::getNumber<uint32_t>(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellRange(lua_State* L) {
	// spell:range(range)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, spell->getRange());
		} else {
			spell->setRange(Lua::getNumber<int32_t>(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellPremium(lua_State* L) {
	// spell:isPremium(bool)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->isPremium());
		} else {
			spell->setPremium(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellEnabled(lua_State* L) {
	// spell:isEnabled(bool)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->isEnabled());
		} else {
			spell->setEnabled(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellNeedTarget(lua_State* L) {
	// spell:needTarget(bool)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getNeedTarget());
		} else {
			spell->setNeedTarget(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellNeedWeapon(lua_State* L) {
	// spell:needWeapon(bool)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getNeedWeapon());
		} else {
			spell->setNeedWeapon(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellNeedLearn(lua_State* L) {
	// spell:needLearn(bool)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getNeedLearn());
		} else {
			spell->setNeedLearn(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellSelfTarget(lua_State* L) {
	// spell:isSelfTarget(bool)
	if (const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell")) {
		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getSelfTarget());
		} else {
			spell->setSelfTarget(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellBlocking(lua_State* L) {
	// spell:isBlocking(blockingSolid, blockingCreature)
	if (const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell")) {
		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getBlockingSolid());
			Lua::pushBoolean(L, spell->getBlockingCreature());
			return 2;
		} else {
			spell->setBlockingSolid(Lua::getBoolean(L, 2));
			spell->setBlockingCreature(Lua::getBoolean(L, 3));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellAggressive(lua_State* L) {
	// spell:isAggressive(bool)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getAggressive());
		} else {
			spell->setAggressive(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellAllowOnSelf(lua_State* L) {
	// spell:allowOnSelf(bool)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getAllowOnSelf());
		} else {
			spell->setAllowOnSelf(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellPzLocked(lua_State* L) {
	// spell:isPzLocked(bool)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getLockedPZ());
		} else {
			spell->setLockedPZ(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellVocation(lua_State* L) {
	// spell:vocation(vocation)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			lua_createtable(L, 0, 0);
			auto it = 0;
			for (const auto &voc : spell->getVocMap()) {
				++it;
				std::string s = std::to_string(it);
				const char* pchar = s.c_str();
				std::string name = g_vocations().getVocation(voc.first)->getVocName();
				Lua::setField(L, pchar, name);
			}
			Lua::setMetatable(L, -1, "Spell");
		} else {
			const int parameters = lua_gettop(L) - 1; // - 1 because self is a parameter aswell, which we want to skip ofc
			for (int i = 0; i < parameters; ++i) {
				if (Lua::getString(L, 2 + i).find(';') != std::string::npos) {
					std::vector<std::string> vocList = explodeString(Lua::getString(L, 2 + i), ";");
					const int32_t vocationId = g_vocations().getVocationId(vocList[0]);
					if (!vocList.empty()) {
						if (vocList[1] == "true") {
							spell->addVocMap(vocationId, true);
						} else {
							spell->addVocMap(vocationId, false);
						}
					}
				} else {
					const int32_t vocationId = g_vocations().getVocationId(Lua::getString(L, 2 + i));
					spell->addVocMap(vocationId, false);
				}
			}
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
 * @function Spell:disciplineRequirement
 * @param disciplineId integer
 * @param minimumRank integer
 * @return boolean success
 * @return string reason
 */
int SpellFunctions::luaSpellDisciplineRequirement(lua_State* L) {
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (!spell || !Lua::isNumber(L, 2) || !Lua::isNumber(L, 3)) {
		pushBooleanReason(L, false, "wrong_type");
		return 2;
	}
	const auto disciplineId = lua_tonumber(L, 2);
	const auto minimumRank = lua_tonumber(L, 3);
	if (std::floor(disciplineId) != disciplineId || std::floor(minimumRank) != minimumRank
	    || disciplineId < 0 || disciplineId > std::numeric_limits<uint16_t>::max()
	    || minimumRank < 0 || minimumRank > std::numeric_limits<uint32_t>::max()) {
		pushBooleanReason(L, false, "wrong_type");
		return 2;
	}
	const auto result = spell->addDisciplineRequirement(static_cast<uint16_t>(disciplineId), static_cast<uint32_t>(minimumRank));
	pushBooleanReason(L, result == SpellRequirementDefinitionResult::Added || result == SpellRequirementDefinitionResult::Duplicate, disciplineDefinitionReason(result));
	return 2;
}

/***
 * @function Spell:tag
 * @param tag string
 * @return boolean success
 */
int SpellFunctions::luaSpellTag(lua_State* L) {
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (!spell || lua_type(L, 2) != LUA_TSTRING) {
		Lua::pushBoolean(L, false);
		return 1;
	}
	Lua::pushBoolean(L, spell->addTag(Lua::getString(L, 2)));
	return 1;
}

/***
 * @function Spell:hasTag
 * @param tag string
 * @return boolean
 */
int SpellFunctions::luaSpellHasTag(lua_State* L) {
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (!spell || lua_type(L, 2) != LUA_TSTRING) {
		Lua::pushBoolean(L, false);
		return 1;
	}
	Lua::pushBoolean(L, spell->hasTag(Lua::getString(L, 2)));
	return 1;
}

/***
 * @function Spell:getTags
 * @return string[]|nil
 */
int SpellFunctions::luaSpellGetTags(lua_State* L) {
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (!spell) {
		lua_pushnil(L);
		return 1;
	}
	const auto &tags = spell->getTags();
	lua_createtable(L, static_cast<int>(tags.size()), 0);
	int index = 1;
	for (const auto &tag : tags) {
		Lua::pushString(L, tag);
		lua_rawseti(L, -2, index++);
	}
	return 1;
}

/***
 * @function Spell:offensiveParameters
 * @param parameters table
 * @return boolean success
 * @return string reason
 */
int SpellFunctions::luaSpellOffensiveParameters(lua_State* L) {
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (!spell || !Lua::isTable(L, 2)) {
		pushBooleanReason(L, false, "wrong_type");
		return 2;
	}
	auto &definition = spell->getOrCreateOffensiveDefinition();
	auto candidate = definition;
	const std::array fields {
		std::pair { "basePower", OffensiveParameterField::BasePower },
		std::pair { "physicalCoefficient", OffensiveParameterField::PhysicalCoefficient },
		std::pair { "magicalCoefficient", OffensiveParameterField::MagicalCoefficient },
		std::pair { "equipmentCoefficient", OffensiveParameterField::EquipmentCoefficient },
		std::pair { "secondaryMultiplier", OffensiveParameterField::SecondaryMultiplier },
		std::pair { "cooldownMilliseconds", OffensiveParameterField::CooldownMilliseconds },
	};
	for (const auto &[name, field] : fields) {
		lua_getfield(L, 2, name);
		OffensiveDefinitionUpdateResult result;
		if (!Lua::isNumber(L, -1)) {
			result = candidate.setParameter(field, std::string {}, spell->getName());
		} else {
			result = candidate.setParameter(field, Lua::getNumber<double>(L, -1), spell->getName());
		}
		lua_pop(L, 1);
		if (result != OffensiveDefinitionUpdateResult::Updated) {
			definition.invalidate();
			pushBooleanReason(L, false, offensiveUpdateReason(result));
			return 2;
		}
	}
	definition = std::move(candidate);
	spell->setCooldown(definition.parameters().cooldownMilliseconds);
	pushBooleanReason(L, true, "updated");
	return 2;
}

/***
 * @function Spell:baseTags
 * @param tags string[]
 * @return boolean success
 * @return string reason
 */
int SpellFunctions::luaSpellBaseTags(lua_State* L) {
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto names = readTagNames(L, 2);
	if (!spell || !names.has_value()) {
		pushBooleanReason(L, false, "wrong_type");
		return 2;
	}
	auto &definition = spell->getOrCreateOffensiveDefinition();
	auto result = buildTags(*names);
	if (!result.success()) {
		definition.invalidate();
		g_logger().error("[SpellFunctions::luaSpellBaseTags] spell={} tag={} reason={}", spell->getName(), result.rejectedTag, spellTagValidationReason(result.result));
		pushBooleanReason(L, false, spellTagValidationReason(result.result));
		return 2;
	}
	const auto update = definition.setBaseTags(std::move(*result.tags));
	pushBooleanReason(L, update == OffensiveDefinitionUpdateResult::Updated, offensiveUpdateReason(update));
	return 2;
}

/***
 * @function Spell:profileTags
 * @param profile string
 * @param tags string[]
 * @return boolean success
 * @return string reason
 */
int SpellFunctions::luaSpellProfileTags(lua_State* L) {
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (!spell || lua_type(L, 2) != LUA_TSTRING) {
		pushBooleanReason(L, false, "wrong_type");
		return 2;
	}
	const auto profileName = Lua::getString(L, 2);
	const auto profile = parseOffensiveProfile(profileName);
	if (!profile.has_value()) {
		g_logger().error("[SpellFunctions::luaSpellProfileTags] spell={} profile={} reason=invalid_profile", spell->getName(), profileName);
		spell->getOrCreateOffensiveDefinition().invalidate();
		pushBooleanReason(L, false, "invalid_profile");
		return 2;
	}
	const auto names = readTagNames(L, 3);
	if (!names.has_value()) {
		pushBooleanReason(L, false, "wrong_type");
		return 2;
	}
	auto &definition = spell->getOrCreateOffensiveDefinition();
	auto result = buildTags(*names);
	if (!result.success()) {
		definition.invalidate();
		g_logger().error("[SpellFunctions::luaSpellProfileTags] spell={} tag={} reason={}", spell->getName(), result.rejectedTag, spellTagValidationReason(result.result));
		pushBooleanReason(L, false, spellTagValidationReason(result.result));
		return 2;
	}
	const auto update = definition.setProfileTags(*profile, std::move(*result.tags));
	pushBooleanReason(L, update == OffensiveDefinitionUpdateResult::Updated, offensiveUpdateReason(update));
	return 2;
}

/***
 * @function Spell:createOffensiveContext
 * @param player Player
 * @return OffensiveCastContext? context
 * @return string reason
 */
int SpellFunctions::luaSpellCreateOffensiveContext(lua_State* L) {
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto &player = Lua::getUserdataShared<Player>(L, 2, "Player");
	if (!spell || !player) {
		lua_pushnil(L);
		Lua::pushString(L, "invalid_player");
		return 2;
	}
	auto result = OffensiveCastContext::create(*spell, *player);
	if (!result.success()) {
		lua_pushnil(L);
		Lua::pushString(L, std::string(offensiveCastContextReason(result.result)));
		return 2;
	}
	Lua::pushSharedUserdata<OffensiveCastContext>(L, std::move(result.context));
	Lua::pushString(L, "created");
	return 2;
}

/***
 * @function OffensiveCastContext:getProfile
 * @return string
 */
int SpellFunctions::luaOffensiveContextProfile(lua_State* L) {
	const auto &context = Lua::getUserdataShared<OffensiveCastContext>(L, 1, "OffensiveCastContext");
	if (!context) {
		lua_pushnil(L);
		return 1;
	}
	Lua::pushString(L, std::string(offensiveProfileName(context->profile())));
	return 1;
}

/***
 * @function OffensiveCastContext:getEquipmentPower
 * @return integer
 */
int SpellFunctions::luaOffensiveContextEquipmentPower(lua_State* L) {
	const auto &context = Lua::getUserdataShared<OffensiveCastContext>(L, 1, "OffensiveCastContext");
	if (!context) {
		lua_pushnil(L);
		return 1;
	}
	Lua::pushNumber(L, context->equipmentPower());
	return 1;
}

/***
 * @function OffensiveCastContext:getRange
 * @return integer
 */
int SpellFunctions::luaOffensiveContextRange(lua_State* L) {
	const auto &context = Lua::getUserdataShared<OffensiveCastContext>(L, 1, "OffensiveCastContext");
	if (!context) {
		lua_pushnil(L);
		return 1;
	}
	Lua::pushNumber(L, context->range());
	return 1;
}

/***
 * @function OffensiveCastContext:requiresAmmunition
 * @return boolean
 */
int SpellFunctions::luaOffensiveContextRequiresAmmunition(lua_State* L) {
	const auto &context = Lua::getUserdataShared<OffensiveCastContext>(L, 1, "OffensiveCastContext");
	if (!context) {
		lua_pushnil(L);
		return 1;
	}
	Lua::pushBoolean(L, context->requiresAmmunition());
	return 1;
}

/***
 * @function OffensiveCastContext:getTags
 * @return string[]
 */
int SpellFunctions::luaOffensiveContextTags(lua_State* L) {
	const auto &context = Lua::getUserdataShared<OffensiveCastContext>(L, 1, "OffensiveCastContext");
	if (!context) {
		lua_pushnil(L);
		return 1;
	}
	const auto names = context->effectiveTags().names();
	lua_createtable(L, static_cast<int>(names.size()), 0);
	int index = 1;
	for (const auto name : names) {
		Lua::pushString(L, std::string(name));
		lua_rawseti(L, -2, index++);
	}
	return 1;
}

/***
 * @function OffensiveCastContext:getPrimaryBaseDamage
 * @return integer
 */
int SpellFunctions::luaOffensiveContextPrimaryBaseDamage(lua_State* L) {
	const auto &context = Lua::getUserdataShared<OffensiveCastContext>(L, 1, "OffensiveCastContext");
	if (!context) {
		lua_pushnil(L);
		return 1;
	}
	Lua::pushNumber(L, context->primaryBaseDamage());
	return 1;
}

/***
 * @function OffensiveCastContext:getSecondaryBaseDamage
 * @return integer
 */
int SpellFunctions::luaOffensiveContextSecondaryBaseDamage(lua_State* L) {
	const auto &context = Lua::getUserdataShared<OffensiveCastContext>(L, 1, "OffensiveCastContext");
	if (!context) {
		lua_pushnil(L);
		return 1;
	}
	Lua::pushNumber(L, context->secondaryBaseDamage());
	return 1;
}

/***
 * @function OffensiveCastContext:validatePrimaryTarget
 * @param target Creature
 * @return boolean success
 * @return string reason
 */
int SpellFunctions::luaOffensiveContextValidatePrimaryTarget(lua_State* L) {
	const auto &context = Lua::getUserdataShared<OffensiveCastContext>(L, 1, "OffensiveCastContext");
	if (!context) {
		pushBooleanReason(L, false, "invalid_context");
		return 2;
	}
	const auto result = context->validatePrimaryTarget(Lua::getCreature(L, 2));
	pushBooleanReason(L, result.success(), offensiveCastContextReason(result.result));
	return 2;
}

/***
 * @function OffensiveCastContext:canAffect
 * @param target Creature
 * @return boolean
 */
int SpellFunctions::luaOffensiveContextCanAffect(lua_State* L) {
	const auto &context = Lua::getUserdataShared<OffensiveCastContext>(L, 1, "OffensiveCastContext");
	Lua::pushBoolean(L, context && context->canAffect(Lua::getCreature(L, 2)));
	return 1;
}

/***
 * @function OffensiveCastContext:commit
 * @param target Creature
 * @return boolean success
 * @return string reason
 */
int SpellFunctions::luaOffensiveContextCommit(lua_State* L) {
	const auto &context = Lua::getUserdataShared<OffensiveCastContext>(L, 1, "OffensiveCastContext");
	if (!context) {
		pushBooleanReason(L, false, "invalid_context");
		return 2;
	}
	const auto result = context->commit(Lua::getCreature(L, 2));
	pushBooleanReason(L, result.success(), offensiveCastContextReason(result.result));
	return 2;
}

// only for InstantSpells
int SpellFunctions::luaSpellWords(lua_State* L) {
	// spell:words(words[, separator = ""])
	const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto &spell = std::static_pointer_cast<InstantSpell>(spellBase);
	if (spell) {
		// if spell != SPELL_INSTANT, it means that this actually is no InstantSpell, so we return nil
		if (spell->spellType != SPELL_INSTANT) {
			lua_pushnil(L);
			return 1;
		}

		if (lua_gettop(L) == 1) {
			Lua::pushString(L, spell->getWords());
			Lua::pushString(L, spell->getSeparator());
			return 2;
		} else {
			std::string sep;
			if (lua_gettop(L) == 3) {
				sep = Lua::getString(L, 3);
			}
			spell->setWords(Lua::getString(L, 2));
			spell->setSeparator(sep);
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// only for InstantSpells
int SpellFunctions::luaSpellNeedDirection(lua_State* L) {
	// spell:needDirection(bool)
	const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto &spell = std::static_pointer_cast<InstantSpell>(spellBase);
	if (spell) {
		// if spell != SPELL_INSTANT, it means that this actually is no InstantSpell, so we return nil
		if (spell->spellType != SPELL_INSTANT) {
			lua_pushnil(L);
			return 1;
		}

		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getNeedDirection());
		} else {
			spell->setNeedDirection(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// only for InstantSpells
int SpellFunctions::luaSpellHasParams(lua_State* L) {
	// spell:hasParams(bool)
	const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto &spell = std::static_pointer_cast<InstantSpell>(spellBase);
	if (spell) {
		// if spell != SPELL_INSTANT, it means that this actually is no InstantSpell, so we return nil
		if (spell->spellType != SPELL_INSTANT) {
			lua_pushnil(L);
			return 1;
		}

		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getHasParam());
		} else {
			spell->setHasParam(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// only for InstantSpells
int SpellFunctions::luaSpellHasPlayerNameParam(lua_State* L) {
	// spell:hasPlayerNameParam(bool)
	const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto &spell = std::static_pointer_cast<InstantSpell>(spellBase);
	if (spell) {
		// if spell != SPELL_INSTANT, it means that this actually is no InstantSpell, so we return nil
		if (spell->spellType != SPELL_INSTANT) {
			lua_pushnil(L);
			return 1;
		}

		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getHasPlayerNameParam());
		} else {
			spell->setHasPlayerNameParam(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// only for InstantSpells
int SpellFunctions::luaSpellNeedCasterTargetOrDirection(lua_State* L) {
	// spell:needCasterTargetOrDirection(bool)
	const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto &spell = std::static_pointer_cast<InstantSpell>(spellBase);
	if (spell) {
		// if spell != SPELL_INSTANT, it means that this actually is no InstantSpell, so we return nil
		if (spell->spellType != SPELL_INSTANT) {
			lua_pushnil(L);
			return 1;
		}

		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getNeedCasterTargetOrDirection());
		} else {
			spell->setNeedCasterTargetOrDirection(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// only for InstantSpells
int SpellFunctions::luaSpellIsBlockingWalls(lua_State* L) {
	// spell:blockWalls(bool)
	const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto &spell = std::static_pointer_cast<InstantSpell>(spellBase);
	if (spell) {
		// if spell != SPELL_INSTANT, it means that this actually is no InstantSpell, so we return nil
		if (spell->spellType != SPELL_INSTANT) {
			lua_pushnil(L);
			return 1;
		}

		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getBlockWalls());
		} else {
			spell->setBlockWalls(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// only for RuneSpells
int SpellFunctions::luaSpellRuneId(lua_State* L) {
	// spell:runeId(id)
	const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto &spell = std::static_pointer_cast<RuneSpell>(spellBase);
	if (spell) {
		// if spell != SPELL_RUNE, it means that this actually is no RuneSpell, so we return nil
		if (spell->spellType != SPELL_RUNE) {
			lua_pushnil(L);
			return 1;
		}

		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, spell->getRuneItemId());
		} else {
			spell->setRuneItemId(Lua::getNumber<uint16_t>(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// only for RuneSpells
int SpellFunctions::luaSpellCharges(lua_State* L) {
	// spell:charges(charges)
	const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto &spell = std::static_pointer_cast<RuneSpell>(spellBase);
	if (spell) {
		// if spell != SPELL_RUNE, it means that this actually is no RuneSpell, so we return nil
		if (spell->spellType != SPELL_RUNE) {
			lua_pushnil(L);
			return 1;
		}

		if (lua_gettop(L) == 1) {
			lua_pushnumber(L, spell->getCharges());
		} else {
			spell->setCharges(Lua::getNumber<uint32_t>(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// only for RuneSpells
int SpellFunctions::luaSpellAllowFarUse(lua_State* L) {
	// spell:allowFarUse(bool)
	const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto &spell = std::static_pointer_cast<RuneSpell>(spellBase);
	if (spell) {
		// if spell != SPELL_RUNE, it means that this actually is no RuneSpell, so we return nil
		if (spell->spellType != SPELL_RUNE) {
			lua_pushnil(L);
			return 1;
		}

		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getAllowFarUse());
		} else {
			spell->setAllowFarUse(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// only for RuneSpells
int SpellFunctions::luaSpellBlockWalls(lua_State* L) {
	// spell:blockWalls(bool)
	const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto &spell = std::static_pointer_cast<RuneSpell>(spellBase);
	if (spell) {
		// if spell != SPELL_RUNE, it means that this actually is no RuneSpell, so we return nil
		if (spell->spellType != SPELL_RUNE) {
			lua_pushnil(L);
			return 1;
		}

		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getCheckLineOfSight());
		} else {
			spell->setCheckLineOfSight(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// only for RuneSpells
int SpellFunctions::luaSpellCheckFloor(lua_State* L) {
	// spell:checkFloor(bool)
	const auto &spellBase = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	const auto &spell = std::static_pointer_cast<RuneSpell>(spellBase);
	if (spell) {
		// if spell != SPELL_RUNE, it means that this actually is no RuneSpell, so we return nil
		if (spell->spellType != SPELL_RUNE) {
			lua_pushnil(L);
			return 1;
		}

		if (lua_gettop(L) == 1) {
			Lua::pushBoolean(L, spell->getCheckFloor());
		} else {
			spell->setCheckFloor(Lua::getBoolean(L, 2));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int SpellFunctions::luaSpellMonkSpellType(lua_State* L) {
	// spell:monkSpellType(type)
	const auto &spell = Lua::getUserdataShared<Spell>(L, 1, "Spell");
	if (spell) {
		if (lua_gettop(L) == 1) {
			Lua::pushNumber(L, static_cast<uint8_t>(spell->getMonkSpellType()));
		} else {
			spell->setMonkSpellType(static_cast<MonkSpell_t>(Lua::getNumber<uint8_t>(L, 2)));
			Lua::pushBoolean(L, true);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}
