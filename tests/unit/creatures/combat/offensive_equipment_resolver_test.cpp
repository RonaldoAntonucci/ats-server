/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/offensive_equipment_resolver.hpp"

#define private public
#include "creatures/players/player.hpp"
#undef private
#include "items/containers/container.hpp"

#include <gtest/gtest.h>

#include "lib/di/container.hpp"
#include "lib/logging/in_memory_logger.hpp"

namespace {
	constexpr uint16_t swordId = 65010;
	constexpr uint16_t axeId = 65011;
	constexpr uint16_t clubId = 65012;
	constexpr uint16_t bowId = 65013;
	constexpr uint16_t crossbowId = 65014;
	constexpr uint16_t shieldId = 65015;
	constexpr uint16_t arrowId = 65016;
	constexpr uint16_t boltId = 65017;
	constexpr uint16_t quiverId = 65018;
	constexpr uint16_t wandId = 65019;
	constexpr uint16_t spearId = 65020;
	constexpr uint16_t strongerSwordId = 65021;
	constexpr uint16_t twoHandedAxeId = 65022;
	constexpr uint16_t spellbookId = 65023;
	constexpr uint16_t lastFixtureId = spellbookId;

	class OffensiveEquipmentResolverTest : public ::testing::Test {
	protected:
		static void SetUpTestSuite() {
			previousTestContainer = DI::getTestContainer();
			injector = std::make_unique<di::extension::injector<>>();
			InMemoryLogger::install(*injector);
			DI::setTestContainer(injector.get());

			auto &items = Item::items.getItems();
			originalItemsSize = items.size();
			for (uint16_t id = swordId; id <= lastFixtureId; ++id) {
				if (id < originalItemsSize) {
					originalItems.emplace(id, std::move(items[id]));
				}
			}
			if (items.size() <= lastFixtureId) {
				items.resize(lastFixtureId + 1);
			}

			configureWeapon(items[swordId], swordId, WEAPON_SWORD, 30, 1);
			configureWeapon(items[axeId], axeId, WEAPON_AXE, 31, 1);
			configureWeapon(items[clubId], clubId, WEAPON_CLUB, 32, 1);
			configureWeapon(items[bowId], bowId, WEAPON_DISTANCE, 20, 6, AMMO_ARROW);
			configureWeapon(items[crossbowId], crossbowId, WEAPON_DISTANCE, 25, 7, AMMO_BOLT);
			configureShield(items[shieldId]);
			configureAmmunition(items[arrowId], arrowId, AMMO_ARROW, 10);
			configureAmmunition(items[boltId], boltId, AMMO_BOLT, 11);
			configureQuiver(items[quiverId]);
			configureWeapon(items[wandId], wandId, WEAPON_WAND, 99, 4);
			configureWeapon(items[spearId], spearId, WEAPON_DISTANCE, 50, 5, AMMO_SPEAR);
			configureWeapon(items[strongerSwordId], strongerSwordId, WEAPON_SWORD, 80, 1);
			configureWeapon(items[twoHandedAxeId], twoHandedAxeId, WEAPON_AXE, 42, 1);
			items[twoHandedAxeId].slotPosition = SLOTP_HAND | SLOTP_TWO_HAND;
			configureShield(items[spellbookId]);
			items[spellbookId].id = spellbookId;
			items[spellbookId].spellbook = true;
		}

		static void TearDownTestSuite() {
			auto &items = Item::items.getItems();
			for (auto &[id, itemType] : originalItems) {
				items[id] = std::move(itemType);
			}
			if (items.size() > originalItemsSize) {
				items.resize(originalItemsSize);
			}
			DI::setTestContainer(previousTestContainer);
			injector.reset();
		}

		static void configureWeapon(ItemType &itemType, uint16_t id, WeaponType_t weaponType, int32_t attack, uint8_t range, Ammo_t ammoType = AMMO_NONE) {
			itemType = ItemType {};
			itemType.id = id;
			itemType.name = "offensive resolver fixture";
			itemType.weaponType = weaponType;
			itemType.ammoType = ammoType;
			itemType.attack = attack;
			itemType.shootRange = range;
			itemType.slotPosition = SLOTP_HAND;
		}

