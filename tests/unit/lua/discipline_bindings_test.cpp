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
#include "lua/functions/creatures/creature_functions.hpp"
#include "lua/functions/lua_functions_loader.hpp"

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
			<discipline id="1" name="Armamento"><attribute id="for" perLevel="1"/><attribute id="des" perLevel="1"/><attribute id="vit" perLevel="1"/></discipline>
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

		void pushPlayer() {
			Lua::pushUserdata<Player>(L.get(), player);
			Lua::setMetatable(L.get(), -1, "Player");
		}

		void callProfile() {
			pushMethod("getDisciplineProfile");
			pushPlayer();
			ASSERT_EQ(lua_pcall(L.get(), 1, 1, 0), LUA_OK) << lua_tostring(L.get(), -1);
			ASSERT_TRUE(lua_istable(L.get(), -1));
		}

		MutationResult callMutation(const char* method, lua_Number id) {
			pushMethod(method);
			pushPlayer();
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

		std::unique_ptr<lua_State, decltype(&lua_close)> L { nullptr, &lua_close };
		std::shared_ptr<Player> player;

		inline static di::extension::injector<> injector {};
		inline static di::extension::injector<>* previousTestContainer = nullptr;
	};
} // namespace

TEST_F(DisciplineLuaBindingsTest, EmptyProfileReturnsEveryAttributeAsZero) {
	callProfile();
	lua_getfield(L.get(), -1, "attributes");
	ASSERT_TRUE(lua_istable(L.get(), -1));
	EXPECT_EQ(0u, numberField(-1, "for"));
	EXPECT_EQ(0u, numberField(-1, "des"));
	EXPECT_EQ(0u, numberField(-1, "vit"));
	EXPECT_EQ(0u, numberField(-1, "int"));
	EXPECT_EQ(0u, numberField(-1, "von"));
	lua_pop(L.get(), 1);
	lua_getfield(L.get(), -1, "disciplines");
	ASSERT_TRUE(lua_istable(L.get(), -1));
	EXPECT_EQ(0u, lua_objlen(L.get(), -1));
}

TEST_F(DisciplineLuaBindingsTest, ProfileReturnsCalculatedAttributesAndOrderedCurrentNames) {
	loadCatalog(R"xml(<disciplines>
		<discipline id="2" name="Defesa"><attribute id="von" perLevel="2"/></discipline>
		<discipline id="1" name="Armas"><attribute id="for" perLevel="1"/></discipline>
	</disciplines>)xml");
	player->setLevel(3);
	ASSERT_TRUE(player->disciplines().addRank(2).success());
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	ASSERT_TRUE(player->disciplines().addRank(1).success());

	callProfile();
	lua_getfield(L.get(), -1, "attributes");
	ASSERT_TRUE(lua_istable(L.get(), -1));
	EXPECT_EQ(6u, numberField(-1, "for"));
	EXPECT_EQ(0u, numberField(-1, "des"));
	EXPECT_EQ(0u, numberField(-1, "vit"));
	EXPECT_EQ(0u, numberField(-1, "int"));
	EXPECT_EQ(6u, numberField(-1, "von"));
	lua_pop(L.get(), 1);

	lua_getfield(L.get(), -1, "disciplines");
	ASSERT_EQ(2u, lua_objlen(L.get(), -1));
	lua_rawgeti(L.get(), -1, 1);
	EXPECT_EQ(1u, numberField(-1, "id"));
	EXPECT_EQ(2u, numberField(-1, "rank"));
	lua_getfield(L.get(), -1, "name");
	EXPECT_EQ("Armas", Lua::getString(L.get(), -1));
	lua_pop(L.get(), 2);
	lua_rawgeti(L.get(), -1, 2);
	EXPECT_EQ(2u, numberField(-1, "id"));
	EXPECT_EQ(1u, numberField(-1, "rank"));
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
