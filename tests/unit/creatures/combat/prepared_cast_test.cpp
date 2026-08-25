/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/prepared_cast.hpp"

#include <gtest/gtest.h>

TEST(PreparedCastDomainTest, UsesSynchronousUnlockedDefaults) {
	const PreparedCastConfig config;

	EXPECT_EQ(0u, config.durationMs);
	EXPECT_FALSE(config.lockMovement);
	EXPECT_FALSE(config.lockDirection);
	EXPECT_FALSE(config.interruptOnPositionChange);
}

TEST(PreparedCastDomainTest, RetainsEveryConfiguredPolicy) {
	const PreparedCastConfig config { 700, true, true, true };

	EXPECT_EQ(700u, config.durationMs);
	EXPECT_TRUE(config.lockMovement);
	EXPECT_TRUE(config.lockDirection);
	EXPECT_TRUE(config.interruptOnPositionChange);
}

TEST(PreparedCastDomainTest, CapturesAnImmutableContext) {
	const PreparedCastContext context { 42, Position { 100, 200, 7 }, DIRECTION_WEST };

	EXPECT_EQ(42u, context.id);
	EXPECT_EQ((Position { 100, 200, 7 }), context.origin);
	EXPECT_EQ(DIRECTION_WEST, context.direction);
	EXPECT_FALSE((std::is_assignable_v<decltype(context.origin), Position>));
	EXPECT_FALSE((std::is_assignable_v<decltype(context.direction), Direction>));
}

TEST(PreparedCastDomainTest, KeepsVariantAndCompletionEventIdentityInState) {
	LuaVariant variant;
	variant.type = VARIANT_POSITION;
	variant.pos = Position { 101, 200, 7 };

	const PreparedCastState state {
		.config = PreparedCastConfig { 700, true, true, true },
		.context = PreparedCastContext { 43, Position { 100, 200, 7 }, DIRECTION_EAST },
		.variant = variant,
		.completionEventId = 99,
	};

	EXPECT_EQ(43u, state.context.id);
	EXPECT_EQ(99u, state.completionEventId);
	EXPECT_EQ(VARIANT_POSITION, state.variant.type);
	EXPECT_EQ((Position { 101, 200, 7 }), state.variant.pos);
	EXPECT_TRUE(state.spell.expired());
}

TEST(PreparedCastDomainTest, SerializesEveryStableInterruptionReason) {
	EXPECT_EQ("position_change", preparedCastInterruptReason(PreparedCastInterruptReason::PositionChange));
	EXPECT_EQ("death", preparedCastInterruptReason(PreparedCastInterruptReason::Death));
	EXPECT_EQ("logout", preparedCastInterruptReason(PreparedCastInterruptReason::Logout));
	EXPECT_EQ("removal", preparedCastInterruptReason(PreparedCastInterruptReason::Removal));
	EXPECT_EQ("scheduler_rejected", preparedCastInterruptReason(PreparedCastInterruptReason::SchedulerRejected));
}
