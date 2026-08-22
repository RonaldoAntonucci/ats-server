/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "items/item.hpp"
#include "lua/functions/items/item_functions.hpp"
#include "lua/functions/lua_functions_loader.hpp"

#include <gtest/gtest.h>

namespace {
	constexpr uint16_t testItemId = 65051;

	class ItemPowerBindingsTest : public ::testing::Test {
	protected:
		static void SetUpTestSuite() {
			auto &items = Item::items.getItems();
			originalItemsSize = items.size();
			if (testItemId < originalItemsSize) {
				originalItem = std::move(items[testItemId]);
			}
			if (items.size() <= testItemId) {
				items.resize(testItemId + 1);
			}
			auto &itemType = items[testItemId];
			itemType = ItemType {};
			itemType.id = testItemId;
			itemType.name = "item power binding fixture";
			itemType.weaponType = WEAPON_SWORD;
			itemType.ammoType = AMMO_ARROW;
			itemType.attack = 30;
			itemType.defense = 40;
			itemType.shootRange = 6;
		}

		static void TearDownTestSuite() {
			auto &items = Item::items.getItems();
			if (originalItem.has_value()) {
				items[testItemId] = std::move(*originalItem);
			}
			if (items.size() > originalItemsSize) {
				items.resize(originalItemsSize);
			}
		}

		void SetUp() override {
			L.reset(luaL_newstate());
			ItemFunctions::init(L.get());
			item = Item::CreateItem(testItemId);
			ASSERT_NE(nullptr, item);
		}

		void pushMethod(const char* method) {
			lua_getglobal(L.get(), "Item");
			ASSERT_TRUE(lua_istable(L.get(), -1));
			lua_getfield(L.get(), -1, method);
			lua_remove(L.get(), -2);
			ASSERT_TRUE(lua_isfunction(L.get(), -1)) << method;
		}

		lua_Number callNumberMethod(const char* method) {
			pushMethod(method);
			Lua::pushUserdata<Item>(L.get(), item);
			Lua::setItemMetatable(L.get(), -1, item);
			EXPECT_EQ(LUA_OK, lua_pcall(L.get(), 1, 1, 0)) << lua_tostring(L.get(), -1);
			EXPECT_TRUE(lua_isnumber(L.get(), -1));
			const auto result = lua_tonumber(L.get(), -1);
			lua_pop(L.get(), 1);
			return result;
		}

		std::unique_ptr<lua_State, decltype(&lua_close)> L { nullptr, &lua_close };
		std::shared_ptr<Item> item;

		inline static size_t originalItemsSize = 0;
		inline static std::optional<ItemType> originalItem;
	};
}

TEST_F(ItemPowerBindingsTest, RegistersAttackAndDefenseMethods) {
	for (const auto method : { "getAttack", "getDefense" }) {
		pushMethod(method);
		lua_pop(L.get(), 1);
	}
}

TEST_F(ItemPowerBindingsTest, ReturnsBaseAttackFromTheItemType) {
	EXPECT_EQ(30, callNumberMethod("getAttack"));
}

TEST_F(ItemPowerBindingsTest, ReturnsEffectiveAttackAttributeOverride) {
	item->setAttribute(ItemAttribute_t::ATTACK, 77);
	EXPECT_EQ(77, callNumberMethod("getAttack"));
}

TEST_F(ItemPowerBindingsTest, ReturnsBaseDefenseFromTheItemType) {
	EXPECT_EQ(40, callNumberMethod("getDefense"));
}

TEST_F(ItemPowerBindingsTest, ReturnsEffectiveDefenseAttributeOverride) {
	item->setAttribute(ItemAttribute_t::DEFENSE, 88);
	EXPECT_EQ(88, callNumberMethod("getDefense"));
}

TEST_F(ItemPowerBindingsTest, ReturnsNilForInvalidItemUserdata) {
	for (const auto method : { "getAttack", "getDefense" }) {
		pushMethod(method);
		lua_pushnil(L.get());
		ASSERT_EQ(LUA_OK, lua_pcall(L.get(), 1, 1, 0)) << lua_tostring(L.get(), -1);
		EXPECT_TRUE(lua_isnil(L.get(), -1));
		lua_pop(L.get(), 1);
	}
}

TEST_F(ItemPowerBindingsTest, LeavesExistingItemTypeMetadataUnchanged) {
	const auto &itemType = Item::items[testItemId];
	EXPECT_EQ(WEAPON_SWORD, itemType.weaponType);
	EXPECT_EQ(AMMO_ARROW, itemType.ammoType);
	EXPECT_EQ(6, itemType.shootRange);
	EXPECT_EQ(30, itemType.attack);
	EXPECT_EQ(40, itemType.defense);
}
