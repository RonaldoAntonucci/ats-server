/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/offensive_spell_definition.hpp"

#include "creatures/combat/spells.hpp"

#include <gtest/gtest.h>

#include "lib/di/container.hpp"
#include "lib/logging/in_memory_logger.hpp"

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

	class OffensiveSpellDefinitionTest : public ::testing::Test {
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

		static OffensivePowerInputs normativeInputs() {
			return { .physicalAttack = 20, .magicalAttack = 999, .equipmentPower = 30 };
		}

		inline static std::unique_ptr<di::extension::injector<>> injector;
		inline static di::extension::injector<>* previousTestContainer = nullptr;
	};
}

TEST_F(OffensiveSpellDefinitionTest, AcceptsTheNormativeParametersAndCooldowns) {
	OffensiveSpellDefinition definition;
	const auto &parameters = definition.parameters();
	EXPECT_DOUBLE_EQ(10.0, parameters.basePower);
	EXPECT_DOUBLE_EQ(1.0, parameters.physicalCoefficient);
	EXPECT_DOUBLE_EQ(0.0, parameters.magicalCoefficient);
	EXPECT_DOUBLE_EQ(1.0, parameters.equipmentCoefficient);
	EXPECT_DOUBLE_EQ(0.5, parameters.secondaryMultiplier);
	EXPECT_EQ(1000u, parameters.cooldownMilliseconds);
}

TEST_F(OffensiveSpellDefinitionTest, CalculatesTheExactNormativePowerFormula) {
	OffensiveSpellDefinition definition;
	EXPECT_EQ(60, definition.calculateBaseDamage(normativeInputs(), OffensiveTarget::Primary, "Synthetic Strike", 77));
}

TEST_F(OffensiveSpellDefinitionTest, IncludesMagicalAttackOnlyThroughItsCoefficient) {
	OffensiveSpellDefinition definition;
	auto parameters = definition.parameters();
	parameters.magicalCoefficient = 0.25;
	ASSERT_EQ(OffensiveDefinitionUpdateResult::Updated, definition.setParameters(parameters, "Synthetic Strike"));
	EXPECT_EQ(309, definition.calculateBaseDamage(normativeInputs(), OffensiveTarget::Primary, "Synthetic Strike", 77));
}

TEST_F(OffensiveSpellDefinitionTest, AppliesOneFinalFloorAfterEveryTermAndTargetMultiplier) {
	OffensiveSpellDefinition definition;
	OffensivePowerParameters parameters {};
	parameters.basePower = 0.9;
	parameters.physicalCoefficient = 0.2;
	parameters.magicalCoefficient = 0;
	parameters.equipmentCoefficient = 0;
	parameters.secondaryMultiplier = 0.5;
	ASSERT_EQ(OffensiveDefinitionUpdateResult::Updated, definition.setParameters(parameters, "Synthetic Strike"));
	const OffensivePowerInputs inputs { .physicalAttack = 1, .magicalAttack = 0, .equipmentPower = 0 };
	EXPECT_EQ(1, definition.calculateBaseDamage(inputs, OffensiveTarget::Primary, "Synthetic Strike", 77));
	EXPECT_EQ(0, definition.calculateBaseDamage(inputs, OffensiveTarget::Secondary, "Synthetic Strike", 77));
}

TEST_F(OffensiveSpellDefinitionTest, PrimaryAlwaysUsesMultiplierOne) {
	OffensiveSpellDefinition definition;
	auto parameters = definition.parameters();
	parameters.secondaryMultiplier = 0;
	ASSERT_EQ(OffensiveDefinitionUpdateResult::Updated, definition.setParameters(parameters, "Synthetic Strike"));
	EXPECT_EQ(60, definition.calculateBaseDamage(normativeInputs(), OffensiveTarget::Primary, "Synthetic Strike", 77));
}

TEST_F(OffensiveSpellDefinitionTest, SecondaryUsesTheConfiguredMultiplier) {
	OffensiveSpellDefinition definition;
	EXPECT_EQ(30, definition.calculateBaseDamage(normativeInputs(), OffensiveTarget::Secondary, "Synthetic Strike", 77));
}

TEST_F(OffensiveSpellDefinitionTest, SecondaryAcceptsBothMultiplierBoundaries) {
	OffensiveSpellDefinition definition;
	ASSERT_EQ(OffensiveDefinitionUpdateResult::Updated, definition.setParameter(OffensiveParameterField::SecondaryMultiplier, 0.0, "Synthetic Strike"));
	EXPECT_EQ(0, definition.calculateBaseDamage(normativeInputs(), OffensiveTarget::Secondary, "Synthetic Strike", 77));
	ASSERT_EQ(OffensiveDefinitionUpdateResult::Updated, definition.setParameter(OffensiveParameterField::SecondaryMultiplier, 1.0, "Synthetic Strike"));
	EXPECT_EQ(60, definition.calculateBaseDamage(normativeInputs(), OffensiveTarget::Secondary, "Synthetic Strike", 77));
}