		static void configureShield(ItemType &itemType) {
			itemType = ItemType {};
			itemType.id = shieldId;
			itemType.name = "offensive resolver shield";
			itemType.type = ITEM_TYPE_SHIELD;
			itemType.weaponType = WEAPON_SHIELD;
			itemType.defense = 40;
			itemType.shootRange = 1;
			itemType.slotPosition = SLOTP_HAND;
		}

		static void configureAmmunition(ItemType &itemType, uint16_t id, Ammo_t ammoType, int32_t attack) {
			itemType = ItemType {};
			itemType.id = id;
			itemType.name = "offensive resolver ammunition";
			itemType.type = ITEM_TYPE_AMMO;
			itemType.weaponType = WEAPON_AMMO;
			itemType.ammoType = ammoType;
			itemType.attack = attack;
			itemType.stackable = true;
			itemType.slotPosition = SLOTP_AMMO;
		}

		static void configureQuiver(ItemType &itemType) {
			itemType = ItemType {};
			itemType.id = quiverId;
			itemType.name = "offensive resolver quiver";
			itemType.group = ITEM_GROUP_CONTAINER;
			itemType.type = ITEM_TYPE_QUIVER;
			itemType.maxItems = 20;
			itemType.slotPosition = SLOTP_RIGHT;
		}

		static std::shared_ptr<Player> makePlayer() {
			return std::make_shared<Player>(std::shared_ptr<ProtocolGame> {});
		}

		static std::shared_ptr<Item> equip(const std::shared_ptr<Player> &player, Slots_t slot, uint16_t id) {
			auto item = Item::CreateItem(id);
			player->internalAddThing(slot, item);
			return item;
		}

		static std::shared_ptr<Item> equipAmmunition(const std::shared_ptr<Player> &player, uint16_t ammunitionId) {
			auto quiver = equip(player, CONST_SLOT_RIGHT, quiverId);
			auto ammunition = Item::CreateItem(ammunitionId, 10);
			quiver->getContainer()->internalAddThing(ammunition);
			return ammunition;
		}

		static OffensiveEquipmentSnapshot resolved(const EquipmentResolution &resolution) {
			EXPECT_TRUE(resolution.success());
			EXPECT_EQ(EquipmentResolutionResult::Resolved, resolution.result);
			EXPECT_TRUE(resolution.snapshot.has_value());
			return *resolution.snapshot;
		}

		inline static std::unique_ptr<di::extension::injector<>> injector;
		inline static di::extension::injector<>* previousTestContainer = nullptr;
		inline static size_t originalItemsSize = 0;
		inline static std::map<uint16_t, ItemType> originalItems;
	};
}

TEST_F(OffensiveEquipmentResolverTest, ResolvesLeftSword) {
	auto player = makePlayer();
	auto weapon = equip(player, CONST_SLOT_LEFT, swordId);
	const auto &snapshot = resolved(OffensiveEquipmentResolver::resolve(*player));
	EXPECT_EQ(OffensiveProfile::Sword, snapshot.profile);
	EXPECT_EQ(30, snapshot.equipmentPower);
	EXPECT_EQ(1, snapshot.range);
	EXPECT_EQ(weapon, snapshot.weapon);
}

TEST_F(OffensiveEquipmentResolverTest, ResolvesAxe) {
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, axeId);
	EXPECT_EQ(OffensiveProfile::Axe, resolved(OffensiveEquipmentResolver::resolve(*player)).profile);
}

TEST_F(OffensiveEquipmentResolverTest, ResolvesClub) {
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, clubId);
	EXPECT_EQ(OffensiveProfile::Club, resolved(OffensiveEquipmentResolver::resolve(*player)).profile);
}

TEST_F(OffensiveEquipmentResolverTest, ResolvesBowWithOneCompatibleArrowUnit) {
	auto player = makePlayer();
	auto weapon = equip(player, CONST_SLOT_LEFT, bowId);
	auto ammunition = equipAmmunition(player, arrowId);
	const auto &snapshot = resolved(OffensiveEquipmentResolver::resolve(*player));
	EXPECT_EQ(OffensiveProfile::Bow, snapshot.profile);
	EXPECT_EQ(30, snapshot.equipmentPower);
	EXPECT_EQ(6, snapshot.range);
	EXPECT_TRUE(snapshot.requiresAmmunition);
	EXPECT_EQ(weapon, snapshot.weapon);
	EXPECT_EQ(ammunition, snapshot.ammunition);
	EXPECT_EQ(ammunition->getParent(), snapshot.ammunitionParent);
}

