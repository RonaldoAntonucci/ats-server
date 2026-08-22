/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/offensive_cast_context.hpp"

#include "config/configmanager.hpp"
#include "creatures/combat/spells.hpp"
#include "creatures/monsters/monster.hpp"
#include "creatures/monsters/monsters.hpp"
#include "creatures/players/disciplines/discipline.hpp"
#include "enums/account_group_type.hpp"

#define private public
#include "creatures/players/player.hpp"
#undef private

#define private public
#include "game/game.hpp"
#undef private
#include "items/containers/container.hpp"
#include "items/tile.hpp"

#include <gtest/gtest.h>

#include "kv/in_memory_kv.hpp"
#include "lib/di/container.hpp"
#include "lib/logging/in_memory_logger.hpp"

namespace {
	constexpr uint16_t swordId = 65030;
	constexpr uint16_t axeId = 65031;
	constexpr uint16_t clubId = 65032;
	constexpr uint16_t bowId = 65033;
	constexpr uint16_t crossbowId = 65034;
	constexpr uint16_t shieldId = 65035;
	constexpr uint16_t arrowId = 65036;
	constexpr uint16_t boltId = 65037;
	constexpr uint16_t quiverId = 65038;
	constexpr uint16_t lastFixtureId = quiverId;

	class TestSpell final : public Spell {
	public:
		bool castSpell(const std::shared_ptr<Creature> &) override {
			return false;
		}

		bool castSpell(const std::shared_ptr<Creature> &, const std::shared_ptr<Creature> &) override {
			return false;
		}

		bool isInstant() const override {
			return true;
		}
	};

	class OffensiveCastContextTest : public ::testing::Test {
	protected:
		static void SetUpTestSuite() {
			previousTestContainer = DI::getTestContainer();
			injector = std::make_unique<di::extension::injector<>>();
			InMemoryLogger::install(*injector);
			KVMemory::install(*injector);
			DI::setTestContainer(injector.get());
			configFile = std::filesystem::temp_directory_path() / "canary-offensive-cast-context.lua";
			std::ofstream config(configFile);
			ASSERT_TRUE(config.is_open());
			config.close();
			configManager().setConfigFileLua(configFile.string());
			ASSERT_TRUE(configManager().load());
			disciplineFile = std::filesystem::temp_directory_path() / "canary-offensive-cast-context.xml";
			std::ofstream disciplines(disciplineFile);
			ASSERT_TRUE(disciplines.is_open());
			disciplines << R"xml(<disciplines><discipline id="1" name="Armamento"><attribute id="pot" perLevel="5"/></discipline></disciplines>)xml";
			disciplines.close();
			ASSERT_TRUE(g_disciplines().loadFromXml(disciplineFile));

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
		}

