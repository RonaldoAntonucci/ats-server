/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/disciplines/discipline.hpp"
#include "creatures/players/player.hpp"
#include "kv/value_wrapper.hpp"
#include "lua/creature/talkaction.hpp"
#include "lua/functions/creatures/creature_functions.hpp"
#include "lua/functions/lua_functions_loader.hpp"
#include "enums/account_group_type.hpp"
#include "enums/account_type.hpp"

#include <gtest/gtest.h>

#include "kv/in_memory_kv.hpp"
#include "lib/logging/in_memory_logger.hpp"

namespace {
	struct MutationResult {
		bool success = false;
		uint32_t before = 0;
		uint32_t after = 0;
		std::string result;
	};

	class DisciplineLuaBindingsTest : public ::testing::Test {
	protected:
		static void SetUpTestSuite() {
			previousTestContainer = DI::getTestContainer();
			InMemoryLogger::install(injector);
			KVMemory::install(injector);
			DI::setTestContainer(&injector);
		}

		static void TearDownTestSuite() {
			DI::setTestContainer(previousTestContainer);
		}

		void SetUp() override {
			dynamic_cast<KVMemory &>(injector.create<KVStore &>()).reset();
			dynamic_cast<InMemoryLogger &>(injector.create<Logger &>()).reset();
			player = std::make_shared<Player>();
			loadCatalog();
			L.reset(luaL_newstate());
			CreatureFunctions::init(L.get());
		}

		void loadCatalog(std::string_view xml = R"xml(<disciplines>
			<discipline id="1" name="Armamento"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="1"/><attribute id="vig" perLevel="1"/><attribute id="sin" perLevel="0"/><attribute id="esp" perLevel="0"/></discipline>
		</disciplines>)xml") {
			const auto file = std::filesystem::temp_directory_path() / "canary-discipline-lua-bindings.xml";
			std::ofstream output(file);
			output << xml;
			output.close();
			ASSERT_TRUE(g_disciplines().loadFromXml(file));
			std::error_code error;
			std::filesystem::remove(file, error);
		}

		void pushMethod(const char* method) {
			lua_getglobal(L.get(), "Player");
			ASSERT_TRUE(lua_istable(L.get(), -1));
			lua_getfield(L.get(), -1, method);
			lua_remove(L.get(), -2);
			ASSERT_TRUE(lua_isfunction(L.get(), -1));
		}

		void pushPlayer(const std::shared_ptr<Player> &target) {
			Lua::pushSharedUserdata<Player>(L.get(), target);
		}

		void callTableMethod(const char* method, const std::shared_ptr<Player> &target) {
			pushMethod(method);
			pushPlayer(target);
			ASSERT_EQ(lua_pcall(L.get(), 1, 1, 0), LUA_OK) << lua_tostring(L.get(), -1);
			ASSERT_TRUE(lua_istable(L.get(), -1));
		}

		void callTableMethod(const char* method) {
			callTableMethod(method, player);
		}

		uint64_t callScalarMethod(const char* method) {
			pushMethod(method);
			pushPlayer(player);
			EXPECT_EQ(lua_pcall(L.get(), 1, 1, 0), LUA_OK) << lua_tostring(L.get(), -1);
			EXPECT_TRUE(lua_isnumber(L.get(), -1));
			const auto value = static_cast<uint64_t>(lua_tointeger(L.get(), -1));
			lua_pop(L.get(), 1);
			return value;
		}

		bool methodExists(const char* method) {
			lua_getglobal(L.get(), "Player");
			lua_getfield(L.get(), -1, method);
			const auto exists = lua_isfunction(L.get(), -1);
			lua_pop(L.get(), 2);
			return exists;
		}

