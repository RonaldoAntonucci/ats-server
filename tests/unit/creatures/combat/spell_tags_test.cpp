/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/spell_tags.hpp"

#include "creatures/combat/spells.hpp"

#include <gtest/gtest.h>

#include "lib/di/container.hpp"
#include "lib/logging/in_memory_logger.hpp"

namespace {
	constexpr std::array<std::string_view, 17> approvedTags = {
		"category.art",
		"damage.neutral",
		"damage.physical",
		"discipline.armament",
		"equipment.shield",
		"execution.area",
		"execution.attack",
		"execution.contact",
		"execution.projectile",
		"function.control",
		"function.offensive",
		"mechanic.knockback",
		"weapon.axe",
		"weapon.bow",
		"weapon.club",
		"weapon.crossbow",
		"weapon.sword",
	};

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

	class SpellTagsTest : public ::testing::Test {
	protected:
		static void SetUpTestSuite() {
			previousTestContainer = DI::getTestContainer();
			injector = std::make_unique<di::extension::injector<>>();
			InMemoryLogger::install(*injector);
			DI::setTestContainer(injector.get());
		}

		static void TearDownTestSuite() {
			DI::setTestContainer(previousTestContainer);
			injector.reset();
		}

		void SetUp() override {
			logger().reset();
		}

		static InMemoryLogger &logger() {
			return dynamic_cast<InMemoryLogger &>(injector->create<Logger &>());
		}

		void expectRejected(std::string_view tag, SpellTagValidationResult expectedResult, std::string_view expectedReason) const {
			const std::array names = { tag };
			const auto result = buildSpellTagSet(names);
			EXPECT_FALSE(result.success());
			EXPECT_FALSE(result.tags.has_value());
			EXPECT_EQ(expectedResult, result.result);
			EXPECT_EQ(tag, result.rejectedTag);
			ASSERT_EQ(1u, logger().logCount());
			const auto [level, message] = logger().getLogEntry(0);
			EXPECT_EQ("error", level);
			EXPECT_NE(std::string::npos, message.find("tag=" + std::string(tag)));
			EXPECT_NE(std::string::npos, message.find("reason=" + std::string(expectedReason)));
		}

		inline static std::unique_ptr<di::extension::injector<>> injector;
		inline static di::extension::injector<>* previousTestContainer = nullptr;
	};
}

TEST_F(SpellTagsTest, AcceptsExactlyTheClosedCatalog) {
	for (const auto name : approvedTags) {
		EXPECT_TRUE(parseSpellTag(name).has_value()) << name;
	}
	EXPECT_FALSE(parseSpellTag("category.spell").has_value());
	EXPECT_EQ(approvedTags.size(), spellTagCount);
}

TEST_F(SpellTagsTest, RoundTripsEveryCanonicalName) {
	for (const auto name : approvedTags) {
		const auto tag = parseSpellTag(name);
		ASSERT_TRUE(tag.has_value()) << name;
		EXPECT_EQ(name, canonicalSpellTagName(*tag));
	}
}

TEST_F(SpellTagsTest, BuildsTheExactNormativeBaseTagSet) {
	const std::array<std::string_view, 6> names = {
		"function.offensive",
		"discipline.armament",
		"execution.attack",
		"damage.physical",
		"category.art",
		"damage.neutral",
	};
	const auto result = buildSpellTagSet(names);
	ASSERT_TRUE(result.success());
	EXPECT_EQ(
		(std::vector<std::string_view> { "category.art", "damage.neutral", "damage.physical", "discipline.armament", "execution.attack", "function.offensive" }),
		result.tags->names()
	);
}

TEST_F(SpellTagsTest, RejectsAnEmptyTagWithStableReason) {
	expectRejected("", SpellTagValidationResult::EmptyTag, "empty_tag");
}

TEST_F(SpellTagsTest, RejectsANamespacelessTagWithStableReason) {
	expectRejected("offensive", SpellTagValidationResult::MissingNamespace, "missing_namespace");
}

TEST_F(SpellTagsTest, RejectsAnUnsupportedTagWithStableReason) {
	expectRejected("function.healing", SpellTagValidationResult::UnsupportedTag, "unsupported_tag");
}

TEST_F(SpellTagsTest, DoesNotPublishAPartialSetAfterInvalidInput) {
	const std::array<std::string_view, 3> names = { "function.offensive", "function.invalid", "weapon.sword" };
	const auto result = buildSpellTagSet(names);
	EXPECT_FALSE(result.success());
	EXPECT_FALSE(result.tags.has_value());
	EXPECT_EQ("function.invalid", result.rejectedTag);
}

TEST_F(SpellTagsTest, CollapsesDuplicateTags) {
	const std::array<std::string_view, 3> names = { "weapon.sword", "damage.physical", "weapon.sword" };
	const auto result = buildSpellTagSet(names);
	ASSERT_TRUE(result.success());
	EXPECT_EQ((std::vector<std::string_view> { "damage.physical", "weapon.sword" }), result.tags->names());
}

TEST_F(SpellTagsTest, ExposesTagsInDeterministicLexicographicOrder) {
	const std::array<std::string_view, 4> names = { "weapon.club", "category.art", "execution.attack", "damage.neutral" };
	const auto result = buildSpellTagSet(names);
	ASSERT_TRUE(result.success());
	EXPECT_EQ((std::vector<std::string_view> { "category.art", "damage.neutral", "execution.attack", "weapon.club" }), result.tags->names());
}

TEST_F(SpellTagsTest, NormalizesTypedTagsWithTheSameOrderingAndDeduplication) {
	const auto tags = normalizeSpellTags({ SpellTag::WeaponSword, SpellTag::CategoryArt, SpellTag::WeaponSword });
	EXPECT_EQ((std::vector<std::string_view> { "category.art", "weapon.sword" }), tags.names());
}

TEST_F(SpellTagsTest, TagEvaluationDoesNotAlterSpellGroupsOrCooldowns) {
	TestSpell spell;
	spell.setGroup(SPELLGROUP_ATTACK);
	spell.setSecondaryGroup(SPELLGROUP_SUPPORT);
	spell.setCooldown(1000);
	spell.setGroupCooldown(250);
	spell.setSecondaryCooldown(500);

	const auto result = buildSpellTagSet(approvedTags);
	ASSERT_TRUE(result.success());
	EXPECT_EQ(SPELLGROUP_ATTACK, spell.getGroup());
	EXPECT_EQ(SPELLGROUP_SUPPORT, spell.getSecondaryGroup());
	EXPECT_EQ(1000u, spell.getCooldown());
	EXPECT_EQ(250u, spell.getGroupCooldown());
	EXPECT_EQ(500u, spell.getSecondaryCooldown());
}
