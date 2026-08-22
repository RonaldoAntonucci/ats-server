/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "lua/scripts/luascript.hpp"

#include <gtest/gtest.h>

namespace {
	std::filesystem::path repositoryRoot() {
		const std::filesystem::path source { __FILE__ };
		return source.parent_path().parent_path().parent_path().parent_path();
	}

	class ArmamentoAssaultScriptIntegrationTest : public ::testing::Test {
	protected:
		void SetUp() override {
			L.reset(luaL_newstate());
			ASSERT_NE(nullptr, L);
			luaL_openlibs(L.get());
			ASSERT_EQ(LUA_OK, luaL_dostring(L.get(), R"lua(
				COMBAT_PHYSICALDAMAGE = 1
				COMBAT_PARAM_TYPE = 2
				COMBAT_PARAM_EFFECT = 3
				COMBAT_PARAM_BLOCKARMOR = 4
				COMBAT_PARAM_BLOCKSHIELD = 5
				COMBAT_PARAM_DISTANCEEFFECT = 6
				CALLBACK_PARAM_LEVELMAGICVALUE = 7
				CONST_ME_DRAWBLOOD = 10
				CONST_ME_HITAREA = 11
				CONST_ME_BLOCKHIT = 12
				CONST_ANI_ARROW = 20
				CONST_ANI_BOLT = 21
				WEAPON_SWORD = 1
				WEAPON_DISTANCE = 2
				WEAPON_AXE = 3
				AMMO_NONE = 0
				AMMO_ARROW = 1
				AMMO_BOLT = 2
				CONST_SLOT_LEFT = 5
				CONST_SLOT_RIGHT = 6
				state = { combats = {}, itemTypes = {}, targets = {}, logs = {}, tags = {} }
				logger = { error = function(message, value) table.insert(state.logs, message .. "|" .. tostring(value)) end }
				function Combat()
					local combat = { parameters = {} }
					function combat:setParameter(parameter, value) self.parameters[parameter] = value end
					function combat:setCallback(parameter, callback) self.callbackParameter, self.callback = parameter, callback end
					function combat:execute(player, variant)
						if state.combatError then error(state.combatError) end
						local minimum, maximum = _G[self.callback](player, 999999, 999999)
						state.execution = { combat = self, player = player, variant = variant, minimum = minimum, maximum = maximum }
						return state.combatResult ~= false
					end
					table.insert(state.combats, combat)
					return combat
				end
				function Spell(kind)
					state.kind = kind
					local spell = {}
					for _, method in ipairs({"name", "words", "id", "needTarget", "isAggressive", "blockWalls", "mana", "soul", "cooldown", "groupCooldown", "disciplineRequirement"}) do
						spell[method] = function(_, ...) state[method] = {...}; return true end
					end
					spell.tag = function(_, tag) table.insert(state.tags, tag); return true end
					spell.register = function() state.registered = true; return true end
					state.spell = spell
					return spell
				end
				function ItemType(id) return state.itemTypes[id] end
				function Creature(id) return state.targets[id] end
				function position(x, y, z)
					local value = {x = x, y = y, z = z or 7}
					function value:getDistance(other)
						return math.max(math.abs(self.x - other.x), math.abs(self.y - other.y), math.abs(self.z - other.z))
					end
					return value
				end
				function weapon(id, weaponType, ammoType, attack, range)
					state.itemTypes[id] = {
						getWeaponType = function() return weaponType end,
						getAmmoType = function() return ammoType end,
						getShootRange = function() return range end,
					}
					return { getId = function() return id end, getAttack = function() return attack end }
				end
				function castWith(left, right, targetX)
					state.execution = nil
					state.targets[77] = { getPosition = function() return position(targetX or 1, 0, 7) end }
					local player = {
						getId = function() return 42 end,
						getStatPhysicalAttack = function() return 20 end,
						getStatMagicalAttack = function() return 999 end,
						getPosition = function() return position(0, 0, 7) end,
						getSlotItem = function(_, slot) return slot == CONST_SLOT_LEFT and left or right end,
					}
					local variant = { getNumber = function() return 77 end }
					return state.spell.onCastSpell(player, variant), player
				end
			)lua"))
				<< lua_tostring(L.get(), -1);

			const auto script = repositoryRoot() / "data/scripts/spells/attack/armamento_assault.lua";
			ASSERT_EQ(LUA_OK, luaL_dofile(L.get(), script.c_str())) << lua_tostring(L.get(), -1);
		}

		void run(std::string_view script) {
			ASSERT_EQ(LUA_OK, luaL_dostring(L.get(), std::string { script }.c_str())) << lua_tostring(L.get(), -1);
		}

		std::unique_ptr<lua_State, decltype(&lua_close)> L { nullptr, &lua_close };
	};
} // namespace

TEST_F(ArmamentoAssaultScriptIntegrationTest, LoadsThroughInstantSpellAndConfiguresLegacyCombats) {
	run(R"lua(
		assert(state.kind == "instant")
		assert(state.id[1] == 298 and state.name[1] == "Assault" and state.words[1] == "assault")
		assert(state.needTarget[1] and state.isAggressive[1] and state.blockWalls[1])
		assert(state.cooldown[1] == 1000 and state.groupCooldown[1] == 0)
		assert(state.disciplineRequirement[1] == 1 and state.disciplineRequirement[2] == 1)
		assert(#state.tags == 6 and state.registered == true)
		assert(#state.combats == 3)
		for _, combat in ipairs(state.combats) do
			assert(combat.parameters[COMBAT_PARAM_TYPE] == COMBAT_PHYSICALDAMAGE)
			assert(combat.parameters[COMBAT_PARAM_BLOCKARMOR] == 1)
			assert(combat.parameters[COMBAT_PARAM_BLOCKSHIELD] == 0)
		end
	)lua");
}

TEST_F(ArmamentoAssaultScriptIntegrationTest, UnsupportedEquipmentStopsBeforeCombat) {
	run(R"lua(
		local axe = weapon(100, WEAPON_AXE, AMMO_NONE, 90, 1)
		assert(castWith(axe, nil, 1) == false)
		assert(state.execution == nil)
	)lua");
}

TEST_F(ArmamentoAssaultScriptIntegrationTest, SwordFormulaUsesCurrentAttackAndAtsStats) {
	run(R"lua(
		local sword = weapon(101, WEAPON_SWORD, AMMO_NONE, 77, 1)
		assert(castWith(sword, nil, 1) == true)
		assert(state.execution.minimum == -107 and state.execution.maximum == -107)
		assert(state.execution.combat.parameters[COMBAT_PARAM_EFFECT] == CONST_ME_DRAWBLOOD)
	)lua");
}

TEST_F(ArmamentoAssaultScriptIntegrationTest, BowUsesWeaponRangeWithoutAmmunitionLookup) {
	run(R"lua(
		local bow = weapon(102, WEAPON_DISTANCE, AMMO_ARROW, 30, 6)
		assert(castWith(nil, bow, 6) == true)
		assert(state.execution.minimum == -60 and state.execution.maximum == -60)
		assert(state.execution.combat.parameters[COMBAT_PARAM_DISTANCEEFFECT] == CONST_ANI_ARROW)
		assert(castWith(nil, bow, 7) == false)
	)lua");
}

TEST_F(ArmamentoAssaultScriptIntegrationTest, CombatFailureClearsTheEphemeralFormulaBridge) {
	run(R"lua(
		state.combatError = "synthetic failure"
		local sword = weapon(103, WEAPON_SWORD, AMMO_NONE, 30, 1)
		local result, player = castWith(sword, nil, 1)
		assert(result == false and #state.logs == 1)
		local minimum, maximum = onGetArmamentoAssaultPrimaryValues(player, 1, 1)
		assert(minimum == 0 and maximum == 0)
	)lua");
}
