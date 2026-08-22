/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "config/configmanager.hpp"
#include "creatures/combat/offensive_cast_context.hpp"
#include "creatures/combat/spells.hpp"
#include "creatures/players/disciplines/discipline.hpp"
#define private public
#include "creatures/players/player.hpp"
#undef private
#include "items/item.hpp"
#include "items/tile.hpp"
#include "lua/functions/creatures/combat/spell_functions.hpp"
#include "lua/functions/creatures/creature_functions.hpp"
#include "lua/functions/lua_functions_loader.hpp"

#include <gtest/gtest.h>

#include "kv/in_memory_kv.hpp"
#include "lib/di/container.hpp"
#include "lib/logging/in_memory_logger.hpp"

namespace {
	constexpr uint16_t swordId = 65050;

	class OffensiveSpellBindingsTest : public ::testing::Test {
	protected:
		static void SetUpTestSuite() {
			previousTestContainer = DI::getTestContainer();
			injector = std::make_unique<di::extension::injector<>>();
			InMemoryLogger::install(*injector);
			KVMemory::install(*injector);
			DI::setTestContainer(injector.get());

			configFile = std::filesystem::temp_directory_path() / "canary-offensive-spell-bindings.lua";
			std::ofstream config(configFile);
			ASSERT_TRUE(config.is_open());
			config.close();
			configManager().setConfigFileLua(configFile.string());
			ASSERT_TRUE(configManager().load());

			disciplineFile = std::filesystem::temp_directory_path() / "canary-offensive-spell-bindings.xml";
			std::ofstream disciplines(disciplineFile);
			ASSERT_TRUE(disciplines.is_open());
			disciplines << R"xml(<disciplines><discipline id="1" name="Armamento"><attribute id="pot" perLevel="5"/></discipline></disciplines>)xml";
			disciplines.close();
			ASSERT_TRUE(g_disciplines().loadFromXml(disciplineFile));

			auto &items = Item::items.getItems();
			originalItemsSize = items.size();
			if (swordId < originalItemsSize) {
				originalSword = std::move(items[swordId]);
			}
			if (items.size() <= swordId) {
				items.resize(swordId + 1);
			}
			auto &sword = items[swordId];
			sword = ItemType {};
			sword.id = swordId;
			sword.name = "offensive binding sword";
			sword.weaponType = WEAPON_SWORD;
			sword.attack = 30;
			sword.shootRange = 1;
			sword.slotPosition = SLOTP_HAND;
		}

		static void TearDownTestSuite() {
			auto &items = Item::items.getItems();
			if (originalSword.has_value()) {
				items[swordId] = std::move(*originalSword);
			}
			if (items.size() > originalItemsSize) {
				items.resize(originalItemsSize);
			}
			std::error_code error;
			std::filesystem::remove(configFile, error);
			std::filesystem::remove(disciplineFile, error);
			DI::setTestContainer(previousTestContainer);
			injector.reset();
		}

		void SetUp() override {
			logger().reset();
			dynamic_cast<KVMemory &>(injector->create<KVStore &>()).reset();
			L.reset(luaL_newstate());
			luaL_openlibs(L.get());
			CreatureFunctions::init(L.get());
			SpellFunctions::init(L.get());
			spell = std::make_shared<InstantSpell>();
			spell->setName("Synthetic Offensive Art");
		}

		static ConfigManager &configManager() {
			return injector->create<ConfigManager &>();
		}

		static InMemoryLogger &logger() {
			return dynamic_cast<InMemoryLogger &>(injector->create<Logger &>());
		}

		void pushSpellMethod(const char* method) {
			lua_getglobal(L.get(), "Spell");
			ASSERT_TRUE(lua_istable(L.get(), -1));
			lua_getfield(L.get(), -1, method);
			lua_remove(L.get(), -2);
			ASSERT_TRUE(lua_isfunction(L.get(), -1)) << method;
			Lua::pushSharedUserdata<Spell>(L.get(), spell);
		}

		bool spellMethodExists(const char* method) {
			lua_getglobal(L.get(), "Spell");
			lua_getfield(L.get(), -1, method);
			const auto exists = lua_isfunction(L.get(), -1);
			lua_pop(L.get(), 2);
			return exists;
		}

		bool contextMethodExists(const char* method) {
			lua_getglobal(L.get(), "OffensiveCastContext");
			lua_getfield(L.get(), -1, method);
			const auto exists = lua_isfunction(L.get(), -1);
			lua_pop(L.get(), 2);
			return exists;
		}

