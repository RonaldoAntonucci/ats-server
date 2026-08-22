/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/spells.hpp"
#include "lua/functions/creatures/combat/spell_functions.hpp"
#include "lua/functions/lua_functions_loader.hpp"

#include <gtest/gtest.h>

namespace {
	class SpellTagBindingsTest : public ::testing::Test {
	protected:
		void SetUp() override {
			L.reset(luaL_newstate());
			SpellFunctions::init(L.get());
			spell = std::make_shared<InstantSpell>();
		}

		void pushMethod(const char* method) {
			lua_getglobal(L.get(), "Spell");
			ASSERT_TRUE(lua_istable(L.get(), -1));
			lua_getfield(L.get(), -1, method);
			lua_remove(L.get(), -2);
			ASSERT_TRUE(lua_isfunction(L.get(), -1)) << method;
			Lua::pushSharedUserdata<Spell>(L.get(), spell);
		}

		bool callBooleanMethod(const char* method, std::string_view value) {
			pushMethod(method);
			Lua::pushString(L.get(), std::string(value));
			EXPECT_EQ(LUA_OK, lua_pcall(L.get(), 2, 1, 0)) << lua_tostring(L.get(), -1);
			EXPECT_TRUE(lua_isboolean(L.get(), -1));
			const bool result = lua_toboolean(L.get(), -1) != 0;
			lua_pop(L.get(), 1);
			return result;
		}

		std::vector<std::string> callGetTags() {
			pushMethod("getTags");
			EXPECT_EQ(LUA_OK, lua_pcall(L.get(), 1, 1, 0)) << lua_tostring(L.get(), -1);
			EXPECT_TRUE(lua_istable(L.get(), -1));
			std::vector<std::string> result;
			const auto count = lua_objlen(L.get(), -1);
			result.reserve(count);
			for (size_t index = 1; index <= count; ++index) {
				lua_rawgeti(L.get(), -1, static_cast<int>(index));
				result.emplace_back(Lua::getString(L.get(), -1));
				lua_pop(L.get(), 1);
			}
			lua_pop(L.get(), 1);
			return result;
		}

		std::unique_ptr<lua_State, decltype(&lua_close)> L { nullptr, &lua_close };
		std::shared_ptr<InstantSpell> spell;
	};
}

TEST_F(SpellTagBindingsTest, RegistersTheFreeFormTagSurface) {
	lua_getglobal(L.get(), "Spell");
	ASSERT_TRUE(lua_istable(L.get(), -1));
	for (const auto method : { "tag", "hasTag", "getTags" }) {
		lua_getfield(L.get(), -1, method);
		EXPECT_TRUE(lua_isfunction(L.get(), -1)) << method;
		lua_pop(L.get(), 1);
	}
	lua_pop(L.get(), 1);
}

TEST_F(SpellTagBindingsTest, TagReturnsTrueForUnknownAndDuplicateValues) {
	EXPECT_TRUE(callBooleanMethod("tag", "future.consumer.custom"));
	EXPECT_TRUE(callBooleanMethod("tag", "future.consumer.custom"));
	EXPECT_EQ((std::vector<std::string> { "future.consumer.custom" }), callGetTags());
}

TEST_F(SpellTagBindingsTest, TagReturnsFalseForEmptyWithoutMutation) {
	ASSERT_TRUE(callBooleanMethod("tag", "category.art"));
	EXPECT_FALSE(callBooleanMethod("tag", ""));
	EXPECT_EQ((std::vector<std::string> { "category.art" }), callGetTags());
}

TEST_F(SpellTagBindingsTest, HasTagIsCaseSensitive) {
	ASSERT_TRUE(callBooleanMethod("tag", "damage.physical"));
	EXPECT_TRUE(callBooleanMethod("hasTag", "damage.physical"));
	EXPECT_FALSE(callBooleanMethod("hasTag", "Damage.Physical"));
}

TEST_F(SpellTagBindingsTest, GetTagsPreservesInsertionOrderAndCaseVariants) {
	ASSERT_TRUE(callBooleanMethod("tag", "function.offensive"));
	ASSERT_TRUE(callBooleanMethod("tag", "Function.Offensive"));
	ASSERT_TRUE(callBooleanMethod("tag", "category.art"));
	EXPECT_EQ((std::vector<std::string> { "function.offensive", "Function.Offensive", "category.art" }), callGetTags());
}
