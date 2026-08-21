/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/stats/character_stats.hpp"

#include <gtest/gtest.h>

namespace {
	[[nodiscard]] constexpr size_t index(CharacterAttribute attribute) {
		return static_cast<size_t>(attribute);
	}

	[[nodiscard]] constexpr size_t index(DerivedStat stat) {
		return static_cast<size_t>(stat);
	}

	[[nodiscard]] uint64_t total(const DerivedStatCalculation &calculation, DerivedStat stat) {
		return calculation.totals.at(index(stat));
	}

	[[nodiscard]] bool saturated(const DerivedStatCalculation &calculation, DerivedStat stat) {
		return calculation.saturated.at(index(stat));
	}
} // namespace

TEST(CharacterStatsTest, ExposesApprovedAttributeIdentifiersInOrder) {
	EXPECT_EQ("pot", characterAttributeId(CharacterAttribute::Potency));
	EXPECT_EQ("tec", characterAttributeId(CharacterAttribute::Technique));
	EXPECT_EQ("vig", characterAttributeId(CharacterAttribute::Vigor));
	EXPECT_EQ("sin", characterAttributeId(CharacterAttribute::Attunement));
	EXPECT_EQ("esp", characterAttributeId(CharacterAttribute::Spirit));
}

TEST(CharacterStatsTest, ExposesStableDerivedStatIdentifiersInOrder) {
	EXPECT_EQ("physicalAttack", derivedStatId(DerivedStat::PhysicalAttack));
	EXPECT_EQ("magicalAttack", derivedStatId(DerivedStat::MagicalAttack));
	EXPECT_EQ("precision", derivedStatId(DerivedStat::Precision));
	EXPECT_EQ("physicalDefense", derivedStatId(DerivedStat::PhysicalDefense));
	EXPECT_EQ("magicalDefense", derivedStatId(DerivedStat::MagicalDefense));
	EXPECT_EQ("maximumHealth", derivedStatId(DerivedStat::MaximumHealth));
	EXPECT_EQ("maximumMana", derivedStatId(DerivedStat::MaximumMana));
}

TEST(CharacterStatsTest, UsesTheNineApprovedDefaultMultipliers) {
	const DerivedStatMultipliers multipliers;
	EXPECT_DOUBLE_EQ(1.0, multipliers.potToPhysicalAttack);
	EXPECT_DOUBLE_EQ(0.3, multipliers.potToPhysicalDefense);
	EXPECT_DOUBLE_EQ(1.0, multipliers.tecToPrecision);
	EXPECT_DOUBLE_EQ(5.0, multipliers.vigToMaximumHealth);
	EXPECT_DOUBLE_EQ(0.7, multipliers.vigToPhysicalDefense);
	EXPECT_DOUBLE_EQ(1.0, multipliers.sinToMagicalAttack);
	EXPECT_DOUBLE_EQ(0.3, multipliers.sinToMagicalDefense);
	EXPECT_DOUBLE_EQ(5.0, multipliers.espToMaximumMana);
	EXPECT_DOUBLE_EQ(0.7, multipliers.espToMagicalDefense);
}

TEST(CharacterStatsTest, CalculatesDefaultPhysicalAttack) {
	const auto result = calculateDerivedStats(AttributeTotals { 10, 0, 0, 0, 0 }, {});
	EXPECT_EQ(10u, total(result, DerivedStat::PhysicalAttack));
}

TEST(CharacterStatsTest, CalculatesDefaultMagicalAttack) {
	const auto result = calculateDerivedStats(AttributeTotals { 0, 0, 0, 10, 0 }, {});
	EXPECT_EQ(10u, total(result, DerivedStat::MagicalAttack));
}

TEST(CharacterStatsTest, CalculatesDefaultPrecision) {
	const auto result = calculateDerivedStats(AttributeTotals { 0, 10, 0, 0, 0 }, {});
	EXPECT_EQ(10u, total(result, DerivedStat::Precision));
}

TEST(CharacterStatsTest, CalculatesDefaultPhysicalDefenseFromBothContributions) {
	const auto result = calculateDerivedStats(AttributeTotals { 10, 0, 10, 0, 0 }, {});
	EXPECT_EQ(10u, total(result, DerivedStat::PhysicalDefense));
}

TEST(CharacterStatsTest, CalculatesDefaultMagicalDefenseFromBothContributions) {
	const auto result = calculateDerivedStats(AttributeTotals { 0, 0, 0, 10, 10 }, {});
	EXPECT_EQ(10u, total(result, DerivedStat::MagicalDefense));
}

TEST(CharacterStatsTest, CalculatesDefaultMaximumHealth) {
	const auto result = calculateDerivedStats(AttributeTotals { 0, 0, 10, 0, 0 }, {});
	EXPECT_EQ(50u, total(result, DerivedStat::MaximumHealth));
}

TEST(CharacterStatsTest, CalculatesDefaultMaximumMana) {
	const auto result = calculateDerivedStats(AttributeTotals { 0, 0, 0, 0, 10 }, {});
	EXPECT_EQ(50u, total(result, DerivedStat::MaximumMana));
}

