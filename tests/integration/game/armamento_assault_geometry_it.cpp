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
	std::filesystem::path assaultRepositoryRoot() {
		const std::filesystem::path source { __FILE__ };
		return source.parent_path().parent_path().parent_path().parent_path();
	}

	class ArmamentoAssaultGeometryIntegrationTest : public ::testing::Test {
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
				WEAPON_CLUB = 4
				AMMO_NONE = 0
				AMMO_ARROW = 1
				AMMO_BOLT = 2
				CONST_SLOT_LEFT = 5
				CONST_SLOT_RIGHT = 6
				state = {areas = {}, combats = {}, itemTypes = {}, executions = {}, targets = {}}
				logger = {error = function() end}
				function createCombatArea(cardinal, diagonal)
					local area = {cardinal = cardinal, diagonal = diagonal}
					table.insert(state.areas, area)
					return area
				end
				function Combat()
					local combat = {parameters = {}}
					function combat:setParameter(parameter, value) self.parameters[parameter] = value end
					function combat:setCallback(_, callback) self.callback = callback end
					function combat:setArea(area) self.area = area end
					function combat:execute(player, variant)
						local minimum, maximum = _G[self.callback](player, 999999, 999999)
						table.insert(state.executions, {area = self.area, minimum = minimum, maximum = maximum})
						return true
					end
					table.insert(state.combats, combat)
					return combat
				end
				function Spell()
					local spell = {}
					for _, method in ipairs({"name", "words", "id", "needTarget", "isAggressive", "blockWalls", "mana", "soul", "cooldown", "groupCooldown", "disciplineRequirement"}) do
						spell[method] = function() return true end
					end
					spell.tag = function() return true end
					spell.register = function() return true end
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
				function castProfile(weaponType, targetX, targetY)
					state.executions = {}
					state.itemTypes[200] = {
						getWeaponType = function() return weaponType end,
						getAmmoType = function() return AMMO_NONE end,
						getShootRange = function() return 1 end,
					}
					local item = {getId = function() return 200 end, getAttack = function() return 30 end}
					state.targets[1] = {getPosition = function() return position(targetX, targetY, 7) end}
					local player = {
						getId = function() return 42 end,
						getStatPhysicalAttack = function() return 20 end,
						getStatMagicalAttack = function() return 0 end,
						getPosition = function() return position(0, 0, 7) end,
						getSlotItem = function(_, slot) return slot == CONST_SLOT_LEFT and item or nil end,
					}
					return state.spell.onCastSpell(player, {getNumber = function() return 1 end})
				end
			)lua"))
				<< lua_tostring(L.get(), -1);
			const auto script = assaultRepositoryRoot() / "data/scripts/spells/attack/armamento_assault.lua";
			ASSERT_EQ(LUA_OK, luaL_dofile(L.get(), script.c_str())) << lua_tostring(L.get(), -1);
		}

		void run(std::string_view script) {
			ASSERT_EQ(LUA_OK, luaL_dostring(L.get(), std::string { script }.c_str())) << lua_tostring(L.get(), -1);
		}

		std::unique_ptr<lua_State, decltype(&lua_close)> L { nullptr, &lua_close };
	};
} // namespace

TEST_F(ArmamentoAssaultGeometryIntegrationTest, ProductionAreasExcludePrimaryCenter) {
	run(R"lua(
		assert(#state.areas == 2)
		assert(state.areas[1].cardinal[1][1] == 1)
		assert(state.areas[1].cardinal[1][2] == 2)
		assert(state.areas[1].cardinal[1][3] == 1)
		assert(state.areas[2].cardinal[2][1] == 1)
		assert(state.areas[2].cardinal[2][2] == 2)
		assert(state.areas[2].cardinal[2][3] == 1)
	)lua");
}

TEST_F(ArmamentoAssaultGeometryIntegrationTest, AxeExecutesPrimaryThenHalfDamageArea) {
	run(R"lua(
		assert(castProfile(WEAPON_AXE, 1, 0) == true)
		assert(#state.executions == 2)
		assert(state.executions[1].area == nil)
		assert(state.executions[1].minimum == -60 and state.executions[1].maximum == -60)
		assert(state.executions[2].area == state.areas[1])
		assert(state.executions[2].minimum == -30 and state.executions[2].maximum == -30)
	)lua");
}

TEST_F(ArmamentoAssaultGeometryIntegrationTest, ClubExecutesPrimaryThenOrthogonalArea) {
	run(R"lua(
		assert(castProfile(WEAPON_CLUB, 1, 1) == true)
		assert(#state.executions == 2)
		assert(state.executions[2].area == state.areas[2])
		assert(state.executions[2].minimum == -30 and state.executions[2].maximum == -30)
	)lua");
}
