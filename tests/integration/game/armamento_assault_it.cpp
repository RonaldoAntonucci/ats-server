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
				CONST_ME_NONE = 0
				ORIGIN_SPELL = 2
				CONST_ME_DRAWBLOOD = 10
				CONST_ME_HITAREA = 11
				CONST_ANI_ARROW = 20
				CONST_ANI_BOLT = 21
				state = { calls = {}, profileTags = {} }
				function Spell(kind)
					state.kind = kind
					local spell = {}
					for _, method in ipairs({"name", "words", "id", "needTarget", "isAggressive", "mana", "soul", "cooldown", "groupCooldown", "disciplineRequirement", "offensiveParameters", "baseTags"}) do
						spell[method] = function(_, ...)
							state[method] = {...}
						end
					end
					spell.profileTags = function(_, profile, tags)
						table.insert(state.profileTags, {profile, tags})
					end
					spell.createOffensiveContext = function(_, player)
						table.insert(state.calls, "create")
						return state.context, state.contextReason
					end
					spell.register = function()
						state.registered = true
					end
					state.spell = spell
					return spell
				end
				function Creature(id)
					return state.targets and state.targets[id] or nil
				end
				function doTargetCombatHealth(...)
					table.insert(state.calls, "combat")
					state.damage = {...}
					return state.combatResult
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

TEST_F(ArmamentoAssaultScriptIntegrationTest, LoadsTheProductionScriptIntoTheLegacyInstantSpellBoundary) {
	run(R"lua(
		assert(state.kind == "instant")
		assert(state.id[1] == 298)
		assert(state.name[1] == "Assault")
		assert(state.words[1] == "assault")
		assert(state.cooldown[1] == 1000)
		assert(state.groupCooldown[1] == 0)
		assert(state.disciplineRequirement[1] == 1 and state.disciplineRequirement[2] == 1)
		assert(state.registered == true)
	)lua");
}

TEST_F(ArmamentoAssaultScriptIntegrationTest, AValidationFailureNeverCommitsOrSubmitsCombat) {
	run(R"lua(
		local target = {}
		state.targets = {[77] = target}
		state.context = {
			getProfile = function() return "sword" end,
			validatePrimaryTarget = function()
				table.insert(state.calls, "validate")
				return false, "combat_denied"
			end,
			commit = function()
				error("commit must not run")
			end,
		}
		local result = state.spell.onCastSpell({}, {getNumber = function() return 77 end})
		assert(result == false)
		assert(table.concat(state.calls, ",") == "create,validate")
		assert(state.damage == nil)
	)lua");
}

TEST_F(ArmamentoAssaultScriptIntegrationTest, AValidCastCommitsBeforeSubmittingExactBaseDamage) {
	run(R"lua(
		local player, target = {}, {}
		state.targets = {[77] = target}
		state.context = {
			getProfile = function() return "sword" end,
			validatePrimaryTarget = function()
				table.insert(state.calls, "validate")
				return true, "created"
			end,
			commit = function()
				table.insert(state.calls, "commit")
				return true, "created"
			end,
			getPrimaryBaseDamage = function()
				table.insert(state.calls, "damage")
				return 60
			end,
		}
		assert(state.spell.onCastSpell(player, {getNumber = function() return 77 end}) == true)
		assert(table.concat(state.calls, ",") == "create,validate,commit,damage,combat")
		assert(state.damage[1] == player and state.damage[2] == target)
		assert(state.damage[3] == COMBAT_PHYSICALDAMAGE)
		assert(state.damage[4] == -60 and state.damage[5] == -60)
		assert(state.damage[6] == CONST_ME_DRAWBLOOD and state.damage[7] == ORIGIN_SPELL)
		assert(state.damage[8] == nil and state.damage[9] == "Assault")
	)lua");
}

TEST_F(ArmamentoAssaultScriptIntegrationTest, ACommittedImmuneResolutionStillCompletesTheLegacyCast) {
	run(R"lua(
		state.combatResult = false
		state.targets = {[77] = {}}
		state.context = {
			getProfile = function() return "sword" end,
			validatePrimaryTarget = function() return true, "created" end,
			commit = function() return true, "created" end,
			getPrimaryBaseDamage = function() return 0 end,
		}
		assert(state.spell.onCastSpell({}, {getNumber = function() return 77 end}) == true)
		assert(state.damage[4] == 0 and state.damage[5] == 0)
	)lua");
}
