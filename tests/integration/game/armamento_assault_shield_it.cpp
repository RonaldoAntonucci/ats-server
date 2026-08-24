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
				COMBAT_PHYSICALDAMAGE=1; COMBAT_PARAM_TYPE=2; COMBAT_PARAM_EFFECT=3; COMBAT_PARAM_BLOCKARMOR=4; COMBAT_PARAM_BLOCKSHIELD=5; COMBAT_PARAM_DISTANCEEFFECT=6; CALLBACK_PARAM_LEVELMAGICVALUE=7
				CONST_ME_DRAWBLOOD=10; CONST_ME_HITAREA=11; CONST_ME_BLOCKHIT=12; CONST_ANI_ARROW=20; CONST_ANI_BOLT=21
				WEAPON_NONE=0; WEAPON_SWORD=1; WEAPON_DISTANCE=2; WEAPON_AXE=3; WEAPON_CLUB=4; WEAPON_SHIELD=5
				AMMO_NONE=0; AMMO_ARROW=1; AMMO_BOLT=2; CONST_SLOT_LEFT=5; CONST_SLOT_RIGHT=6
				state={events={},itemTypes={}}
				logger={error=function() end}
				function createCombatArea(cardinal,diagonal) return {cardinal=cardinal,diagonal=diagonal} end
				function Combat()
					local combat={parameters={}}
					function combat:setParameter(parameter,value) self.parameters[parameter]=value end
					function combat:setCallback(_,callback)
						self.callbackName=callback
						self.callback=_G[callback]
						_G[callback]=nil
						return self.callback~=nil
					end
					function combat:setArea(area) self.area=area end
					function combat:execute(player,variant)
						table.insert(state.events,"damage")
						local minimum,maximum=self.callback(player,999999,999999)
						state.damage={combat=self,minimum=minimum,maximum=maximum,variant=variant}
						state.target.health=40
						return true
					end
					return combat
				end
				function Spell()
					local spell={}
					for _,method in ipairs({"name","words","id","needTarget","isAggressive","blockWalls","mana","soul","cooldown","groupCooldown","disciplineRequirement"}) do spell[method]=function() return true end end
					spell.tag=function() return true end; spell.register=function() return true end; state.spell=spell; return spell
				end
				function ItemType(id) return state.itemTypes[id] end
				function Position(x,y,z)
					local position={x=x,y=y,z=z}
					function position:getDistance(other) return math.max(math.abs(self.x-other.x),math.abs(self.y-other.y),math.abs(self.z-other.z)) end
					return position
				end
				function Tile(position) table.insert(state.events,"tile:"..position.x..","..position.y); return state.tile end
				function Creature() return state.target end
				function item(id,weaponType,attack,defense)
					state.itemTypes[id]={getWeaponType=function() return weaponType end,getAmmoType=function() return AMMO_NONE end,getShootRange=function() return 1 end}
					return {getId=function() return id end,getAttack=function() state.attackReads=state.attackReads+1; return attack end,getDefense=function() state.defenseReads=state.defenseReads+1; return defense end}
				end
				function reset(left,right)
					state.events={}; state.damage=nil; state.move=nil; state.attackReads=0; state.defenseReads=0; state.tile={position=Position(12,10,7)}
					state.target={health=100,getId=function() return 77 end,getPosition=function() return Position(11,10,7) end,isRemoved=function() table.insert(state.events,"removed-check"); return false end,getHealth=function(self) table.insert(state.events,"health-check"); return self.health end,move=function(_,tile,flags) table.insert(state.events,"move:"..flags); state.move=tile; return 0 end}
					state.player={getId=function() return 42 end,getStatPhysicalAttack=function() return 20 end,getStatMagicalAttack=function() return 999 end,getPosition=function() return Position(10,10,7) end,getSlotItem=function(_,slot) return slot==CONST_SLOT_LEFT and left or right end}
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

TEST_F(ArmamentoAssaultShieldIntegrationTest, ProductionShieldUsesDefenseThenMovesOneTile) {
	run(R"lua(
		local shield=item(100,WEAPON_SHIELD,500,77)
		reset(shield,nil)
		assert(state.spell.onCastSpell(state.player,{getNumber=function() return 77 end}) == true)
		assert(state.damage.minimum == -107 and state.damage.maximum == -107)
		assert(state.damage.combat.parameters[COMBAT_PARAM_EFFECT] == CONST_ME_BLOCKHIT)
		assert(state.attackReads == 0 and state.defenseReads == 1)
		assert(table.concat(state.events,",") == "damage,removed-check,health-check,tile:12,10,move:0")
		assert(state.move == state.tile)
	)lua");
}

TEST_F(ArmamentoAssaultShieldIntegrationTest, OffensiveWeaponOverridesShieldFallback) {
	run(R"lua(
		local shield=item(101,WEAPON_SHIELD,500,77)
		local sword=item(102,WEAPON_SWORD,30,500)
		reset(shield,sword)
		assert(state.spell.onCastSpell(state.player,{getNumber=function() return 77 end}) == true)
		assert(state.damage.minimum == -60 and state.damage.combat.parameters[COMBAT_PARAM_EFFECT] == CONST_ME_DRAWBLOOD)
		assert(state.attackReads == 1 and state.defenseReads == 0 and state.move == nil)
		assert(table.concat(state.events,",") == "damage")
	)lua");
}