		MutationResult callMutation(const char* method, lua_Number id) {
			pushMethod(method);
			pushPlayer(player);
			lua_pushnumber(L.get(), id);
			EXPECT_EQ(lua_pcall(L.get(), 2, 4, 0), LUA_OK) << lua_tostring(L.get(), -1);

			MutationResult mutation;
			EXPECT_TRUE(lua_isboolean(L.get(), -4));
			EXPECT_TRUE(lua_isnumber(L.get(), -3));
			EXPECT_TRUE(lua_isnumber(L.get(), -2));
			EXPECT_TRUE(lua_isstring(L.get(), -1));
			mutation.success = lua_toboolean(L.get(), -4) != 0;
			mutation.before = static_cast<uint32_t>(lua_tointeger(L.get(), -3));
			mutation.after = static_cast<uint32_t>(lua_tointeger(L.get(), -2));
			mutation.result = Lua::getString(L.get(), -1);
			lua_pop(L.get(), 4);
			return mutation;
		}

		uint64_t numberField(int tableIndex, const char* field) {
			lua_getfield(L.get(), tableIndex, field);
			EXPECT_TRUE(lua_isnumber(L.get(), -1));
			const auto value = static_cast<uint64_t>(lua_tointeger(L.get(), -1));
			lua_pop(L.get(), 1);
			return value;
		}

		std::set<std::string> stringKeys(int tableIndex) {
			const auto absoluteIndex = tableIndex > 0 ? tableIndex : lua_gettop(L.get()) + tableIndex + 1;
			std::set<std::string> keys;
			lua_pushnil(L.get());
			while (lua_next(L.get(), absoluteIndex) != 0) {
				EXPECT_TRUE(lua_isstring(L.get(), -2));
				keys.insert(Lua::getString(L.get(), -2));
				lua_pop(L.get(), 1);
			}
			return keys;
		}

		std::unique_ptr<lua_State, decltype(&lua_close)> L { nullptr, &lua_close };
		std::shared_ptr<Player> player;

		inline static di::extension::injector<> injector {};
		inline static di::extension::injector<>* previousTestContainer = nullptr;
	};
} // namespace

TEST(TalkActionDisciplinePermissionTest, AllowsOnlyGamemasterAndHigherAccountTypes) {
	TalkAction action;
	action.setGroupType(GROUP_TYPE_GAMEMASTER);

	EXPECT_FALSE(action.canExecute(ACCOUNT_TYPE_NORMAL));
	EXPECT_FALSE(action.canExecute(ACCOUNT_TYPE_TUTOR));
	EXPECT_FALSE(action.canExecute(ACCOUNT_TYPE_SENIORTUTOR));
	EXPECT_TRUE(action.canExecute(ACCOUNT_TYPE_GAMEMASTER));
	EXPECT_TRUE(action.canExecute(ACCOUNT_TYPE_COMMUNITYMANAGER));
	EXPECT_TRUE(action.canExecute(ACCOUNT_TYPE_GOD));
}

TEST_F(DisciplineLuaBindingsTest, RegistersGranularReadsWithoutLegacyProfile) {
	for (const auto method : {
			 "getDisciplines",
			 "getAttributes",
			 "getStats",
			 "getAttributePot",
			 "getAttributeTec",
			 "getAttributeVig",
			 "getAttributeSin",
			 "getAttributeEsp",
			 "getStatPhysicalAttack",
			 "getStatMagicalAttack",
			 "getStatPrecision",
			 "getStatPhysicalDefense",
			 "getStatMagicalDefense",
			 "getStatMaximumHealth",
			 "getStatMaximumMana",
		 }) {
		EXPECT_TRUE(methodExists(method)) << method;
	}
	EXPECT_FALSE(methodExists("getDisciplineProfile"));
}

TEST_F(DisciplineLuaBindingsTest, AttributeTableHasExactlyTheFiveAtsKeys) {
	callTableMethod("getAttributes");
	EXPECT_EQ((std::set<std::string> { "pot", "tec", "vig", "sin", "esp" }), stringKeys(-1));
}

