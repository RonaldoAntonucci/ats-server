/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/spells.hpp"
#include "creatures/players/player.hpp"
#include "lua/functions/creatures/combat/spell_functions.hpp"
#include "lua/functions/lua_functions_loader.hpp"
#include "lua/scripts/scripts.hpp"

#include <gtest/gtest.h>

#include "lib/logging/in_memory_logger.hpp"

namespace {
	struct PreparedCallbackObservation {
		int argumentCount = 0;
		int contextFieldCount = 0;
		uint64_t id = 0;
		Position origin;
		Direction direction = DIRECTION_NONE;
		std::string reason;
	};

	PreparedCallbackObservation callbackObservation;

	void inspectPreparedContext(lua_State* L, int index) {
		callbackObservation.contextFieldCount = 0;
		lua_pushnil(L);
		while (lua_next(L, index) != 0) {
			++callbackObservation.contextFieldCount;
			lua_pop(L, 1);
		}
		lua_getfield(L, index, "id");
		callbackObservation.id = Lua::getNumber<uint64_t>(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, index, "origin");
		const int originIndex = lua_gettop(L);
		lua_getfield(L, originIndex, "x");
		callbackObservation.origin.x = Lua::getNumber<uint16_t>(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, originIndex, "y");
		callbackObservation.origin.y = Lua::getNumber<uint16_t>(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, originIndex, "z");
		callbackObservation.origin.z = Lua::getNumber<uint8_t>(L, -1);
		lua_pop(L, 1);
		lua_pop(L, 1);
		lua_getfield(L, index, "direction");
		callbackObservation.direction = Lua::getNumber<Direction>(L, -1);
		lua_pop(L, 1);
	}

	int inspectPreparedBooleanCallback(lua_State* L) {
		callbackObservation.argumentCount = lua_gettop(L);
		inspectPreparedContext(L, 3);
		lua_settop(L, 0);
		lua_pushboolean(L, 1);
		return 1;
	}

	int inspectPreparedInterruptCallback(lua_State* L) {
		callbackObservation.argumentCount = lua_gettop(L);
		inspectPreparedContext(L, 3);
		callbackObservation.reason = Lua::getString(L, 4);
		return 0;
	}

	class PreparedCastBindingsTest : public ::testing::Test {
	protected:
		static void SetUpTestSuite() {
			previousTestContainer = DI::getTestContainer();
			InMemoryLogger::install(injector);
			DI::setTestContainer(&injector);
		}

		static void TearDownTestSuite() {
			g_spells().clear();
			g_scripts().getScriptInterface().reInitState();
			DI::setTestContainer(previousTestContainer);
		}

		void SetUp() override {
			g_spells().clear();
			ASSERT_TRUE(g_scripts().getScriptInterface().reInitState());
			L = g_scripts().getScriptInterface().getLuaState();
			SpellFunctions::init(L);
			spell = std::static_pointer_cast<InstantSpell>(createSpell("instant"));
		}

		std::shared_ptr<Spell> createSpell(const char* type) {
			lua_getglobal(L, "Spell");
			lua_pushstring(L, type);
			EXPECT_EQ(LUA_OK, lua_pcall(L, 1, 1, 0)) << lua_tostring(L, -1);
			auto created = Lua::getUserdataShared<Spell>(L, -1, "Spell");
			lua_pop(L, 1);
			return created;
		}

		void pushMethod(const char* method, const std::shared_ptr<Spell> &target = nullptr) {
			lua_getglobal(L, "Spell");
			ASSERT_TRUE(lua_istable(L, -1));
			lua_getfield(L, -1, method);
			lua_remove(L, -2);
			ASSERT_TRUE(lua_isfunction(L, -1)) << method;
			Lua::pushSharedUserdata<Spell>(L, target ? target : spell);
		}

		bool methodExists(const char* method) {
			lua_getglobal(L, "Spell");
			lua_getfield(L, -1, method);
			const bool exists = lua_isfunction(L, -1);
			lua_pop(L, 2);
			return exists;
		}

