/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/spells.hpp"
#include "creatures/players/disciplines/discipline.hpp"
#include "lua/functions/creatures/combat/spell_functions.hpp"
#include "lua/functions/lua_functions_loader.hpp"

#include <gtest/gtest.h>

#include "lib/logging/in_memory_logger.hpp"

namespace {
	class SpellTagBindingsTest : public ::testing::Test {
	protected:
		static void SetUpTestSuite() {
			previousTestContainer = DI::getTestContainer();
			InMemoryLogger::install(injector);
			DI::setTestContainer(&injector);

			const auto catalogFile = std::filesystem::temp_directory_path() / "canary-spell-tag-bindings.xml";
			std::ofstream catalog(catalogFile);
			ASSERT_TRUE(catalog.is_open());
			catalog << R"xml(<disciplines><discipline id="1" name="Armamento"/></disciplines>)xml";
			catalog.close();
			ASSERT_TRUE(g_disciplines().loadFromXml(catalogFile));
			std::error_code error;
			std::filesystem::remove(catalogFile, error);
		}

		static void TearDownTestSuite() {
			DI::setTestContainer(previousTestContainer);
		}

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

		bool methodExists(const char* method) {
			lua_getglobal(L.get(), "Spell");
			lua_getfield(L.get(), -1, method);
			const auto exists = lua_isfunction(L.get(), -1);
			lua_pop(L.get(), 2);
			return exists;
		}

		bool globalExists(const char* name) {
			lua_getglobal(L.get(), name);
			const auto exists = !lua_isnil(L.get(), -1);
			lua_pop(L.get(), 1);
			return exists;
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

		inline static di::extension::injector<> injector {};
		inline static di::extension::injector<>* previousTestContainer = nullptr;
	};
}

TEST_F(SpellTagBindingsTest, RegistersTheFreeFormTagSurface) {
	for (const auto method : { "disciplineRequirement", "tag", "hasTag", "getTags" }) {
		EXPECT_TRUE(methodExists(method)) << method;
	}
}

TEST_F(SpellTagBindingsTest, DoesNotRegisterTheObsoleteOffensiveSurface) {
	for (const auto method : { "offensiveParameters", "baseTags", "profileTags", "profileEffects", "createOffensiveContext" }) {
		EXPECT_FALSE(methodExists(method)) << method;
	}
	EXPECT_FALSE(globalExists("OffensiveCastContext"));
}

TEST_F(SpellTagBindingsTest, DisciplineRequirementStillMutatesTheGenericRequirementSet) {
	pushMethod("disciplineRequirement");
	lua_pushinteger(L.get(), 1);
	lua_pushinteger(L.get(), 2);
	ASSERT_EQ(LUA_OK, lua_pcall(L.get(), 3, 2, 0)) << lua_tostring(L.get(), -1);
	ASSERT_TRUE(lua_isboolean(L.get(), -2));
	ASSERT_TRUE(lua_toboolean(L.get(), -2));
	ASSERT_TRUE(lua_isstring(L.get(), -1));
	EXPECT_EQ("added", Lua::getString(L.get(), -1));
	lua_pop(L.get(), 2);

	ASSERT_EQ(1u, spell->getRequirements().disciplines().size());
	EXPECT_EQ((DisciplineRequirement { .disciplineId = 1, .minimumRank = 2 }), spell->getRequirements().disciplines().front());
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