		static void TearDownTestSuite() {
			auto &items = Item::items.getItems();
			for (auto &[id, itemType] : originalItems) {
				items[id] = std::move(itemType);
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
			kvMemory().reset();
			liveTiles.clear();
		}

		static KVMemory &kvMemory() {
			return dynamic_cast<KVMemory &>(injector->create<KVStore &>());
		}

		static ConfigManager &configManager() {
			return injector->create<ConfigManager &>();
		}

		static void configureWeapon(ItemType &itemType, uint16_t id, WeaponType_t weaponType, int32_t attack, uint8_t range, Ammo_t ammoType = AMMO_NONE) {
			itemType = ItemType {};
			itemType.id = id;
			itemType.name = "offensive context fixture";
			itemType.weaponType = weaponType;
			itemType.ammoType = ammoType;
			itemType.attack = attack;
			itemType.shootRange = range;
			itemType.slotPosition = SLOTP_HAND;
		}

		static void configureShield(ItemType &itemType) {
			itemType = ItemType {};
			itemType.id = shieldId;
			itemType.name = "offensive context shield";
			itemType.type = ITEM_TYPE_SHIELD;
			itemType.weaponType = WEAPON_SHIELD;
			itemType.defense = 40;
			itemType.shootRange = 1;
			itemType.slotPosition = SLOTP_HAND;
		}

		static void configureAmmunition(ItemType &itemType, uint16_t id, Ammo_t ammoType, int32_t attack) {
			itemType = ItemType {};
			itemType.id = id;
			itemType.name = "offensive context ammunition";
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
			itemType.name = "offensive context quiver";
			itemType.group = ITEM_GROUP_CONTAINER;
			itemType.type = ITEM_TYPE_QUIVER;
			itemType.maxItems = 20;
			itemType.slotPosition = SLOTP_RIGHT;
		}

		static SpellTagSet normativeBaseTags(std::vector<SpellTag> additions = {}) {
			std::vector tags {
				SpellTag::CategoryArt,
				SpellTag::DamageNeutral,
				SpellTag::DamagePhysical,
				SpellTag::DisciplineArmament,
				SpellTag::ExecutionAttack,
				SpellTag::FunctionOffensive,
			};
			tags.insert(tags.end(), additions.begin(), additions.end());
			return normalizeSpellTags(std::move(tags));
		}

		static TestSpell makeSpell(std::vector<SpellTag> extraBaseTags = {}) {
			TestSpell spell;
			spell.setName("Synthetic Offensive Art");
			auto &definition = spell.getOrCreateOffensiveDefinition();
			EXPECT_EQ(OffensiveDefinitionUpdateResult::Updated, definition.setBaseTags(normativeBaseTags(std::move(extraBaseTags))));
			definition.freeze();
			return spell;
		}

		static std::shared_ptr<Player> makePlayer(const Position &position = Position { 1000, 1000, 7 }) {
			auto player = std::make_shared<Player>();
			auto group = std::make_shared<Group>();
			group->id = GROUP_TYPE_NORMAL;
			group->access = false;
			player->setGroup(group);
			placeCreature(player, position);
			return player;
		}

		static std::shared_ptr<Monster> makeTarget(const Position &position = Position { 1001, 1000, 7 }, bool attackable = true, int32_t health = 100) {
			auto monsterType = std::make_shared<MonsterType>("offensive context target");
			monsterType->info.isAttackable = attackable;
			monsterType->info.health = health;
			monsterType->info.healthMax = std::max(health, 100);
			auto target = std::make_shared<Monster>(monsterType);
			placeCreature(target, position);
			return target;
		}

		static void placeCreature(const std::shared_ptr<Creature> &creature, const Position &position) {
			auto tile = std::make_shared<DynamicTile>(position);
			tile->addThing(creature);
			liveTiles.emplace_back(std::move(tile));
		}

		static std::shared_ptr<Item> equip(const std::shared_ptr<Player> &player, Slots_t slot, uint16_t id) {
			auto item = Item::CreateItem(id);
			player->internalAddThing(slot, item);
			return item;
		}

		static std::shared_ptr<Item> equipAmmunition(const std::shared_ptr<Player> &player, uint16_t ammunitionId, uint8_t count = 10) {
			auto quiver = equip(player, CONST_SLOT_RIGHT, quiverId);
			auto ammunition = Item::CreateItem(ammunitionId, count);
			quiver->getContainer()->internalAddThing(ammunition);
			return ammunition;
		}

		static std::shared_ptr<OffensiveCastContext> createContext(const Spell &spell, const std::shared_ptr<Player> &player) {
			const auto result = OffensiveCastContext::create(spell, *player);
			EXPECT_TRUE(result.success());
			EXPECT_EQ(OffensiveCastContextResult::Created, result.result);
			EXPECT_NE(nullptr, result.context);
			return result.context;
		}

		inline static size_t originalItemsSize = 0;
		inline static std::map<uint16_t, ItemType> originalItems;
		inline static std::unique_ptr<di::extension::injector<>> injector;
		inline static di::extension::injector<>* previousTestContainer = nullptr;
		inline static std::filesystem::path configFile;
		inline static std::filesystem::path disciplineFile;
		inline static std::vector<std::shared_ptr<Tile>> liveTiles;
	};
}

TEST_F(OffensiveCastContextTest, FreezesEquipmentStatsAndDamageAtCreation) {
	auto spell = makeSpell();
	auto player = makePlayer();
	auto weapon = equip(player, CONST_SLOT_LEFT, swordId);
	auto context = createContext(spell, player);
	EXPECT_EQ(OffensiveProfile::Sword, context->equipment().profile);
	EXPECT_EQ(30, context->equipmentPower());
	EXPECT_EQ(1, context->range());
	EXPECT_FALSE(context->requiresAmmunition());
	EXPECT_EQ(40, context->primaryBaseDamage());
	EXPECT_EQ(20, context->secondaryBaseDamage());
	weapon->setAttribute(ItemAttribute_t::ATTACK, 99);
	EXPECT_EQ(30, context->equipmentPower());
	EXPECT_EQ(40, context->primaryBaseDamage());
}

TEST_F(OffensiveCastContextTest, FreezesNonzeroDerivedStatsAfterRankChanges) {
	auto spell = makeSpell();
	auto player = makePlayer();
	player->setLevel(1);
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	equip(player, CONST_SLOT_LEFT, swordId);
	auto context = createContext(spell, player);
	ASSERT_EQ(5, context->physicalAttack());
	ASSERT_EQ(45, context->primaryBaseDamage());
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	EXPECT_EQ(10, player->characterStats().stat(DerivedStat::PhysicalAttack));
	EXPECT_EQ(5, context->physicalAttack());
	EXPECT_EQ(45, context->primaryBaseDamage());
}

TEST_F(OffensiveCastContextTest, RequiresAnOffensiveDefinition) {
	TestSpell spell;
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, swordId);
	const auto result = OffensiveCastContext::create(spell, *player);
	EXPECT_FALSE(result.success());
	EXPECT_EQ(OffensiveCastContextResult::MissingOffensiveDefinition, result.result);
	EXPECT_EQ("missing_offensive_definition", offensiveCastContextReason(result.result));
}

