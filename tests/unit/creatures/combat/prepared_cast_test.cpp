/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/prepared_cast.hpp"
#include "creatures/players/player.hpp"
#include "items/tile.hpp"

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

namespace {
	class TestPreparedCreature final : public Creature {
	public:
		std::string getDescription(int32_t) override {
			return name;
		}

		const std::string &getName() const override {
			return name;
		}

		const std::string &getTypeName() const override {
			return name;
		}

		const std::string &getNameDescription() const override {
			return name;
		}

		CreatureType_t getType() const override {
			return CREATURETYPE_PLAYER;
		}

		void setID() override { }
		void removeList() override { }
		void addList() override { }

		uint16_t getStepSpeed() const override {
			return 0;
		}

	private:
		std::string name = "prepared test creature";

	protected:
		bool dropCorpse(const std::shared_ptr<Creature> &, const std::shared_ptr<Creature> &, bool, bool) override {
			return false;
		}

		void death(const std::shared_ptr<Creature> &) override { }
	};

	PreparedCastState makePreparedState(uint64_t id, PreparedCastConfig config = { 700, true, true, true }) {
		return {
			.config = config,
			.context = PreparedCastContext { id, Position { 100, 200, 7 }, DIRECTION_EAST },
		};
	}
}

TEST(PreparedCastCreatureTest, OwnsOnlyOneActivePreparation) {
	auto creature = std::make_shared<Player>();

	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(100)));
	EXPECT_TRUE(creature->hasPreparedCast());
	EXPECT_TRUE(creature->preparedCastLocksMovement());
	EXPECT_TRUE(creature->preparedCastLocksDirection());
	EXPECT_FALSE(creature->beginPreparedCast(makePreparedState(101)));
	EXPECT_EQ(nullptr, creature->completePreparedCast(101));
	EXPECT_TRUE(creature->hasPreparedCast());
}

TEST(PreparedCastCreatureTest, CompletesTheMatchingTokenExactlyOnce) {
	auto creature = std::make_shared<Player>();
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(200)));

	auto completed = creature->completePreparedCast(200);

	ASSERT_NE(nullptr, completed);
	EXPECT_EQ(200u, completed->context.id);
	EXPECT_FALSE(creature->hasPreparedCast());
	EXPECT_FALSE(creature->preparedCastLocksMovement());
	EXPECT_FALSE(creature->preparedCastLocksDirection());
	EXPECT_EQ(nullptr, creature->completePreparedCast(200));
}

TEST(PreparedCastCreatureTest, InterruptsAndReleasesDerivedLocksExactlyOnce) {
	auto creature = std::make_shared<Player>();
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(300)));

	auto interrupted = creature->interruptPreparedCast(PreparedCastInterruptReason::Death);

	ASSERT_NE(nullptr, interrupted);
	EXPECT_EQ(300u, interrupted->context.id);
	EXPECT_FALSE(creature->hasPreparedCast());
	EXPECT_FALSE(creature->preparedCastLocksMovement());
	EXPECT_FALSE(creature->preparedCastLocksDirection());
	EXPECT_EQ(nullptr, creature->interruptPreparedCast(PreparedCastInterruptReason::Death));
}

TEST(PreparedCastCreatureTest, PreservesLegacyLocksDuringPreparedCleanup) {
	auto creature = std::make_shared<Player>();
	creature->setMoveLocked(true);
	creature->setDirectionLocked(true);
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(400)));

	ASSERT_NE(nullptr, creature->interruptPreparedCast(PreparedCastInterruptReason::Removal));

	EXPECT_TRUE(creature->isMoveLocked());
	EXPECT_TRUE(creature->isDirectionLocked());
}

TEST(PreparedCastCreatureTest, RejectsVoluntaryMovementButAllowsForcedMovement) {
	auto creature = std::make_shared<TestPreparedCreature>();
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(500)));

	creature->startAutoWalk({ DIRECTION_NORTH });
	Direction direction = DIRECTION_NONE;
	uint32_t flags = 0;
	EXPECT_FALSE(creature->getNextStep(direction, flags));

	creature->startAutoWalk({ DIRECTION_WEST }, true);
	EXPECT_TRUE(creature->getNextStep(direction, flags));
	EXPECT_EQ(DIRECTION_WEST, direction);
	EXPECT_TRUE(creature->hasPreparedCast());
	creature->stopEventWalk();
}

TEST(PreparedCastCreatureTest, StartingPreparationCancelsQueuedMovement) {
	auto creature = std::make_shared<TestPreparedCreature>();
	creature->startAutoWalk({ DIRECTION_SOUTH }, true);
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(600)));

	Direction direction = DIRECTION_NONE;
	uint32_t flags = 0;
	EXPECT_FALSE(creature->getNextStep(direction, flags));
}

TEST(PreparedCastCreatureTest, RealSelfPositionChangeInterruptsConfiguredPreparation) {
	auto creature = std::make_shared<Player>();
	auto oldTile = std::make_shared<DynamicTile>(Position { 100, 200, 7 });
	auto newTile = std::make_shared<DynamicTile>(Position { 101, 200, 7 });
	creature->setParent(oldTile);
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(700)));

	creature->Creature::onCreatureMove(creature, newTile, newTile->getPosition(), oldTile, oldTile->getPosition(), false);

	EXPECT_FALSE(creature->hasPreparedCast());
}

TEST(PreparedCastCreatureTest, PositionChangePolicyCanRemainDisabled) {
	auto creature = std::make_shared<Player>();
	auto oldTile = std::make_shared<DynamicTile>(Position { 100, 200, 7 });
	auto newTile = std::make_shared<DynamicTile>(Position { 101, 200, 7 });
	creature->setParent(oldTile);
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(800, { 700, true, true, false })));

	creature->Creature::onCreatureMove(creature, newTile, newTile->getPosition(), oldTile, oldTile->getPosition(), false);

	EXPECT_TRUE(creature->hasPreparedCast());
}

TEST(PreparedCastCreatureTest, OtherCreatureMovementCannotInterruptPreparation) {
	auto creature = std::make_shared<Player>();
	auto other = std::make_shared<Player>();
	auto oldTile = std::make_shared<DynamicTile>(Position { 100, 200, 7 });
	auto newTile = std::make_shared<DynamicTile>(Position { 101, 200, 7 });
	creature->setParent(oldTile);
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(900)));

	creature->Creature::onCreatureMove(other, newTile, newTile->getPosition(), oldTile, oldTile->getPosition(), false);

	EXPECT_TRUE(creature->hasPreparedCast());
}

TEST(PreparedCastCreatureTest, DeathInterruptsPreparationBeforeDeathHandling) {
	auto creature = std::make_shared<TestPreparedCreature>();
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(1000)));

	creature->onDeath();

	EXPECT_FALSE(creature->hasPreparedCast());
}