		void pushStringArray(std::initializer_list<const char*> values) {
			lua_createtable(L.get(), static_cast<int>(values.size()), 0);
			int index = 1;
			for (const auto value : values) {
				Lua::pushString(L.get(), value);
				lua_rawseti(L.get(), -2, index++);
			}
		}

		void pushNormativeParameters() {
			lua_createtable(L.get(), 0, 6);
			for (const auto &[field, value] : {
					 std::pair { "basePower", 10.0 },
					 std::pair { "physicalCoefficient", 1.0 },
					 std::pair { "magicalCoefficient", 0.0 },
					 std::pair { "equipmentCoefficient", 1.0 },
					 std::pair { "secondaryMultiplier", 0.5 },
					 std::pair { "cooldownMilliseconds", 1000.0 },
				 }) {
				lua_pushnumber(L.get(), value);
				lua_setfield(L.get(), -2, field);
			}
		}

		std::pair<bool, std::string> callBooleanReason(int argumentCount) {
			EXPECT_EQ(LUA_OK, lua_pcall(L.get(), argumentCount, 2, 0)) << lua_tostring(L.get(), -1);
			EXPECT_TRUE(lua_isboolean(L.get(), -2));
			EXPECT_TRUE(lua_isstring(L.get(), -1));
			const auto result = std::pair { lua_toboolean(L.get(), -2) != 0, Lua::getString(L.get(), -1) };
			lua_pop(L.get(), 2);
			return result;
		}

		void configureValidDefinition() {
			pushSpellMethod("offensiveParameters");
			pushNormativeParameters();
			ASSERT_EQ((std::pair { true, std::string { "updated" } }), callBooleanReason(2));

			pushSpellMethod("baseTags");
			pushStringArray({ "category.art", "damage.neutral", "damage.physical", "discipline.armament", "execution.attack", "function.offensive" });
			ASSERT_EQ((std::pair { true, std::string { "updated" } }), callBooleanReason(2));
		}

		std::shared_ptr<Player> makeEquippedPlayer() {
			auto player = std::make_shared<Player>();
			auto tile = std::make_shared<DynamicTile>(Position { 1000, 1000, 7 });
			tile->addThing(player);
			liveTiles.emplace_back(std::move(tile));
			player->internalAddThing(CONST_SLOT_LEFT, Item::CreateItem(swordId));
			return player;
		}

		void createContext(const std::shared_ptr<Player> &player) {
			pushSpellMethod("createOffensiveContext");
			Lua::pushSharedUserdata<Player>(L.get(), player);
			ASSERT_EQ(LUA_OK, lua_pcall(L.get(), 2, 2, 0)) << lua_tostring(L.get(), -1);
			ASSERT_TRUE(lua_isuserdata(L.get(), -2));
			ASSERT_EQ("created", Lua::getString(L.get(), -1));
			lua_pop(L.get(), 1);
		}

		std::unique_ptr<lua_State, decltype(&lua_close)> L { nullptr, &lua_close };
		std::shared_ptr<InstantSpell> spell;
		std::vector<std::shared_ptr<Tile>> liveTiles;

		inline static std::unique_ptr<di::extension::injector<>> injector;
		inline static di::extension::injector<>* previousTestContainer = nullptr;
		inline static size_t originalItemsSize = 0;
		inline static std::optional<ItemType> originalSword;
		inline static std::filesystem::path configFile;
		inline static std::filesystem::path disciplineFile;
	};
} // namespace

TEST_F(OffensiveSpellBindingsTest, RegistersTheApprovedSpellAndReadOnlyContextSurface) {
	for (const auto method : { "disciplineRequirement", "offensiveParameters", "baseTags", "profileTags", "createOffensiveContext" }) {
		EXPECT_TRUE(spellMethodExists(method)) << method;
	}
	for (const auto method : { "getProfile", "getEquipmentPower", "getRange", "requiresAmmunition", "getTags", "getPrimaryBaseDamage", "getSecondaryBaseDamage", "validatePrimaryTarget", "canAffect", "commit" }) {
		EXPECT_TRUE(contextMethodExists(method)) << method;
	}
	EXPECT_FALSE(contextMethodExists("setEquipmentPower"));
	EXPECT_FALSE(contextMethodExists("consumeAmmunition"));
}

TEST_F(OffensiveSpellBindingsTest, DeclaresAnAuthoritativeDisciplineRequirement) {
	pushSpellMethod("disciplineRequirement");
	lua_pushinteger(L.get(), 1);
	lua_pushinteger(L.get(), 2);
	EXPECT_EQ((std::pair { true, std::string { "added" } }), callBooleanReason(3));
	ASSERT_EQ(1u, spell->getRequirements().disciplines().size());
	EXPECT_EQ((DisciplineRequirement { .disciplineId = 1, .minimumRank = 2 }), spell->getRequirements().disciplines().front());
}