TEST_F(OffensiveCastContextTest, ReturnsUnsupportedEquipmentWithoutAContext) {
	auto spell = makeSpell();
	auto player = makePlayer();
	const auto result = OffensiveCastContext::create(spell, *player);
	EXPECT_FALSE(result.success());
	EXPECT_EQ(OffensiveCastContextResult::UnsupportedEquipment, result.result);
	EXPECT_EQ("unsupported_equipment", offensiveCastContextReason(result.result));
}

TEST_F(OffensiveCastContextTest, ReturnsMissingAmmunitionWithoutAContext) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, bowId);
	const auto result = OffensiveCastContext::create(spell, *player);
	EXPECT_FALSE(result.success());
	EXPECT_EQ(OffensiveCastContextResult::MissingAmmunition, result.result);
}

TEST_F(OffensiveCastContextTest, SwordTagsAreSortedAndComplete) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, swordId);
	auto context = createContext(spell, player);
	EXPECT_EQ(
		(std::vector<std::string_view> { "category.art", "damage.neutral", "damage.physical", "discipline.armament", "execution.attack", "execution.contact", "function.offensive", "weapon.sword" }),
		context->effectiveTags().names()
	);
}

TEST_F(OffensiveCastContextTest, AxeTagsContainAreaContactAndWeapon) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, axeId);
	auto tags = createContext(spell, player)->effectiveTags();
	EXPECT_TRUE(tags.contains(SpellTag::ExecutionArea));
	EXPECT_TRUE(tags.contains(SpellTag::ExecutionContact));
	EXPECT_TRUE(tags.contains(SpellTag::WeaponAxe));
}

TEST_F(OffensiveCastContextTest, ClubTagsContainAreaContactAndWeapon) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, clubId);
	auto tags = createContext(spell, player)->effectiveTags();
	EXPECT_TRUE(tags.contains(SpellTag::ExecutionArea));
	EXPECT_TRUE(tags.contains(SpellTag::ExecutionContact));
	EXPECT_TRUE(tags.contains(SpellTag::WeaponClub));
}

TEST_F(OffensiveCastContextTest, BowTagsContainProjectileAndWeapon) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, bowId);
	equipAmmunition(player, arrowId);
	auto tags = createContext(spell, player)->effectiveTags();
	EXPECT_TRUE(tags.contains(SpellTag::ExecutionProjectile));
	EXPECT_TRUE(tags.contains(SpellTag::WeaponBow));
}