TEST_F(OffensiveSpellDefinitionTest, ProducesDeterministicDamageWithoutOwnedRng) {
	OffensiveSpellDefinition definition;
	for (size_t iteration = 0; iteration < 32; ++iteration) {
		EXPECT_EQ(60, definition.calculateBaseDamage(normativeInputs(), OffensiveTarget::Primary, "Synthetic Strike", 77));
	}
}

TEST_F(OffensiveSpellDefinitionTest, RejectsWrongParameterTypesWithFieldAndReason) {
	OffensiveSpellDefinition definition;
	EXPECT_EQ(
		OffensiveDefinitionUpdateResult::WrongType,
		definition.setParameter(OffensiveParameterField::BasePower, std::string { "ten" }, "Synthetic Strike")
	);
	ASSERT_EQ(1u, logger().logCount());
	const auto [level, message] = logger().getLogEntry(0);
	EXPECT_EQ("error", level);
	EXPECT_NE(std::string::npos, message.find("spell=Synthetic Strike"));
	EXPECT_NE(std::string::npos, message.find("field=basePower"));
	EXPECT_NE(std::string::npos, message.find("reason=wrong_type"));
	EXPECT_EQ(
		OffensiveDefinitionUpdateResult::WrongType,
		definition.setParameter(OffensiveParameterField::CooldownMilliseconds, true, "Synthetic Strike")
	);
}

TEST_F(OffensiveSpellDefinitionTest, RejectsNonFiniteParameters) {
	OffensiveSpellDefinition definition;
	EXPECT_EQ(OffensiveDefinitionUpdateResult::NonFinite, definition.setParameter(OffensiveParameterField::PhysicalCoefficient, std::numeric_limits<double>::quiet_NaN(), "Synthetic Strike"));
	EXPECT_EQ(OffensiveDefinitionUpdateResult::NonFinite, definition.setParameter(OffensiveParameterField::MagicalCoefficient, std::numeric_limits<double>::infinity(), "Synthetic Strike"));
	EXPECT_DOUBLE_EQ(1.0, definition.parameters().physicalCoefficient);
	EXPECT_DOUBLE_EQ(0.0, definition.parameters().magicalCoefficient);
}

TEST_F(OffensiveSpellDefinitionTest, RejectsAnInvalidParameterSetAtomically) {
	OffensiveSpellDefinition definition;
	auto parameters = definition.parameters();
	parameters.basePower = 100;
	parameters.equipmentCoefficient = std::numeric_limits<double>::quiet_NaN();
	EXPECT_EQ(OffensiveDefinitionUpdateResult::NonFinite, definition.setParameters(parameters, "Synthetic Strike"));
	EXPECT_DOUBLE_EQ(10.0, definition.parameters().basePower);
	EXPECT_DOUBLE_EQ(1.0, definition.parameters().equipmentCoefficient);
	ASSERT_EQ(1u, logger().logCount());
	const auto [level, message] = logger().getLogEntry(0);
	EXPECT_EQ("error", level);
	EXPECT_NE(std::string::npos, message.find("field=equipmentCoefficient"));
	EXPECT_NE(std::string::npos, message.find("reason=non_finite"));
}

TEST_F(OffensiveSpellDefinitionTest, RejectsNegativePowerTermsWithoutPartialMutation) {
	OffensiveSpellDefinition definition;
	for (const auto field : { OffensiveParameterField::BasePower, OffensiveParameterField::PhysicalCoefficient, OffensiveParameterField::MagicalCoefficient, OffensiveParameterField::EquipmentCoefficient }) {
		EXPECT_EQ(OffensiveDefinitionUpdateResult::BelowMinimum, definition.setParameter(field, -1.0, "Synthetic Strike"));
	}
	EXPECT_DOUBLE_EQ(10.0, definition.parameters().basePower);
	EXPECT_DOUBLE_EQ(1.0, definition.parameters().physicalCoefficient);
	EXPECT_DOUBLE_EQ(0.0, definition.parameters().magicalCoefficient);
	EXPECT_DOUBLE_EQ(1.0, definition.parameters().equipmentCoefficient);
}

TEST_F(OffensiveSpellDefinitionTest, RejectsSecondaryMultiplierOutsideItsInclusiveBounds) {
	OffensiveSpellDefinition definition;
	EXPECT_EQ(OffensiveDefinitionUpdateResult::BelowMinimum, definition.setParameter(OffensiveParameterField::SecondaryMultiplier, -0.01, "Synthetic Strike"));
	EXPECT_EQ(OffensiveDefinitionUpdateResult::AboveMaximum, definition.setParameter(OffensiveParameterField::SecondaryMultiplier, 1.01, "Synthetic Strike"));
	EXPECT_DOUBLE_EQ(0.5, definition.parameters().secondaryMultiplier);
}