		bool callPrepare(const std::function<void(lua_State*)> &pushArgument) {
			pushMethod("prepare");
			pushArgument(L);
			EXPECT_EQ(LUA_OK, lua_pcall(L, 2, 1, 0)) << lua_tostring(L, -1);
			EXPECT_TRUE(lua_isboolean(L, -1));
			const bool result = lua_toboolean(L, -1) != 0;
			lua_pop(L, 1);
			return result;
		}

		bool callPrepareTable(const std::function<void(lua_State*)> &populate) {
			return callPrepare([&populate](lua_State* state) {
				lua_newtable(state);
				populate(state);
			});
		}

		void setIntegerField(const char* field, lua_Integer value) {
			lua_pushinteger(L, value);
			lua_setfield(L, -2, field);
		}

		void setBooleanField(const char* field, bool value) {
			lua_pushboolean(L, value);
			lua_setfield(L, -2, field);
		}

		bool callCallbackSetter(const char* method, bool pushFunction) {
			pushMethod(method);
			if (pushFunction) {
				lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
			} else {
				lua_pushinteger(L, 1);
			}
			EXPECT_EQ(LUA_OK, lua_pcall(L, 2, 1, 0)) << lua_tostring(L, -1);
			EXPECT_TRUE(lua_isboolean(L, -1));
			const bool result = lua_toboolean(L, -1) != 0;
			lua_pop(L, 1);
			return result;
		}

		bool callRegister() {
			pushMethod("register");
			EXPECT_EQ(LUA_OK, lua_pcall(L, 1, 1, 0)) << lua_tostring(L, -1);
			EXPECT_TRUE(lua_isboolean(L, -1));
			const bool result = lua_toboolean(L, -1) != 0;
			lua_pop(L, 1);
			return result;
		}

		int32_t installCallback(lua_CFunction callback) {
			lua_pushcfunction(L, callback);
			return g_scripts().getScriptInterface().getEvent();
		}

		lua_State* L = nullptr;
		std::shared_ptr<InstantSpell> spell;

		inline static di::extension::injector<> injector {};
		inline static di::extension::injector<>* previousTestContainer = nullptr;
	};
}

TEST_F(PreparedCastBindingsTest, RegistersTheDocumentedSurface) {
	EXPECT_TRUE(methodExists("prepare"));
	EXPECT_TRUE(methodExists("onPrepareStart"));
	EXPECT_TRUE(methodExists("onPrepareInterrupt"));
}

TEST_F(PreparedCastBindingsTest, AcceptsDurationWithFalsePolicyDefaults) {
	ASSERT_TRUE(callPrepareTable([this](lua_State*) { setIntegerField("duration", 700); }));

	EXPECT_TRUE(spell->isPreparedCastDefinitionValid());
	EXPECT_EQ(700u, spell->getPreparedCastConfig().durationMs);
	EXPECT_FALSE(spell->getPreparedCastConfig().lockMovement);
	EXPECT_FALSE(spell->getPreparedCastConfig().lockDirection);
	EXPECT_FALSE(spell->getPreparedCastConfig().interruptOnPositionChange);
}

TEST_F(PreparedCastBindingsTest, AcceptsEveryApprovedBooleanPolicy) {
	ASSERT_TRUE(callPrepareTable([this](lua_State*) {
		setIntegerField("duration", 701);
		setBooleanField("lockMovement", true);
		setBooleanField("lockDirection", true);
		setBooleanField("interruptOnPositionChange", true);
	}));

	EXPECT_EQ(701u, spell->getPreparedCastConfig().durationMs);
	EXPECT_TRUE(spell->getPreparedCastConfig().lockMovement);
	EXPECT_TRUE(spell->getPreparedCastConfig().lockDirection);
	EXPECT_TRUE(spell->getPreparedCastConfig().interruptOnPositionChange);
}