TEST_F(DisciplineLuaBindingsTest, StatsTableHasExactlyTheSevenStableKeys) {
	callTableMethod("getStats");
	EXPECT_EQ((std::set<std::string> { "physicalAttack", "magicalAttack", "precision", "physicalDefense", "magicalDefense", "maximumHealth", "maximumMana" }), stringKeys(-1));
}

TEST_F(DisciplineLuaBindingsTest, EmptyReadsReturnEveryAttributeAndStatAsZero) {
	callTableMethod("getAttributes");
	EXPECT_EQ(0u, numberField(-1, "pot"));
	EXPECT_EQ(0u, numberField(-1, "tec"));
	EXPECT_EQ(0u, numberField(-1, "vig"));
	EXPECT_EQ(0u, numberField(-1, "sin"));
	EXPECT_EQ(0u, numberField(-1, "esp"));
	lua_pop(L.get(), 1);
	callTableMethod("getStats");
	EXPECT_EQ(0u, numberField(-1, "physicalAttack"));
	EXPECT_EQ(0u, numberField(-1, "magicalAttack"));
	EXPECT_EQ(0u, numberField(-1, "precision"));
	EXPECT_EQ(0u, numberField(-1, "physicalDefense"));
	EXPECT_EQ(0u, numberField(-1, "magicalDefense"));
	EXPECT_EQ(0u, numberField(-1, "maximumHealth"));
	EXPECT_EQ(0u, numberField(-1, "maximumMana"));
	lua_pop(L.get(), 1);
	callTableMethod("getDisciplines");
	EXPECT_EQ(0u, lua_objlen(L.get(), -1));
}

TEST_F(DisciplineLuaBindingsTest, AggregateReadsReturnCalculatedValuesAndOrderedCurrentDisciplines) {
	loadCatalog(R"xml(<disciplines>
		<discipline id="2" name="Defesa"><attribute id="esp" perLevel="2"/></discipline>
		<discipline id="1" name="Armas"><attribute id="pot" perLevel="1"/></discipline>
	</disciplines>)xml");
	player->setLevel(3);
	ASSERT_TRUE(player->disciplines().addRank(2).success());
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	ASSERT_TRUE(player->disciplines().addRank(1).success());

	callTableMethod("getAttributes");
	EXPECT_EQ(6u, numberField(-1, "pot"));
	EXPECT_EQ(0u, numberField(-1, "tec"));
	EXPECT_EQ(0u, numberField(-1, "vig"));
	EXPECT_EQ(0u, numberField(-1, "sin"));
	EXPECT_EQ(6u, numberField(-1, "esp"));
	lua_pop(L.get(), 1);
	callTableMethod("getStats");
	EXPECT_EQ(6u, numberField(-1, "physicalAttack"));
	EXPECT_EQ(0u, numberField(-1, "magicalAttack"));
	EXPECT_EQ(0u, numberField(-1, "precision"));
	EXPECT_EQ(2u, numberField(-1, "physicalDefense"));
	EXPECT_EQ(5u, numberField(-1, "magicalDefense"));
	EXPECT_EQ(0u, numberField(-1, "maximumHealth"));
	EXPECT_EQ(30u, numberField(-1, "maximumMana"));
	lua_pop(L.get(), 1);
	callTableMethod("getDisciplines");
	ASSERT_EQ(2u, lua_objlen(L.get(), -1));
	lua_rawgeti(L.get(), -1, 1);
	EXPECT_EQ(1u, numberField(-1, "id"));
	EXPECT_EQ(2u, numberField(-1, "rank"));
	lua_getfield(L.get(), -1, "name");
	EXPECT_EQ("Armas", Lua::getString(L.get(), -1));
	lua_pop(L.get(), 1);
	lua_getfield(L.get(), -1, "perLevel");
	EXPECT_EQ((std::set<std::string> { "pot", "tec", "vig", "sin", "esp" }), stringKeys(-1));
	EXPECT_EQ(1u, numberField(-1, "pot"));
	EXPECT_EQ(0u, numberField(-1, "tec"));
	EXPECT_EQ(0u, numberField(-1, "vig"));
	EXPECT_EQ(0u, numberField(-1, "sin"));
	EXPECT_EQ(0u, numberField(-1, "esp"));
	lua_pop(L.get(), 2);
	lua_rawgeti(L.get(), -1, 2);
	EXPECT_EQ(2u, numberField(-1, "id"));
	EXPECT_EQ(1u, numberField(-1, "rank"));
}