TEST_F(OffensiveEquipmentResolverTest, ResolvesCrossbowWithOneCompatibleBoltUnit) {
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, crossbowId);
	auto ammunition = equipAmmunition(player, boltId);
	const auto &snapshot = resolved(OffensiveEquipmentResolver::resolve(*player));
	EXPECT_EQ(OffensiveProfile::Crossbow, snapshot.profile);
	EXPECT_EQ(36, snapshot.equipmentPower);
	EXPECT_EQ(7, snapshot.range);
	EXPECT_EQ(ammunition, snapshot.ammunition);
}

TEST_F(OffensiveEquipmentResolverTest, ResolvesRightWeaponWhenLeftIsEmpty) {
	auto player = makePlayer();
	auto weapon = equip(player, CONST_SLOT_RIGHT, swordId);
	const auto &snapshot = resolved(OffensiveEquipmentResolver::resolve(*player));
	EXPECT_EQ(OffensiveProfile::Sword, snapshot.profile);
	EXPECT_EQ(weapon, snapshot.weapon);
}

TEST_F(OffensiveEquipmentResolverTest, ResolvesRightWeaponWhenLeftIsUnsupported) {
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, wandId);
	auto weapon = equip(player, CONST_SLOT_RIGHT, clubId);
	const auto &snapshot = resolved(OffensiveEquipmentResolver::resolve(*player));
	EXPECT_EQ(OffensiveProfile::Club, snapshot.profile);
	EXPECT_EQ(weapon, snapshot.weapon);
}

TEST_F(OffensiveEquipmentResolverTest, LeftWeaponWinsAndRightWeaponContributesNothing) {
	auto player = makePlayer();
	auto left = equip(player, CONST_SLOT_LEFT, swordId);
	equip(player, CONST_SLOT_RIGHT, strongerSwordId);
	const auto &snapshot = resolved(OffensiveEquipmentResolver::resolve(*player));
	EXPECT_EQ(left, snapshot.weapon);
	EXPECT_EQ(30, snapshot.equipmentPower);
}

TEST_F(OffensiveEquipmentResolverTest, RightWeaponWinsOverLeftShield) {
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, shieldId);
	auto weapon = equip(player, CONST_SLOT_RIGHT, axeId);
	const auto &snapshot = resolved(OffensiveEquipmentResolver::resolve(*player));
	EXPECT_EQ(OffensiveProfile::Axe, snapshot.profile);
	EXPECT_EQ(weapon, snapshot.weapon);
	EXPECT_EQ(31, snapshot.equipmentPower);
}

TEST_F(OffensiveEquipmentResolverTest, LeftWeaponWinsOverRightShield) {
	auto player = makePlayer();
	auto weapon = equip(player, CONST_SLOT_LEFT, clubId);
	equip(player, CONST_SLOT_RIGHT, shieldId);
	const auto &snapshot = resolved(OffensiveEquipmentResolver::resolve(*player));
	EXPECT_EQ(OffensiveProfile::Club, snapshot.profile);
	EXPECT_EQ(weapon, snapshot.weapon);
	EXPECT_EQ(32, snapshot.equipmentPower);
}

TEST_F(OffensiveEquipmentResolverTest, ResolvesLeftShieldWhenNoWeaponExists) {
	auto player = makePlayer();
	auto shield = equip(player, CONST_SLOT_LEFT, shieldId);
	const auto &snapshot = resolved(OffensiveEquipmentResolver::resolve(*player));
	EXPECT_EQ(OffensiveProfile::Shield, snapshot.profile);
	EXPECT_EQ(40, snapshot.equipmentPower);
	EXPECT_EQ(shield, snapshot.weapon);
	EXPECT_FALSE(snapshot.requiresAmmunition);
}

TEST_F(OffensiveEquipmentResolverTest, ResolvesRightShieldWhenLeftHasNoShield) {
	auto player = makePlayer();
	auto shield = equip(player, CONST_SLOT_RIGHT, shieldId);
	EXPECT_EQ(shield, resolved(OffensiveEquipmentResolver::resolve(*player)).weapon);
}

TEST_F(OffensiveEquipmentResolverTest, LeftShieldWinsOverRightShield) {
	auto player = makePlayer();
	auto left = equip(player, CONST_SLOT_LEFT, shieldId);
	equip(player, CONST_SLOT_RIGHT, shieldId);
	EXPECT_EQ(left, resolved(OffensiveEquipmentResolver::resolve(*player)).weapon);
}

