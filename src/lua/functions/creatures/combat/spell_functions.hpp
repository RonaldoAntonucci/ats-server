/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once

class SpellFunctions {
public:
	static void init(lua_State* L);

private:
	static int luaSpellCreate(lua_State* L);

	static int luaSpellOnCastSpell(lua_State* L);
	static int luaSpellRegister(lua_State* L);
	static int luaSpellName(lua_State* L);
	static int luaSpellId(lua_State* L);
	static int luaSpellGroup(lua_State* L);
	static int luaSpellCooldown(lua_State* L);
	static int luaSpellGroupCooldown(lua_State* L);
	static int luaSpellLevel(lua_State* L);
	static int luaSpellMagicLevel(lua_State* L);
	static int luaSpellMana(lua_State* L);
	static int luaSpellManaPercent(lua_State* L);
	static int luaSpellSoul(lua_State* L);
	static int luaSpellRange(lua_State* L);
	static int luaSpellPremium(lua_State* L);
	static int luaSpellEnabled(lua_State* L);
	static int luaSpellNeedTarget(lua_State* L);
	static int luaSpellAllowOnSelf(lua_State* L);
	static int luaSpellPzLocked(lua_State* L);
	static int luaSpellNeedWeapon(lua_State* L);
	static int luaSpellNeedLearn(lua_State* L);
	static int luaSpellSelfTarget(lua_State* L);
	static int luaSpellBlocking(lua_State* L);
	static int luaSpellAggressive(lua_State* L);
	static int luaSpellVocation(lua_State* L);
	static int luaSpellDisciplineRequirement(lua_State* L);
	static int luaSpellTag(lua_State* L);
	static int luaSpellHasTag(lua_State* L);
	static int luaSpellGetTags(lua_State* L);
	static int luaSpellOffensiveParameters(lua_State* L);
	static int luaSpellBaseTags(lua_State* L);
	static int luaSpellProfileTags(lua_State* L);
	static int luaSpellCreateOffensiveContext(lua_State* L);

	static int luaOffensiveContextProfile(lua_State* L);
	static int luaOffensiveContextEquipmentPower(lua_State* L);
	static int luaOffensiveContextRange(lua_State* L);
	static int luaOffensiveContextRequiresAmmunition(lua_State* L);
	static int luaOffensiveContextTags(lua_State* L);
	static int luaOffensiveContextPrimaryBaseDamage(lua_State* L);
	static int luaOffensiveContextSecondaryBaseDamage(lua_State* L);
	static int luaOffensiveContextValidatePrimaryTarget(lua_State* L);
	static int luaOffensiveContextCanAffect(lua_State* L);
	static int luaOffensiveContextCommit(lua_State* L);

	static int luaSpellCastSound(lua_State* L);
	static int luaSpellImpactSound(lua_State* L);

	// Only for InstantSpells.
	static int luaSpellWords(lua_State* L);
	static int luaSpellNeedDirection(lua_State* L);
	static int luaSpellHasParams(lua_State* L);
	static int luaSpellHasPlayerNameParam(lua_State* L);
	static int luaSpellNeedCasterTargetOrDirection(lua_State* L);
	static int luaSpellIsBlockingWalls(lua_State* L);

	// Only for RuneSpells.
	static int luaSpellRuneId(lua_State* L);
	static int luaSpellCharges(lua_State* L);
	static int luaSpellAllowFarUse(lua_State* L);
	static int luaSpellBlockWalls(lua_State* L);
	static int luaSpellCheckFloor(lua_State* L);
	static int luaSpellMonkSpellType(lua_State* L);
};
