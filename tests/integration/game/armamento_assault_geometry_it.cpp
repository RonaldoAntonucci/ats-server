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
				COMBAT_PHYSICALDAMAGE=1; CONST_ME_NONE=0; ORIGIN_SPELL=2
				CONST_ME_DRAWBLOOD=10; CONST_ME_HITAREA=11; CONST_ANI_ARROW=20; CONST_ANI_BOLT=21
				state={events={}, tiles={}, creatures={}, damage={}}
				function Spell()
					local spell={}
					for _, method in ipairs({"name","words","id","needTarget","isAggressive","mana","soul","cooldown","groupCooldown","disciplineRequirement","offensiveParameters","baseTags","profileTags"}) do spell[method]=function() end end
					spell.createOffensiveContext=function() table.insert(state.events,"create"); return state.context,"created" end
					spell.register=function() end; state.spell=spell; return spell
				end
				function Position(x,y,z) return {x=x,y=y,z=z} end
				local function key(p) return p.x..","..p.y..","..p.z end
				function Tile(p) return state.tiles[key(p)] end
				function Creature(id) return state.creatures[id] end
				function doTargetCombatHealth(_,target,_,minimum,maximum,effect)
					table.insert(state.events,"damage:"..target:getId()); table.insert(state.damage,{target:getId(),minimum,maximum,effect}); return true
				end
				function makeCreature(id,x,y)
					local position=Position(x,y,7)
					return {getId=function() return id end,getPosition=function() return position end}
				end
				function setup(profile,x,y)
					state.events={}; state.tiles={}; state.creatures={}; state.damage={}
					local playerPosition=Position(0,0,7)
					playerPosition.sendDistanceEffect=function(_,_,effect) table.insert(state.events,"distance:"..effect) end
					state.player={getPosition=function() return playerPosition end}; state.primary=makeCreature(1,x,y); state.creatures[1]=state.primary
					state.context={getProfile=function() return profile end,getPrimaryBaseDamage=function() return 60 end,getSecondaryBaseDamage=function() return 30 end,
						validatePrimaryTarget=function() table.insert(state.events,"validate"); return true,"created" end,
						canAffect=function(_,target) table.insert(state.events,"affect:"..target:getId()); return true end,
						commit=function() table.insert(state.events,"commit"); return true,"created" end}
				end
				function place(id,x,y)
					local target=makeCreature(id,x,y); state.creatures[id]=target; state.tiles[x..","..y..",7"]={getTopCreature=function() return target end}
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

TEST_F(ArmamentoAssaultGeometryIntegrationTest, AxeProductionFlowOrdersPrimaryAndTwoSecondaryHits) {
	run(R"lua(
		setup("axe",1,0); place(2,1,1); place(3,1,-1)
		assert(state.spell.onCastSpell(state.player,{getNumber=function() return 1 end}) == true)
		assert(table.concat(state.events,",") == "create,validate,affect:2,affect:3,commit,damage:1,damage:2,damage:3")
		assert(state.damage[1][2] == -60 and state.damage[2][2] == -30 and state.damage[3][2] == -30)
	)lua");
}

TEST_F(ArmamentoAssaultGeometryIntegrationTest, BowProductionFlowEmitsArrowBeforeOnePrimaryHit) {
	run(R"lua(
		setup("bow",4,0)
		assert(state.spell.onCastSpell(state.player,{getNumber=function() return 1 end}) == true)
		assert(table.concat(state.events,",") == "create,validate,commit,distance:20,damage:1")
		assert(#state.damage == 1 and state.damage[1][4] == CONST_ME_HITAREA)
	)lua");
}
