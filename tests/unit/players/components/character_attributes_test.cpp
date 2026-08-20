/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/player.hpp"
#include "kv/kv.hpp"

#include "injection_fixture.hpp"

class CharacterAttributesTest : public ::testing::Test {
protected:
	void SetUp() override {
		kv = &fixture.kv();
		player = std::make_shared<Player>();
		player->setGUID(kPlayerGuid);
	}

	void setAllocation(const ValueWrapper &allocation) const {
		player->kv()->scoped("character-attributes")->set("allocation", allocation);
	}

	static constexpr uint32_t kPlayerGuid = 1001;
	InjectionFixture fixture {};
	KVMemory* kv = nullptr;
	std::shared_ptr<Player> player;
};

TEST_F(CharacterAttributesTest, StartsWithZeroAttributesAndFullBudget) {
	const auto snapshot = player->characterAttributes().snapshot();

	EXPECT_TRUE(snapshot.valid);
	EXPECT_EQ(0, snapshot.values.strength);
	EXPECT_EQ(0, snapshot.values.dexterity);
	EXPECT_EQ(0, snapshot.values.vitality);
	EXPECT_EQ(0, snapshot.values.intelligence);
	EXPECT_EQ(0, snapshot.values.willpower);
	EXPECT_EQ(100, snapshot.freePoints);
}

TEST_F(CharacterAttributesTest, AddsEveryAttributeAndStoresOneCompleteMap) {
	auto &attributes = player->characterAttributes();

	EXPECT_EQ(CharacterAttributeAddResult::Success, attributes.add("str", 10));
	EXPECT_EQ(CharacterAttributeAddResult::Success, attributes.add("dex", 20));
	EXPECT_EQ(CharacterAttributeAddResult::Success, attributes.add("vit", 30));
	EXPECT_EQ(CharacterAttributeAddResult::Success, attributes.add("int", 15));
	EXPECT_EQ(CharacterAttributeAddResult::Success, attributes.add("wil", 5));

	const auto snapshot = attributes.snapshot();
	EXPECT_TRUE(snapshot.valid);
	EXPECT_EQ(10, snapshot.values.strength);
	EXPECT_EQ(20, snapshot.values.dexterity);
	EXPECT_EQ(30, snapshot.values.vitality);
	EXPECT_EQ(15, snapshot.values.intelligence);
	EXPECT_EQ(5, snapshot.values.willpower);
	EXPECT_EQ(20, snapshot.freePoints);

	const auto stored = kv->get("player.1001.character-attributes.allocation");
	ASSERT_TRUE(stored.has_value());
	ASSERT_TRUE(std::holds_alternative<MapType>(stored->getVariant()));
	const auto map = stored->get<MapType>();
	EXPECT_EQ(5, map.size());
	EXPECT_EQ(10, map.at("strength")->get<IntType>());
	EXPECT_EQ(20, map.at("dexterity")->get<IntType>());
	EXPECT_EQ(30, map.at("vitality")->get<IntType>());
	EXPECT_EQ(15, map.at("intelligence")->get<IntType>());
	EXPECT_EQ(5, map.at("willpower")->get<IntType>());
}

TEST_F(CharacterAttributesTest, RejectsInvalidAdditionWithoutMutation) {
	auto &attributes = player->characterAttributes();
	ASSERT_EQ(CharacterAttributeAddResult::Success, attributes.add("str", 40));
	const auto before = attributes.snapshot();

	EXPECT_EQ(CharacterAttributeAddResult::UnknownAttribute, attributes.add("luck", 10));
	EXPECT_EQ(CharacterAttributeAddResult::InvalidAmount, attributes.add("str", 0));
	EXPECT_EQ(CharacterAttributeAddResult::InsufficientPoints, attributes.add("dex", 61));

	const auto after = attributes.snapshot();
	EXPECT_EQ(before.values, after.values);
	EXPECT_EQ(before.freePoints, after.freePoints);
}

TEST_F(CharacterAttributesTest, RejectsAdditionWhenBudgetIsExhausted) {
	auto &attributes = player->characterAttributes();
	ASSERT_EQ(CharacterAttributeAddResult::Success, attributes.add("str", 100));

	EXPECT_EQ(CharacterAttributeAddResult::InsufficientPoints, attributes.add("dex", 1));
	const auto snapshot = attributes.snapshot();
	EXPECT_EQ(100, snapshot.values.strength);
	EXPECT_EQ(0, snapshot.values.dexterity);
	EXPECT_EQ(0, snapshot.freePoints);
}