TEST_F(DisciplineLuaBindingsTest, PublicMaximumRemainsExactForAttributesAndStats) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="Overflow"><attribute id="pot" perLevel="4294967295"/></discipline></disciplines>)xml");
	player->setLevel(std::numeric_limits<uint32_t>::max());
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
												  { "1", ValueWrapper(ValueVariant { std::numeric_limits<IntType>::max() }) },
											  });

	callTableMethod("getAttributes");
	EXPECT_EQ(maxPublicDerivedStat, numberField(-1, "pot"));
	lua_pop(L.get(), 1);
	callTableMethod("getStats");
	EXPECT_EQ(maxPublicDerivedStat, numberField(-1, "physicalAttack"));
}

TEST_F(DisciplineLuaBindingsTest, AggregateReadsAreReadOnlyAndScopedToTheReceiver) {
	player->setGUID(1);
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	const auto ranksBefore = player->disciplines().ranks();
	const auto writesBefore = dynamic_cast<KVMemory &>(injector.create<KVStore &>()).writes();
	const auto emptyPlayer = std::make_shared<Player>();
	emptyPlayer->setGUID(2);

	callTableMethod("getAttributes", emptyPlayer);
	EXPECT_EQ(0u, numberField(-1, "pot"));
	lua_pop(L.get(), 1);
	callTableMethod("getAttributes", player);
	EXPECT_EQ(1u, numberField(-1, "pot"));

	EXPECT_EQ(ranksBefore, player->disciplines().ranks());
	EXPECT_EQ(writesBefore, dynamic_cast<KVMemory &>(injector.create<KVStore &>()).writes());
}

TEST_F(DisciplineLuaBindingsTest, ScalarAttributeGettersMatchAggregateFields) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="All"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="2"/><attribute id="vig" perLevel="3"/><attribute id="sin" perLevel="4"/><attribute id="esp" perLevel="5"/></discipline></disciplines>)xml");
	ASSERT_TRUE(player->disciplines().addRank(1).success());

	callTableMethod("getAttributes");
	for (const auto &[method, field] : {
			 std::pair { "getAttributePot", "pot" },
			 std::pair { "getAttributeTec", "tec" },
			 std::pair { "getAttributeVig", "vig" },
			 std::pair { "getAttributeSin", "sin" },
			 std::pair { "getAttributeEsp", "esp" },
		 }) {
		EXPECT_EQ(numberField(-1, field), callScalarMethod(method)) << method;
	}
	lua_pop(L.get(), 1);
}

TEST_F(DisciplineLuaBindingsTest, ScalarStatGettersMatchAggregateFieldsAndNotRuntimeResources) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="All"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="1"/><attribute id="vig" perLevel="1"/><attribute id="sin" perLevel="1"/><attribute id="esp" perLevel="1"/></discipline></disciplines>)xml");
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	const auto runtimeMaximumHealth = player->getMaxHealth();
	const auto runtimeMaximumMana = player->getMaxMana();

	callTableMethod("getStats");
	for (const auto &[method, field] : {
			 std::pair { "getStatPhysicalAttack", "physicalAttack" },
			 std::pair { "getStatMagicalAttack", "magicalAttack" },
			 std::pair { "getStatPrecision", "precision" },
			 std::pair { "getStatPhysicalDefense", "physicalDefense" },
			 std::pair { "getStatMagicalDefense", "magicalDefense" },
			 std::pair { "getStatMaximumHealth", "maximumHealth" },
			 std::pair { "getStatMaximumMana", "maximumMana" },
		 }) {
		EXPECT_EQ(numberField(-1, field), callScalarMethod(method)) << method;
	}
	EXPECT_EQ(5u, callScalarMethod("getStatMaximumHealth"));
	EXPECT_EQ(5u, callScalarMethod("getStatMaximumMana"));
	EXPECT_NE(static_cast<uint64_t>(runtimeMaximumHealth), callScalarMethod("getStatMaximumHealth"));
	EXPECT_NE(static_cast<uint64_t>(runtimeMaximumMana), callScalarMethod("getStatMaximumMana"));
	lua_pop(L.get(), 1);
}