TEST_F(OffensiveSpellBindingsTest, AcceptsNormativeParametersAndUpdatesLegacyCooldown) {
	pushSpellMethod("offensiveParameters");
	pushNormativeParameters();
	EXPECT_EQ((std::pair { true, std::string { "updated" } }), callBooleanReason(2));
	ASSERT_TRUE(spell->getOffensiveDefinition().has_value());
	EXPECT_EQ(1000u, spell->getCooldown());
	EXPECT_DOUBLE_EQ(0.5, spell->getOffensiveDefinition()->parameters().secondaryMultiplier);
}

TEST_F(OffensiveSpellBindingsTest, RejectsWrongNumericTypesWithFieldAndReason) {
	pushSpellMethod("offensiveParameters");
	pushNormativeParameters();
	Lua::pushString(L.get(), "ten");
	lua_setfield(L.get(), -2, "basePower");
	EXPECT_EQ((std::pair { false, std::string { "wrong_type" } }), callBooleanReason(2));
	ASSERT_TRUE(spell->getOffensiveDefinition().has_value());
	EXPECT_FALSE(spell->getOffensiveDefinition()->valid());
	ASSERT_GE(logger().logCount(), 1u);
	const auto [level, message] = logger().getLogEntry(logger().logCount() - 1);
	EXPECT_EQ("error", level);
	EXPECT_NE(std::string::npos, message.find("field=basePower"));
	EXPECT_NE(std::string::npos, message.find("reason=wrong_type"));
	pushSpellMethod("register");
	ASSERT_EQ(LUA_OK, lua_pcall(L.get(), 1, 1, 0)) << lua_tostring(L.get(), -1);
	EXPECT_FALSE(lua_toboolean(L.get(), -1));
}

TEST_F(OffensiveSpellBindingsTest, RejectsNonFiniteAndOutOfRangeParametersAtomically) {
	pushSpellMethod("offensiveParameters");
	pushNormativeParameters();
	lua_pushnumber(L.get(), std::numeric_limits<double>::infinity());
	lua_setfield(L.get(), -2, "physicalCoefficient");
	EXPECT_EQ((std::pair { false, std::string { "non_finite" } }), callBooleanReason(2));
	EXPECT_DOUBLE_EQ(1.0, spell->getOffensiveDefinition()->parameters().physicalCoefficient);
}

TEST_F(OffensiveSpellBindingsTest, AcceptsSortedDeduplicatedBaseTags) {
	pushSpellMethod("baseTags");
	pushStringArray({ "function.offensive", "category.art", "category.art" });
	EXPECT_EQ((std::pair { true, std::string { "updated" } }), callBooleanReason(2));
	EXPECT_EQ((std::vector<std::string_view> { "category.art", "function.offensive" }), spell->getOffensiveDefinition()->baseTags().names());
}

TEST_F(OffensiveSpellBindingsTest, RejectsAnInvalidTagWithSpellTagAndReasonEvidence) {
	pushSpellMethod("baseTags");
	pushStringArray({ "category.art", "not-a-tag" });
	EXPECT_EQ((std::pair { false, std::string { "missing_namespace" } }), callBooleanReason(2));
	EXPECT_FALSE(spell->getOffensiveDefinition()->valid());
	ASSERT_GE(logger().logCount(), 1u);
	const auto [level, message] = logger().getLogEntry(logger().logCount() - 1);
	EXPECT_EQ("error", level);
	EXPECT_NE(std::string::npos, message.find("spell=Synthetic Offensive Art"));
	EXPECT_NE(std::string::npos, message.find("tag=not-a-tag"));
	EXPECT_NE(std::string::npos, message.find("reason=missing_namespace"));
	pushSpellMethod("register");
	ASSERT_EQ(LUA_OK, lua_pcall(L.get(), 1, 1, 0)) << lua_tostring(L.get(), -1);
	EXPECT_FALSE(lua_toboolean(L.get(), -1));
}

TEST_F(OffensiveSpellBindingsTest, ConfiguresProfileTagsWithoutAnAssaultNameBranch) {
	spell->setName("Second Synthetic Technique");
	pushSpellMethod("profileTags");
	Lua::pushString(L.get(), "sword");
	pushStringArray({ "execution.contact", "weapon.sword" });
	EXPECT_EQ((std::pair { true, std::string { "updated" } }), callBooleanReason(3));
	EXPECT_EQ((std::vector<std::string_view> { "execution.contact", "weapon.sword" }), spell->getOffensiveDefinition()->profileTags(OffensiveProfile::Sword).names());
}