TEST(CharacterStatsTest, ReturnsEveryStatAsZeroForZeroAttributes) {
	const auto result = calculateDerivedStats({}, {});
	EXPECT_EQ(DerivedStatTotals {}, result.totals);
}

TEST(CharacterStatsTest, AppliesCustomMultipliersIndependently) {
	DerivedStatMultipliers multipliers;
	multipliers.potToPhysicalAttack = 2.0;
	multipliers.tecToPrecision = 3.0;
	multipliers.vigToMaximumHealth = 4.0;
	const auto result = calculateDerivedStats(AttributeTotals { 2, 3, 4, 0, 0 }, multipliers);
	EXPECT_EQ(4u, total(result, DerivedStat::PhysicalAttack));
	EXPECT_EQ(9u, total(result, DerivedStat::Precision));
	EXPECT_EQ(16u, total(result, DerivedStat::MaximumHealth));
}

TEST(CharacterStatsTest, ZeroMultiplierDisablesOnlyItsContribution) {
	DerivedStatMultipliers multipliers;
	multipliers.potToPhysicalAttack = 0.0;
	const auto result = calculateDerivedStats(AttributeTotals { 10, 10, 10, 0, 0 }, multipliers);
	EXPECT_EQ(0u, total(result, DerivedStat::PhysicalAttack));
	EXPECT_EQ(10u, total(result, DerivedStat::Precision));
	EXPECT_EQ(50u, total(result, DerivedStat::MaximumHealth));
}

TEST(CharacterStatsTest, RoundsOnePositiveFractionUp) {
	DerivedStatMultipliers multipliers;
	multipliers.potToPhysicalAttack = 0.01;
	const auto result = calculateDerivedStats(AttributeTotals { 1, 0, 0, 0, 0 }, multipliers);
	EXPECT_EQ(1u, total(result, DerivedStat::PhysicalAttack));
}

TEST(CharacterStatsTest, DoesNotRoundAnExactIntegerPastItsValue) {
	DerivedStatMultipliers multipliers;
	multipliers.potToPhysicalAttack = 0.5;
	const auto result = calculateDerivedStats(AttributeTotals { 2, 0, 0, 0, 0 }, multipliers);
	EXPECT_EQ(1u, total(result, DerivedStat::PhysicalAttack));
}

TEST(CharacterStatsTest, RoundsPhysicalDefenseOnlyAfterSummingFractions) {
	DerivedStatMultipliers multipliers;
	multipliers.potToPhysicalDefense = 0.3;
	multipliers.vigToPhysicalDefense = 0.3;
	const auto result = calculateDerivedStats(AttributeTotals { 1, 0, 1, 0, 0 }, multipliers);
	EXPECT_EQ(1u, total(result, DerivedStat::PhysicalDefense));
}

TEST(CharacterStatsTest, RoundsMagicalDefenseOnlyAfterSummingFractions) {
	DerivedStatMultipliers multipliers;
	multipliers.sinToMagicalDefense = 0.3;
	multipliers.espToMagicalDefense = 0.3;
	const auto result = calculateDerivedStats(AttributeTotals { 0, 0, 0, 1, 1 }, multipliers);
	EXPECT_EQ(1u, total(result, DerivedStat::MagicalDefense));
}

TEST(CharacterStatsTest, PreservesTheMaximumPublicValueExactly) {
	const auto result = calculateDerivedStats(AttributeTotals { maxPublicDerivedStat, 0, 0, 0, 0 }, {});
	EXPECT_EQ(maxPublicDerivedStat, total(result, DerivedStat::PhysicalAttack));
	EXPECT_FALSE(saturated(result, DerivedStat::PhysicalAttack));
}

TEST(CharacterStatsTest, SaturatesAValueAboveThePublicMaximum) {
	const auto result = calculateDerivedStats(AttributeTotals { maxPublicDerivedStat + 1, 0, 0, 0, 0 }, {});
	EXPECT_EQ(maxPublicDerivedStat, total(result, DerivedStat::PhysicalAttack));
	EXPECT_TRUE(saturated(result, DerivedStat::PhysicalAttack));
}

TEST(CharacterStatsTest, MarksOnlyTheStatusThatSaturated) {
	DerivedStatMultipliers multipliers;
	multipliers.potToPhysicalAttack = 2.0;
	const auto result = calculateDerivedStats(AttributeTotals { maxPublicDerivedStat, 1, 1, 1, 1 }, multipliers);
	EXPECT_TRUE(saturated(result, DerivedStat::PhysicalAttack));
	EXPECT_FALSE(saturated(result, DerivedStat::MagicalAttack));
	EXPECT_FALSE(saturated(result, DerivedStat::Precision));
	EXPECT_FALSE(saturated(result, DerivedStat::PhysicalDefense));
	EXPECT_FALSE(saturated(result, DerivedStat::MagicalDefense));
	EXPECT_FALSE(saturated(result, DerivedStat::MaximumHealth));
	EXPECT_FALSE(saturated(result, DerivedStat::MaximumMana));
}