TEST_F(OffensiveCastContextTest, CrossbowTagsContainProjectileAndWeapon) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, crossbowId);
	equipAmmunition(player, boltId);
	auto tags = createContext(spell, player)->effectiveTags();
	EXPECT_TRUE(tags.contains(SpellTag::ExecutionProjectile));
	EXPECT_TRUE(tags.contains(SpellTag::WeaponCrossbow));
}

TEST_F(OffensiveCastContextTest, ShieldKeepsContingentKnockbackTag) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, shieldId);
	auto tags = createContext(spell, player)->effectiveTags();
	EXPECT_TRUE(tags.contains(SpellTag::EquipmentShield));
	EXPECT_TRUE(tags.contains(SpellTag::FunctionControl));
	EXPECT_TRUE(tags.contains(SpellTag::MechanicKnockback));
}

TEST_F(OffensiveCastContextTest, DuplicateBaseAndProfileTagsCollapse) {
	auto spell = makeSpell({ SpellTag::ExecutionContact, SpellTag::WeaponSword });
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, swordId);
	const auto names = createContext(spell, player)->effectiveTags().names();
	EXPECT_EQ(1, std::ranges::count(names, "execution.contact"));
	EXPECT_EQ(1, std::ranges::count(names, "weapon.sword"));
}

TEST_F(OffensiveCastContextTest, RejectsMissingAndDeadPrimaryTargetsBeforeCommit) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, swordId);
	auto context = createContext(spell, player);
	EXPECT_EQ(OffensiveCastContextResult::InvalidPrimaryTarget, context->validatePrimaryTarget(nullptr).result);
	EXPECT_EQ(OffensiveCastContextResult::InvalidPrimaryTarget, context->validatePrimaryTarget(makeTarget(Position { 1001, 1000, 7 }, true, 0)).result);
	EXPECT_FALSE(context->committed());
}

TEST_F(OffensiveCastContextTest, RejectsPrimaryTargetOutsideFrozenRange) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, swordId);
	auto context = createContext(spell, player);
	EXPECT_EQ(OffensiveCastContextResult::OutOfRange, context->validatePrimaryTarget(makeTarget(Position { 1002, 1000, 7 })).result);
	EXPECT_EQ("out_of_range", offensiveCastContextReason(OffensiveCastContextResult::OutOfRange));
}

TEST_F(OffensiveCastContextTest, RejectsPrimaryTargetWithoutLineOfSight) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, bowId);
	equipAmmunition(player, arrowId);
	auto context = createContext(spell, player);
	auto target = makeTarget(Position { 1000, 1002, 7 });
	const Position blockerPosition { 1000, 1001, 7 };
	const auto previousTile = g_game().map.getTile(blockerPosition);
	auto blocker = std::make_shared<DynamicTile>(blockerPosition);
	blocker->setFlag(TILESTATE_BLOCKPROJECTILE);
	g_game().map.setTile(blockerPosition, blocker);
	EXPECT_EQ(OffensiveCastContextResult::LineOfSightBlocked, context->validatePrimaryTarget(target).result);
	g_game().map.setTile(blockerPosition, previousTile);
}

TEST_F(OffensiveCastContextTest, ReportsFundamentalProtectionZoneDenial) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, swordId);
	auto context = createContext(spell, player);
	auto target = makeTarget();
	target->getTile()->setFlag(TILESTATE_PROTECTIONZONE);
	const auto result = context->validatePrimaryTarget(target);
	EXPECT_EQ(OffensiveCastContextResult::CombatDenied, result.result);
	EXPECT_NE(RETURNVALUE_NOERROR, result.combatResult);
	EXPECT_FALSE(context->committed());
}

TEST_F(OffensiveCastContextTest, CommitRevalidatesTargetAfterSuccessfulPreflight) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, swordId);
	auto context = createContext(spell, player);
	auto target = makeTarget();
	ASSERT_TRUE(context->validatePrimaryTarget(target).success());
	placeCreature(target, Position { 1002, 1000, 7 });
	EXPECT_EQ(OffensiveCastContextResult::OutOfRange, context->commit(target).result);
	EXPECT_FALSE(context->committed());
}