TEST_F(OffensiveSpellDefinitionTest, RejectsFractionalNegativeAndOverflowingCooldowns) {
	OffensiveSpellDefinition definition;
	EXPECT_EQ(OffensiveDefinitionUpdateResult::NotInteger, definition.setParameter(OffensiveParameterField::CooldownMilliseconds, 999.5, "Synthetic Strike"));
	EXPECT_EQ(OffensiveDefinitionUpdateResult::BelowMinimum, definition.setParameter(OffensiveParameterField::CooldownMilliseconds, -1.0, "Synthetic Strike"));
	EXPECT_EQ(OffensiveDefinitionUpdateResult::AboveMaximum, definition.setParameter(OffensiveParameterField::CooldownMilliseconds, static_cast<double>(std::numeric_limits<uint32_t>::max()) + 1.0, "Synthetic Strike"));
	EXPECT_EQ(1000u, definition.parameters().cooldownMilliseconds);
}

TEST_F(OffensiveSpellDefinitionTest, ClampsNegativeResultsToZero) {
	OffensiveSpellDefinition definition;
	const OffensivePowerInputs inputs { .physicalAttack = 0, .magicalAttack = 0, .equipmentPower = -100 };
	EXPECT_EQ(0, definition.calculateBaseDamage(inputs, OffensiveTarget::Primary, "Synthetic Strike", 77));
}

TEST_F(OffensiveSpellDefinitionTest, ClampsOverflowAndLogsSpellCasterAndOriginalValue) {
	OffensiveSpellDefinition definition;
	const OffensivePowerInputs inputs { .physicalAttack = std::numeric_limits<uint64_t>::max(), .magicalAttack = 0, .equipmentPower = 0 };
	EXPECT_EQ(std::numeric_limits<int32_t>::max(), definition.calculateBaseDamage(inputs, OffensiveTarget::Primary, "Synthetic Strike", 77));
	ASSERT_EQ(1u, logger().logCount());
	const auto [level, message] = logger().getLogEntry(0);
	EXPECT_EQ("warning", level);
	EXPECT_NE(std::string::npos, message.find("spell=Synthetic Strike"));
	EXPECT_NE(std::string::npos, message.find("caster=77"));
	EXPECT_NE(std::string::npos, message.find("original="));
	EXPECT_NE(std::string::npos, message.find("reason=overflow"));
}

TEST_F(OffensiveSpellDefinitionTest, FreezePreventsFurtherMutation) {
	OffensiveSpellDefinition definition;
	definition.freeze();
	EXPECT_TRUE(definition.frozen());
	EXPECT_EQ(OffensiveDefinitionUpdateResult::Frozen, definition.setParameter(OffensiveParameterField::BasePower, 20.0, "Synthetic Strike"));
	EXPECT_EQ(OffensiveDefinitionUpdateResult::Frozen, definition.setBaseTags(normalizeSpellTags({ SpellTag::CategoryArt })));
	EXPECT_DOUBLE_EQ(10.0, definition.parameters().basePower);
}

TEST_F(OffensiveSpellDefinitionTest, TagsNeverDeriveOrAlterCooldownValues) {
	TestSpell spell;
	auto &definition = spell.getOrCreateOffensiveDefinition();
	spell.setCooldown(definition.parameters().cooldownMilliseconds);
	spell.setGroupCooldown(0);
	ASSERT_EQ(OffensiveDefinitionUpdateResult::Updated, definition.setBaseTags(normalizeSpellTags({ SpellTag::ExecutionAttack, SpellTag::FunctionOffensive })));
	EXPECT_EQ(1000u, spell.getCooldown());
	EXPECT_EQ(0u, spell.getGroupCooldown());
}

TEST_F(OffensiveSpellDefinitionTest, SpellOwnsTheDefinitionWithoutAParallelRegistry) {
	TestSpell spell;
	EXPECT_FALSE(spell.getOffensiveDefinition().has_value());
	auto &definition = spell.getOrCreateOffensiveDefinition();
	EXPECT_EQ(&definition, &spell.getOffensiveDefinition().value());
}

TEST_F(OffensiveSpellDefinitionTest, FormulaInputsExposeNoDefensePrecisionOrPenetration) {
	static_assert(std::is_aggregate_v<OffensivePowerInputs>);
	static_assert(std::is_same_v<decltype(OffensivePowerInputs::physicalAttack), uint64_t>);
	static_assert(std::is_same_v<decltype(OffensivePowerInputs::magicalAttack), uint64_t>);
	static_assert(std::is_same_v<decltype(OffensivePowerInputs::equipmentPower), int32_t>);
	SUCCEED();
}