TEST_F(PreparedCastBindingsTest, RejectsMissingZeroNegativeFractionalAndWrongTypeDuration) {
	EXPECT_FALSE(callPrepareTable([](lua_State*) {}));
	EXPECT_FALSE(callPrepareTable([this](lua_State*) { setIntegerField("duration", 0); }));
	EXPECT_FALSE(callPrepareTable([this](lua_State*) { setIntegerField("duration", -1); }));
	EXPECT_FALSE(callPrepareTable([](lua_State* state) {
		lua_pushnumber(state, 700.5);
		lua_setfield(state, -2, "duration");
	}));
	EXPECT_FALSE(callPrepareTable([](lua_State* state) {
		lua_pushstring(state, "700");
		lua_setfield(state, -2, "duration");
	}));
	EXPECT_FALSE(spell->isPreparedCastDefinitionValid());
}

TEST_F(PreparedCastBindingsTest, RejectsDurationAboveTheNativeSchedulerRange) {
	EXPECT_FALSE(callPrepareTable([](lua_State* state) {
		lua_pushnumber(state, static_cast<lua_Number>(std::numeric_limits<uint32_t>::max()) + 1.0);
		lua_setfield(state, -2, "duration");
	}));
	EXPECT_FALSE(spell->isPreparedCastDefinitionValid());
	EXPECT_EQ(0u, spell->getPreparedCastConfig().durationMs);
}

TEST_F(PreparedCastBindingsTest, RejectsNonTableUnknownKeysAndWrongBooleanTypes) {
	EXPECT_FALSE(callPrepare([](lua_State* state) { lua_pushinteger(state, 700); }));
	EXPECT_FALSE(callPrepareTable([this](lua_State*) {
		setIntegerField("duration", 700);
		setBooleanField("typo", true);
	}));
	for (const char* field : { "lockMovement", "lockDirection", "interruptOnPositionChange" }) {
		EXPECT_FALSE(callPrepareTable([this, field](lua_State* state) {
			setIntegerField("duration", 700);
			lua_pushinteger(state, 1);
			lua_setfield(state, -2, field);
		})) << field;
	}
}

TEST_F(PreparedCastBindingsTest, ValidCallAtomicallyReplacesAnInvalidCandidate) {
	spell->setPreparedCastConfig({ 333, true, false, false });
	EXPECT_FALSE(callPrepareTable([this](lua_State*) { setIntegerField("duration", 0); }));
	EXPECT_EQ(333u, spell->getPreparedCastConfig().durationMs);
	EXPECT_TRUE(spell->getPreparedCastConfig().lockMovement);
	EXPECT_FALSE(spell->isPreparedCastDefinitionValid());

	ASSERT_TRUE(callPrepareTable([this](lua_State*) {
		setIntegerField("duration", 700);
		setBooleanField("lockDirection", true);
	}));
	EXPECT_TRUE(spell->isPreparedCastDefinitionValid());
	EXPECT_EQ(700u, spell->getPreparedCastConfig().durationMs);
	EXPECT_FALSE(spell->getPreparedCastConfig().lockMovement);
	EXPECT_TRUE(spell->getPreparedCastConfig().lockDirection);
}

TEST_F(PreparedCastBindingsTest, RejectsPrepareOnRuneSpell) {
	const auto rune = createSpell("rune");
	pushMethod("prepare", rune);
	lua_newtable(L);
	setIntegerField("duration", 700);
	ASSERT_EQ(LUA_OK, lua_pcall(L, 2, 1, 0)) << lua_tostring(L, -1);
	EXPECT_FALSE(lua_toboolean(L, -1));
	lua_pop(L, 1);
}

TEST_F(PreparedCastBindingsTest, LoadsOptionalCallbacksIndependently) {
	EXPECT_TRUE(callCallbackSetter("onPrepareStart", true));
	EXPECT_TRUE(spell->hasPrepareStartCallback());
	EXPECT_GT(spell->getPrepareStartScriptId(), 0);
	EXPECT_FALSE(spell->hasPrepareInterruptCallback());

	EXPECT_TRUE(callCallbackSetter("onPrepareInterrupt", true));
	EXPECT_TRUE(spell->hasPrepareInterruptCallback());
	EXPECT_GT(spell->getPrepareInterruptScriptId(), spell->getPrepareStartScriptId());
}