TEST_F(CharacterAttributesTest, ResetIsTotalAndIdempotent) {
	auto &attributes = player->characterAttributes();
	ASSERT_EQ(CharacterAttributeAddResult::Success, attributes.add("str", 40));
	ASSERT_EQ(CharacterAttributeAddResult::Success, attributes.add("vit", 35));

	attributes.reset();
	auto snapshot = attributes.snapshot();
	EXPECT_EQ(CharacterAttributeValues {}, snapshot.values);
	EXPECT_EQ(100, snapshot.freePoints);
	EXPECT_TRUE(snapshot.valid);

	attributes.reset();
	snapshot = attributes.snapshot();
	EXPECT_EQ(CharacterAttributeValues {}, snapshot.values);
	EXPECT_EQ(100, snapshot.freePoints);
	EXPECT_TRUE(snapshot.valid);
}

TEST_F(CharacterAttributesTest, KeepsAllocationAcrossPlayerInstancesAndIsolatesGuids) {
	ASSERT_EQ(CharacterAttributeAddResult::Success, player->characterAttributes().add("wil", 55));

	auto sameCharacter = std::make_shared<Player>();
	sameCharacter->setGUID(kPlayerGuid);
	EXPECT_EQ(55, sameCharacter->characterAttributes().snapshot().values.willpower);
	EXPECT_EQ(45, sameCharacter->characterAttributes().snapshot().freePoints);

	auto otherCharacter = std::make_shared<Player>();
	otherCharacter->setGUID(kPlayerGuid + 1);
	EXPECT_EQ(CharacterAttributeValues {}, otherCharacter->characterAttributes().snapshot().values);
	EXPECT_EQ(100, otherCharacter->characterAttributes().snapshot().freePoints);
}

TEST_F(CharacterAttributesTest, LevelChangesDoNotChangeAllocationOrBudget) {
	auto &attributes = player->characterAttributes();
	ASSERT_EQ(CharacterAttributeAddResult::Success, attributes.add("int", 65));

	player->setLevel(200);
	auto snapshot = attributes.snapshot();
	EXPECT_EQ(65, snapshot.values.intelligence);
	EXPECT_EQ(35, snapshot.freePoints);

	player->setLevel(1);
	snapshot = attributes.snapshot();
	EXPECT_EQ(65, snapshot.values.intelligence);
	EXPECT_EQ(35, snapshot.freePoints);
}

TEST_F(CharacterAttributesTest, RejectsMapWithMissingFieldUntilReset) {
	setAllocation(ValueWrapper { { "strength", 10 }, { "dexterity", 20 }, { "vitality", 30 }, { "intelligence", 15 } });

	EXPECT_FALSE(player->characterAttributes().snapshot().valid);
	EXPECT_EQ(CharacterAttributeAddResult::InvalidState, player->characterAttributes().add("wil", 5));

	player->characterAttributes().reset();
	EXPECT_TRUE(player->characterAttributes().snapshot().valid);
	EXPECT_EQ(100, player->characterAttributes().snapshot().freePoints);
}

TEST_F(CharacterAttributesTest, RejectsMapWithWrongFieldTypeUntilReset) {
	setAllocation(ValueWrapper { { "strength", std::string("10") }, { "dexterity", 20 }, { "vitality", 30 }, { "intelligence", 15 }, { "willpower", 5 } });

	EXPECT_FALSE(player->characterAttributes().snapshot().valid);
	EXPECT_EQ(CharacterAttributeAddResult::InvalidState, player->characterAttributes().add("wil", 5));

	player->characterAttributes().reset();
	EXPECT_TRUE(player->characterAttributes().snapshot().valid);
}

TEST_F(CharacterAttributesTest, RejectsMapWithOutOfRangeFieldUntilReset) {
	setAllocation(ValueWrapper { { "strength", 101 }, { "dexterity", 0 }, { "vitality", 0 }, { "intelligence", 0 }, { "willpower", 0 } });

	EXPECT_FALSE(player->characterAttributes().snapshot().valid);
	EXPECT_EQ(CharacterAttributeAddResult::InvalidState, player->characterAttributes().add("str", 1));

	player->characterAttributes().reset();
	EXPECT_TRUE(player->characterAttributes().snapshot().valid);
}

TEST_F(CharacterAttributesTest, RejectsMapWhoseSumExceedsBudgetUntilReset) {
	setAllocation(ValueWrapper { { "strength", 60 }, { "dexterity", 50 }, { "vitality", 0 }, { "intelligence", 0 }, { "willpower", 0 } });

	EXPECT_FALSE(player->characterAttributes().snapshot().valid);
	EXPECT_EQ(CharacterAttributeAddResult::InvalidState, player->characterAttributes().add("vit", 1));

	player->characterAttributes().reset();
	EXPECT_TRUE(player->characterAttributes().snapshot().valid);
}