TEST_F(OffensiveEquipmentResolverTest, TwoHandedMetadataAddsNoPowerOrBehavior) {
	auto player = makePlayer();
	auto weapon = equip(player, CONST_SLOT_LEFT, twoHandedAxeId);
	const auto &snapshot = resolved(OffensiveEquipmentResolver::resolve(*player));
	EXPECT_EQ(OffensiveProfile::Axe, snapshot.profile);
	EXPECT_EQ(42, snapshot.equipmentPower);
	EXPECT_EQ(weapon, snapshot.weapon);
}

TEST_F(OffensiveEquipmentResolverTest, RejectsUnsupportedWandOnlyEquipment) {
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, wandId);
	const auto resolution = OffensiveEquipmentResolver::resolve(*player);
	EXPECT_EQ(EquipmentResolutionResult::UnsupportedEquipment, resolution.result);
	EXPECT_EQ("unsupported_equipment", equipmentResolutionReason(resolution.result));
	EXPECT_FALSE(resolution.snapshot.has_value());
}

TEST_F(OffensiveEquipmentResolverTest, RejectsUnsupportedThrownWeaponProfile) {
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, spearId);
	EXPECT_EQ(EquipmentResolutionResult::UnsupportedEquipment, OffensiveEquipmentResolver::resolve(*player).result);
}

TEST_F(OffensiveEquipmentResolverTest, RejectsSpellbookEvenWithShieldMetadata) {
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, spellbookId);
	EXPECT_EQ(EquipmentResolutionResult::UnsupportedEquipment, OffensiveEquipmentResolver::resolve(*player).result);
}

TEST_F(OffensiveEquipmentResolverTest, RejectsEmptyHandsAsUnsupportedEquipment) {
	auto player = makePlayer();
	EXPECT_EQ(EquipmentResolutionResult::UnsupportedEquipment, OffensiveEquipmentResolver::resolve(*player).result);
}

TEST_F(OffensiveEquipmentResolverTest, BowWithoutAQuiverReturnsMissingAmmunition) {
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, bowId);
	const auto resolution = OffensiveEquipmentResolver::resolve(*player);
	EXPECT_EQ(EquipmentResolutionResult::MissingAmmunition, resolution.result);
	EXPECT_EQ("missing_ammunition", equipmentResolutionReason(resolution.result));
	EXPECT_FALSE(resolution.snapshot.has_value());
}

TEST_F(OffensiveEquipmentResolverTest, BowDoesNotAcceptBoltAmmunition) {
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, bowId);
	equipAmmunition(player, boltId);
	EXPECT_EQ(EquipmentResolutionResult::MissingAmmunition, OffensiveEquipmentResolver::resolve(*player).result);
}

TEST_F(OffensiveEquipmentResolverTest, CrossbowDoesNotAcceptArrowAmmunition) {
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, crossbowId);
	equipAmmunition(player, arrowId);
	EXPECT_EQ(EquipmentResolutionResult::MissingAmmunition, OffensiveEquipmentResolver::resolve(*player).result);
}

TEST_F(OffensiveEquipmentResolverTest, SelectedBowDoesNotFallBackToRightWeaponWhenAmmoIsMissing) {
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, bowId);
	equip(player, CONST_SLOT_RIGHT, strongerSwordId);
	EXPECT_EQ(EquipmentResolutionResult::MissingAmmunition, OffensiveEquipmentResolver::resolve(*player).result);
}

TEST_F(OffensiveEquipmentResolverTest, SnapshotPowerDoesNotChangeWithLaterItemMutation) {
	auto player = makePlayer();
	auto weapon = equip(player, CONST_SLOT_LEFT, swordId);
	const auto resolution = OffensiveEquipmentResolver::resolve(*player);
	ASSERT_TRUE(resolution.success());
	weapon->setAttribute(ItemAttribute_t::ATTACK, 999);
	EXPECT_EQ(30, resolution.snapshot->equipmentPower);
}

TEST_F(OffensiveEquipmentResolverTest, SnapshotHasNoDerivedAttackOrLegacySkillFields) {
	static_assert(std::is_same_v<decltype(OffensiveEquipmentSnapshot::equipmentPower), int32_t>);
	static_assert(std::is_same_v<decltype(OffensiveEquipmentSnapshot::range), uint8_t>);
	SUCCEED();
}