TEST_F(PreparedCastBindingsTest, RejectsNonFunctionCallbacksWithoutPartialIdentity) {
	EXPECT_FALSE(callCallbackSetter("onPrepareStart", false));
	EXPECT_FALSE(callCallbackSetter("onPrepareInterrupt", false));
	EXPECT_FALSE(spell->hasPrepareStartCallback());
	EXPECT_FALSE(spell->hasPrepareInterruptCallback());
}

TEST_F(PreparedCastBindingsTest, InvalidPreparationCannotRegisterEvenWithFinalCallback) {
	spell->setName("invalid prepared binding");
	spell->setWords("invalid prepared binding");
	spell->setScriptId(100);
	ASSERT_FALSE(callPrepareTable([this](lua_State*) { setIntegerField("duration", 0); }));

	EXPECT_FALSE(callRegister());
	EXPECT_FALSE(g_spells().hasInstantSpell(spell->getWords()));
}

TEST_F(PreparedCastBindingsTest, PreparedSpellStillRequiresFinalCallback) {
	spell->setName("missing final prepared binding");
	spell->setWords("missing final prepared binding");
	ASSERT_TRUE(callPrepareTable([this](lua_State*) { setIntegerField("duration", 700); }));

	EXPECT_FALSE(callRegister());
	EXPECT_FALSE(g_spells().hasInstantSpell(spell->getWords()));
}

TEST_F(PreparedCastBindingsTest, ValidPreparedSpellRegistersWithoutOptionalCallbacks) {
	spell->setName("optional prepared binding");
	spell->setWords("optional prepared binding");
	spell->setScriptId(101);
	ASSERT_TRUE(callPrepareTable([this](lua_State*) { setIntegerField("duration", 700); }));

	EXPECT_TRUE(callRegister());
	EXPECT_TRUE(g_spells().hasInstantSpell(spell->getWords()));
	EXPECT_FALSE(spell->hasPrepareStartCallback());
	EXPECT_FALSE(spell->hasPrepareInterruptCallback());
}

TEST_F(PreparedCastBindingsTest, DeliversOnlyTheApprovedContextAndStableReason) {
	callbackObservation = {};
	spell->setPrepareStartScriptId(installCallback(inspectPreparedBooleanCallback));
	const auto creature = std::make_shared<Player>();
	const LuaVariant variant;
	const PreparedCastContext context { 4242, Position { 100, 200, 7 }, DIRECTION_WEST };

	EXPECT_TRUE(spell->executePrepareStart(creature, variant, context)) << g_scripts().getScriptInterface().getLastLuaError();
	EXPECT_EQ(3, callbackObservation.argumentCount);
	EXPECT_EQ(3, callbackObservation.contextFieldCount);
	EXPECT_EQ(4242u, callbackObservation.id);
	EXPECT_EQ((Position { 100, 200, 7 }), callbackObservation.origin);
	EXPECT_EQ(DIRECTION_WEST, callbackObservation.direction);

	callbackObservation = {};
	spell->setScriptId(installCallback(inspectPreparedBooleanCallback));
	ASSERT_TRUE(spell->executePreparedCast(creature, variant, context));
	EXPECT_EQ(3, callbackObservation.argumentCount);
	EXPECT_EQ(3, callbackObservation.contextFieldCount);
	EXPECT_EQ(4242u, callbackObservation.id);

	callbackObservation = {};
	spell->setPrepareInterruptScriptId(installCallback(inspectPreparedInterruptCallback));
	spell->executePrepareInterrupt(creature, variant, context, PreparedCastInterruptReason::PositionChange);
	EXPECT_EQ(4, callbackObservation.argumentCount);
	EXPECT_EQ(3, callbackObservation.contextFieldCount);
	EXPECT_EQ("position_change", callbackObservation.reason);
}