TEST_F(OffensiveCastContextTest, FiltersIllegalSecondaryWithoutCancelingLegalTarget) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, axeId);
	auto context = createContext(spell, player);
	auto allowed = makeTarget(Position { 1001, 1000, 7 });
	auto denied = makeTarget(Position { 1000, 1001, 7 });
	denied->getTile()->setFlag(TILESTATE_PROTECTIONZONE);
	EXPECT_TRUE(context->canAffect(allowed));
	EXPECT_FALSE(context->canAffect(denied));
	EXPECT_FALSE(context->committed());
}

TEST_F(OffensiveCastContextTest, MeleeCommitIsSingleUseAndConsumesNoItem) {
	auto spell = makeSpell();
	auto player = makePlayer();
	auto weapon = equip(player, CONST_SLOT_LEFT, swordId);
	auto context = createContext(spell, player);
	auto target = makeTarget();
	EXPECT_TRUE(context->commit(target).success());
	EXPECT_TRUE(context->committed());
	EXPECT_EQ(weapon, player->getInventoryItem(CONST_SLOT_LEFT));
	const auto second = context->commit(target);
	EXPECT_EQ(OffensiveCastContextResult::ContextAlreadyCommitted, second.result);
	EXPECT_EQ("context_already_committed", offensiveCastContextReason(second.result));
}

TEST_F(OffensiveCastContextTest, RangedCommitConsumesExactlyOneSnapshottedUnit) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, bowId);
	auto ammunition = equipAmmunition(player, arrowId, 10);
	auto context = createContext(spell, player);
	auto target = makeTarget(Position { 1003, 1000, 7 });
	EXPECT_TRUE(context->commit(target).success());
	EXPECT_EQ(9, ammunition->getItemCount());
	EXPECT_EQ(OffensiveCastContextResult::ContextAlreadyCommitted, context->commit(target).result);
	EXPECT_EQ(9, ammunition->getItemCount());
}

TEST_F(OffensiveCastContextTest, AmmunitionMovedAfterSnapshotPreventsCommit) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, crossbowId);
	auto ammunition = equipAmmunition(player, boltId);
	auto context = createContext(spell, player);
	auto target = makeTarget(Position { 1003, 1000, 7 });
	ammunition->resetParent();
	EXPECT_EQ(OffensiveCastContextResult::MissingAmmunition, context->commit(target).result);
	EXPECT_FALSE(context->committed());
}

TEST_F(OffensiveCastContextTest, EmptyAmmunitionStackPreventsCommit) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, bowId);
	auto ammunition = equipAmmunition(player, arrowId, 1);
	auto context = createContext(spell, player);
	auto target = makeTarget(Position { 1003, 1000, 7 });
	ammunition->setItemCount(0);
	EXPECT_EQ(OffensiveCastContextResult::MissingAmmunition, context->commit(target).result);
	EXPECT_FALSE(context->committed());
}

TEST_F(OffensiveCastContextTest, CommitTouchesNoManaSoulOrTargetHealth) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, swordId);
	auto context = createContext(spell, player);
	auto target = makeTarget();
	const auto mana = player->getMana();
	const auto soul = player->getSoul();
	const auto health = target->getHealth();
	EXPECT_TRUE(context->commit(target).success());
	EXPECT_EQ(mana, player->getMana());
	EXPECT_EQ(soul, player->getSoul());
	EXPECT_EQ(health, target->getHealth());
}

TEST_F(OffensiveCastContextTest, ImmuneTargetStillAllowsAmmunitionCommit) {
	auto spell = makeSpell();
	auto player = makePlayer();
	equip(player, CONST_SLOT_LEFT, crossbowId);
	auto ammunition = equipAmmunition(player, boltId, 2);
	auto context = createContext(spell, player);
	auto target = makeTarget(Position { 1003, 1000, 7 });
	target->setImmune(true);
	ASSERT_TRUE(target->isImmune());
	EXPECT_TRUE(context->commit(target).success());
	EXPECT_EQ(1, ammunition->getItemCount());
	EXPECT_EQ(100, target->getHealth());
}