TEST_F(DisciplineLuaBindingsTest, AddRankReturnsStableSuccessFields) {
	const auto mutation = callMutation("addDisciplineRank", 1);
	EXPECT_TRUE(mutation.success);
	EXPECT_EQ(0u, mutation.before);
	EXPECT_EQ(1u, mutation.after);
	EXPECT_EQ("success", mutation.result);
	EXPECT_EQ(1u, player->disciplines().ranks().at(1));
}

TEST_F(DisciplineLuaBindingsTest, RemoveRankReturnsStableSuccessFields) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	const auto mutation = callMutation("removeDisciplineRank", 1);
	EXPECT_TRUE(mutation.success);
	EXPECT_EQ(1u, mutation.before);
	EXPECT_EQ(0u, mutation.after);
	EXPECT_EQ("success", mutation.result);
	EXPECT_TRUE(player->disciplines().ranks().empty());
}

TEST_F(DisciplineLuaBindingsTest, UnknownDisciplineReturnsStableFailureWithoutMutation) {
	const auto mutation = callMutation("addDisciplineRank", 2);
	EXPECT_FALSE(mutation.success);
	EXPECT_EQ(0u, mutation.before);
	EXPECT_EQ(0u, mutation.after);
	EXPECT_EQ("unknown_discipline", mutation.result);
	EXPECT_TRUE(player->disciplines().ranks().empty());
}

TEST_F(DisciplineLuaBindingsTest, RemovingUnownedDisciplineReturnsStableFailure) {
	const auto mutation = callMutation("removeDisciplineRank", 1);
	EXPECT_FALSE(mutation.success);
	EXPECT_EQ(0u, mutation.before);
	EXPECT_EQ(0u, mutation.after);
	EXPECT_EQ("not_owned", mutation.result);
}

TEST_F(DisciplineLuaBindingsTest, InvalidNumericIdsNeverWrapIntoValidIds) {
	for (const auto id : { -1.0, 0.0, 1.5, 65536.0 }) {
		const auto mutation = callMutation("addDisciplineRank", id);
		EXPECT_FALSE(mutation.success);
		EXPECT_EQ(0u, mutation.before);
		EXPECT_EQ(0u, mutation.after);
		EXPECT_EQ("invalid_id", mutation.result);
	}
	EXPECT_TRUE(player->disciplines().ranks().empty());
}

TEST_F(DisciplineLuaBindingsTest, TechnicalRankLimitReturnsStableFailureWithoutMutation) {
	const ValueWrapper ranks {
		{ "1", ValueWrapper(ValueVariant { std::numeric_limits<IntType>::max() }) },
	};
	player->kv()->scoped("disciplines")->set("ranks", ranks);
	const auto mutation = callMutation("addDisciplineRank", 1);
	EXPECT_FALSE(mutation.success);
	EXPECT_EQ(static_cast<uint32_t>(std::numeric_limits<IntType>::max()), mutation.before);
	EXPECT_EQ(static_cast<uint32_t>(std::numeric_limits<IntType>::max()), mutation.after);
	EXPECT_EQ("rank_limit", mutation.result);
}
