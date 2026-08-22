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

	class ArmamentoAssaultShieldIntegrationTest : public ::testing::Test {
	protected:
		void SetUp() override {
			L.reset(luaL_newstate());
			ASSERT_NE(nullptr, L);
			luaL_openlibs(L.get());
			ASSERT_EQ(LUA_OK, luaL_dostring(L.get(), R"lua(
				COMBAT_PHYSICALDAMAGE=1; CONST_ME_NONE=0; ORIGIN_SPELL=2
				CONST_ME_DRAWBLOOD=10; CONST_ME_HITAREA=11; CONST_ME_BLOCKHIT=12; CONST_ANI_ARROW=20; CONST_ANI_BOLT=21
				state={events={}}
				function Spell()
					local spell={}
					for _, method in ipairs({"name","words","id","needTarget","isAggressive","mana","soul","cooldown","groupCooldown","disciplineRequirement","offensiveParameters","baseTags","profileTags"}) do spell[method]=function() end end
					spell.createOffensiveContext=function() table.insert(state.events,"create"); return state.context,"created" end
					spell.register=function() end; state.spell=spell; return spell
				end
				function Position(x,y,z) return {x=x,y=y,z=z} end
				function Tile(position) table.insert(state.events,"tile:"..position.x..","..position.y); return state.tile end
				function Creature() return state.target end
				function doTargetCombatHealth(_,target,_,minimum,maximum,effect)
					table.insert(state.events,"damage"); assert(target==state.target and minimum==-60 and maximum==-60 and effect==CONST_ME_BLOCKHIT); return true
				end
				state.player={getPosition=function() return Position(10,10,7) end}
				state.tile={}
				state.target={getId=function() return 77 end,getPosition=function() return Position(11,10,7) end,
					isRemoved=function() table.insert(state.events,"removed-check"); return false end,
					getHealth=function() table.insert(state.events,"health-check"); return 40 end,
					move=function(_,tile,flags) table.insert(state.events,"move:"..flags); assert(tile==state.tile); return 0 end}
				state.context={getProfile=function() return "shield" end,getPrimaryBaseDamage=function() return 60 end,
					validatePrimaryTarget=function() table.insert(state.events,"validate"); return true,"created" end,
					commit=function() table.insert(state.events,"commit"); return true,"created" end}
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

TEST_F(ArmamentoAssaultShieldIntegrationTest, ProductionShieldDamagesBeforeOneNormalMove) {
	run(R"lua(
		assert(state.spell.onCastSpell(state.player,{getNumber=function() return 77 end}) == true)
		assert(table.concat(state.events,",") == "create,validate,commit,damage,removed-check,health-check,tile:12,10,move:0")
	)lua");
}
