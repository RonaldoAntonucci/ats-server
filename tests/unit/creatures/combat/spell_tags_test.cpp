/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/spells.hpp"

#include <gtest/gtest.h>

namespace {
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
}

TEST(SpellTagsTest, AcceptsAnArbitraryTagWithoutACompiledCatalog) {
	TestSpell spell;
	EXPECT_TRUE(spell.addTag("future.system.unseen-tag"));
	EXPECT_TRUE(spell.hasTag("future.system.unseen-tag"));
}

TEST(SpellTagsTest, AcceptsATagWithoutANamespace) {
	TestSpell spell;
	EXPECT_TRUE(spell.addTag("offensive"));
	EXPECT_EQ((std::vector<std::string> { "offensive" }), spell.getTags());
}

TEST(SpellTagsTest, RejectsOnlyTheEmptyStringWithoutMutation) {
	TestSpell spell;
	ASSERT_TRUE(spell.addTag("category.art"));
	EXPECT_FALSE(spell.addTag(""));
	EXPECT_EQ((std::vector<std::string> { "category.art" }), spell.getTags());
}

TEST(SpellTagsTest, CollapsesExactDuplicatesAtTheirFirstInsertion) {
	TestSpell spell;
	ASSERT_TRUE(spell.addTag("weapon.sword"));
	EXPECT_TRUE(spell.addTag("weapon.sword"));
	EXPECT_EQ((std::vector<std::string> { "weapon.sword" }), spell.getTags());
}

TEST(SpellTagsTest, PreservesFirstInsertionOrder) {
	TestSpell spell;
	ASSERT_TRUE(spell.addTag("function.offensive"));
	ASSERT_TRUE(spell.addTag("category.art"));
	ASSERT_TRUE(spell.addTag("damage.physical"));
	EXPECT_EQ((std::vector<std::string> { "function.offensive", "category.art", "damage.physical" }), spell.getTags());
}

TEST(SpellTagsTest, TreatsCaseVariantsAsDistinctTags) {
	TestSpell spell;
	ASSERT_TRUE(spell.addTag("Damage.Physical"));
	ASSERT_TRUE(spell.addTag("damage.physical"));
	EXPECT_EQ((std::vector<std::string> { "Damage.Physical", "damage.physical" }), spell.getTags());
}

TEST(SpellTagsTest, HasTagUsesExactCaseSensitiveComparison) {
	TestSpell spell;
	ASSERT_TRUE(spell.addTag("discipline.armament"));
	EXPECT_TRUE(spell.hasTag("discipline.armament"));
	EXPECT_FALSE(spell.hasTag("Discipline.Armament"));
	EXPECT_FALSE(spell.hasTag("discipline"));
}

TEST(SpellTagsTest, TagOperationsDoNotAlterSpellGroupsOrCooldowns) {
	TestSpell spell;
	spell.setGroup(SPELLGROUP_ATTACK);
	spell.setSecondaryGroup(SPELLGROUP_SUPPORT);
	spell.setCooldown(1000);
	spell.setGroupCooldown(250);
	spell.setSecondaryCooldown(500);

	ASSERT_TRUE(spell.addTag("any.tag"));
	EXPECT_TRUE(spell.hasTag("any.tag"));
	EXPECT_EQ(1u, spell.getTags().size());
	EXPECT_EQ(SPELLGROUP_ATTACK, spell.getGroup());
	EXPECT_EQ(SPELLGROUP_SUPPORT, spell.getSecondaryGroup());
	EXPECT_EQ(1000u, spell.getCooldown());
	EXPECT_EQ(250u, spell.getGroupCooldown());
	EXPECT_EQ(500u, spell.getSecondaryCooldown());
}