TEST_F(OffensiveSpellBindingsTest, ReturnsNilAndStableReasonForUnsupportedEquipment) {
	configureValidDefinition();
	auto player = std::make_shared<Player>();
	pushSpellMethod("createOffensiveContext");
	Lua::pushSharedUserdata<Player>(L.get(), player);
	ASSERT_EQ(LUA_OK, lua_pcall(L.get(), 2, 2, 0)) << lua_tostring(L.get(), -1);
	EXPECT_TRUE(lua_isnil(L.get(), -2));
	EXPECT_EQ("unsupported_equipment", Lua::getString(L.get(), -1));
}

TEST_F(OffensiveSpellBindingsTest, ExposesTheValidatedImmutableContextSnapshot) {
	configureValidDefinition();
	auto player = makeEquippedPlayer();
	createContext(player);
	lua_setglobal(L.get(), "context");
	ASSERT_EQ(LUA_OK, luaL_dostring(L.get(), R"lua(
		assert(context:getProfile() == "sword")
		assert(context:getEquipmentPower() == 30)
		assert(context:getRange() == 1)
		assert(context:requiresAmmunition() == false)
		assert(context:getPrimaryBaseDamage() == 40)
		assert(context:getSecondaryBaseDamage() == 20)
		assert(table.concat(context:getTags(), ",") == "category.art,damage.neutral,damage.physical,discipline.armament,execution.attack,execution.contact,function.offensive,weapon.sword")
	)lua"))
		<< lua_tostring(L.get(), -1);
}

TEST_F(OffensiveSpellBindingsTest, ContextValuesRemainFrozenAfterEquipmentMutation) {
	configureValidDefinition();
	auto player = makeEquippedPlayer();
	createContext(player);
	auto context = Lua::getUserdataShared<OffensiveCastContext>(L.get(), -1, "OffensiveCastContext");
	ASSERT_NE(nullptr, context);
	player->getInventoryItem(CONST_SLOT_LEFT)->setAttribute(ItemAttribute_t::ATTACK, 99);
	EXPECT_EQ(30, context->equipmentPower());
	EXPECT_EQ(40, context->primaryBaseDamage());
}

TEST_F(OffensiveSpellBindingsTest, ContextSharedUserdataRunsItsTypedGarbageCollector) {
	configureValidDefinition();
	auto player = makeEquippedPlayer();
	createContext(player);
	std::weak_ptr<OffensiveCastContext> weakContext;
	{
		auto context = Lua::getUserdataShared<OffensiveCastContext>(L.get(), -1, "OffensiveCastContext");
		ASSERT_NE(nullptr, context);
		weakContext = context;
	}
	lua_pop(L.get(), 1);
	lua_gc(L.get(), LUA_GCCOLLECT, 0);
	EXPECT_TRUE(weakContext.expired());
}

TEST_F(OffensiveSpellBindingsTest, TargetValidationAndCommitReturnStableFailureShapes) {
	configureValidDefinition();
	auto player = makeEquippedPlayer();
	createContext(player);
	lua_setglobal(L.get(), "context");
	ASSERT_EQ(LUA_OK, luaL_dostring(L.get(), R"lua(
		local valid, reason = context:validatePrimaryTarget(nil)
		assert(valid == false)
		assert(reason == "invalid_primary_target")
		assert(context:canAffect(nil) == false)
		local committed, commitReason = context:commit(nil)
		assert(committed == false)
		assert(commitReason == "invalid_primary_target")
	)lua"))
		<< lua_tostring(L.get(), -1);
}

TEST_F(OffensiveSpellBindingsTest, ASecondSyntheticSpellReusesDefinitionContextTagsAndDamage) {
	spell->setName("Second Synthetic Technique");
	configureValidDefinition();
	pushSpellMethod("disciplineRequirement");
	lua_pushinteger(L.get(), 1);
	lua_pushinteger(L.get(), 1);
	ASSERT_EQ((std::pair { true, std::string { "added" } }), callBooleanReason(3));
	auto player = makeEquippedPlayer();
	createContext(player);
	auto context = Lua::getUserdataShared<OffensiveCastContext>(L.get(), -1, "OffensiveCastContext");
	ASSERT_NE(nullptr, context);
	EXPECT_EQ(OffensiveProfile::Sword, context->profile());
	EXPECT_EQ(40, context->primaryBaseDamage());
}
