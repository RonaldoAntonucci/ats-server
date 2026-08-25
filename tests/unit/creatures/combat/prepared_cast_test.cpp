/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/prepared_cast.hpp"
#include "creatures/combat/spells.hpp"
#include "creatures/combat/condition.hpp"
#include "creatures/players/grouping/groups.hpp"
#include "creatures/players/player.hpp"
#include "enums/account_group_type.hpp"
#include "game/game.hpp"
#include "items/tile.hpp"
#include "lua/functions/lua_functions_loader.hpp"
#include "lua/scripts/scripts.hpp"

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
	const auto deadline = PreparedCastClock::time_point {} + std::chrono::milliseconds(700);

	const PreparedCastState state {
		.config = PreparedCastConfig { 700, true, true, true },
		.context = PreparedCastContext { 43, Position { 100, 200, 7 }, DIRECTION_EAST },
		.variant = variant,
		.completionEventId = 99,
		.deadline = deadline,
	};

	EXPECT_EQ(43u, state.context.id);
	EXPECT_EQ(99u, state.completionEventId);
	EXPECT_EQ(deadline, state.deadline);
	EXPECT_EQ(VARIANT_POSITION, state.variant.type);
	EXPECT_EQ((Position { 101, 200, 7 }), state.variant.pos);
	EXPECT_TRUE(state.spell.expired());
}

TEST(PreparedCastSnapshotTest, RejectsSynchronousCastDuration) {
	const auto now = PreparedCastClock::time_point {} + std::chrono::seconds(1);
	const PreparedCastState state {
		.config = PreparedCastConfig {},
		.context = PreparedCastContext { 44 },
		.deadline = now + std::chrono::milliseconds(1),
	};

	EXPECT_FALSE(state.snapshotAt(now).has_value());
}

TEST(PreparedCastSnapshotTest, ReturnsTheFullDurationAtStart) {
	const auto now = PreparedCastClock::time_point {} + std::chrono::seconds(1);
	const PreparedCastState state {
		.config = PreparedCastConfig { 700 },
		.context = PreparedCastContext { 45 },
		.deadline = now + std::chrono::milliseconds(700),
	};

	const auto snapshot = state.snapshotAt(now);
	ASSERT_TRUE(snapshot.has_value());
	EXPECT_EQ(45u, snapshot->id);
	EXPECT_EQ(700u, snapshot->durationMs);
	EXPECT_EQ(700u, snapshot->remainingMs);
}

TEST(PreparedCastSnapshotTest, ReturnsPartialRemainingDuration) {
	const auto now = PreparedCastClock::time_point {} + std::chrono::seconds(1);
	const PreparedCastState state {
		.config = PreparedCastConfig { 700 },
		.context = PreparedCastContext { 46 },
		.deadline = now + std::chrono::milliseconds(350),
	};

	const auto snapshot = state.snapshotAt(now);
	ASSERT_TRUE(snapshot.has_value());
	EXPECT_EQ(350u, snapshot->remainingMs);
}

TEST(PreparedCastSnapshotTest, RoundsAPositiveFractionalMillisecondUp) {
	const auto now = PreparedCastClock::time_point {} + std::chrono::seconds(1);
	const PreparedCastState state {
		.config = PreparedCastConfig { 700 },
		.context = PreparedCastContext { 47 },
		.deadline = now + std::chrono::microseconds(1),
	};

	const auto snapshot = state.snapshotAt(now);
	ASSERT_TRUE(snapshot.has_value());
	EXPECT_EQ(1u, snapshot->remainingMs);
}

TEST(PreparedCastSnapshotTest, ClampsRemainingDurationToTheConfiguredTotal) {
	const auto now = PreparedCastClock::time_point {} + std::chrono::seconds(1);
	const PreparedCastState state {
		.config = PreparedCastConfig { 700 },
		.context = PreparedCastContext { 48 },
		.deadline = now + std::chrono::milliseconds(701),
	};

	const auto snapshot = state.snapshotAt(now);
	ASSERT_TRUE(snapshot.has_value());
	EXPECT_EQ(700u, snapshot->remainingMs);
}

TEST(PreparedCastSnapshotTest, IsEmptyAtTheExactDeadline) {
	const auto deadline = PreparedCastClock::time_point {} + std::chrono::seconds(1);
	const PreparedCastState state {
		.config = PreparedCastConfig { 700 },
		.context = PreparedCastContext { 49 },
		.deadline = deadline,
	};

	EXPECT_FALSE(state.snapshotAt(deadline).has_value());
}

TEST(PreparedCastSnapshotTest, IsEmptyAfterTheDeadline) {
	const auto deadline = PreparedCastClock::time_point {} + std::chrono::seconds(1);
	const PreparedCastState state {
		.config = PreparedCastConfig { 700 },
		.context = PreparedCastContext { 50 },
		.deadline = deadline,
	};

	EXPECT_FALSE(state.snapshotAt(deadline + std::chrono::milliseconds(1)).has_value());
}

TEST(PreparedCastSnapshotTest, UsesThePreparedCastTokenAsTheSnapshotId) {
	const auto now = PreparedCastClock::time_point {} + std::chrono::seconds(1);
	const PreparedCastState state {
		.config = PreparedCastConfig { 700 },
		.context = PreparedCastContext { 51 },
		.deadline = now + std::chrono::milliseconds(100),
	};

	const auto snapshot = state.snapshotAt(now);
	ASSERT_TRUE(snapshot.has_value());
	EXPECT_EQ(state.context.id, snapshot->id);
}

TEST(PreparedCastDomainTest, SerializesEveryStableInterruptionReason) {
	EXPECT_EQ("position_change", preparedCastInterruptReason(PreparedCastInterruptReason::PositionChange));
	EXPECT_EQ("death", preparedCastInterruptReason(PreparedCastInterruptReason::Death));
	EXPECT_EQ("logout", preparedCastInterruptReason(PreparedCastInterruptReason::Logout));
	EXPECT_EQ("removal", preparedCastInterruptReason(PreparedCastInterruptReason::Removal));
	EXPECT_EQ("scheduler_rejected", preparedCastInterruptReason(PreparedCastInterruptReason::SchedulerRejected));
}

namespace {
	int preparedInterruptCallbackCount = 0;
	std::string preparedInterruptCallbackReason;
	size_t preparedCancelQueriesBeforeCallback = 0;

	int observePreparedInterruption(lua_State* L) {
		++preparedInterruptCallbackCount;
		preparedInterruptCallbackReason = Lua::getString(L, 4);
		preparedCancelQueriesBeforeCallback = g_game().getPreparedCastPublicationMetrics().cancelSpatialQueries;
		return 0;
	}

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
		const auto deadline = PreparedCastClock::now() + std::chrono::milliseconds(config.durationMs);
		return {
			.config = config,
			.context = PreparedCastContext { id, Position { 100, 200, 7 }, DIRECTION_EAST },
			.deadline = deadline,
		};
	}

	void placePreparedCreature(const std::shared_ptr<Creature> &creature, const Position &position) {
		ASSERT_NE(nullptr, g_game().map.getOrCreateTile(position, true));
		ASSERT_TRUE(g_game().internalPlaceCreature(creature, position, false, true));
	}

	void removePreparedCreature(const std::shared_ptr<Creature> &creature) {
		if (creature && !creature->isRemoved()) {
			const auto tile = creature->getTile();
			ASSERT_NE(nullptr, tile);
			tile->removeCreature(creature);
			creature->setParent({});
			creature->removeList();
			creature->setRemoved();
			Game::removeCreatureCheck(creature);
		}
	}

	std::shared_ptr<Player> makePreparedPlayer() {
		static uint32_t nextGuid = 900000;
		auto player = std::make_shared<Player>();
		const auto guid = nextGuid++;
		player->setName("Prepared Cast Viewer " + std::to_string(guid));
		player->setGUID(guid);
		player->setID();
		auto group = std::make_shared<Group>();
		group->id = GROUP_TYPE_NORMAL;
		player->setGroup(group);
		player->setFlag(PlayerFlags_t::IgnoreSpellCheck);
		return player;
	}

	std::shared_ptr<InstantSpell> makePreparedInstant(std::string_view words, uint16_t spellId) {
		auto spell = std::make_shared<InstantSpell>();
		spell->setName(std::string(words));
		spell->setWords(words);
		spell->setSpellId(spellId);
		spell->setSelfTarget(true);
		spell->setAggressive(false);
		spell->soundCastEffect = SoundEffect_t::SILENCE;
		return spell;
	}

	std::shared_ptr<InstantSpell> makeObservedPreparedInstant(std::string_view words) {
		auto spell = makePreparedInstant(words, 0);
		lua_State* L = g_scripts().getScriptInterface().getLuaState();
		lua_pushcfunction(L, observePreparedInterruption);
		spell->setPrepareInterruptScriptId(g_scripts().getScriptInterface().getEvent());
		g_spells().setInstantSpell(spell->getWords(), spell);
		return spell;
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
	g_game().resetPreparedCastPublicationMetrics();

	creature->Creature::onCreatureMove(creature, newTile, newTile->getPosition(), oldTile, oldTile->getPosition(), false);

	EXPECT_FALSE(creature->hasPreparedCast());
	EXPECT_EQ(1u, g_game().getPreparedCastPublicationMetrics().cancelSpatialQueries);
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
	g_game().resetPreparedCastPublicationMetrics();

	creature->onDeath();

	EXPECT_FALSE(creature->hasPreparedCast());
	EXPECT_EQ(1u, g_game().getPreparedCastPublicationMetrics().cancelSpatialQueries);
}

TEST(PreparedCastPublicationTest, ZeroDurationCreatesNoStartPublication) {
	auto creature = std::make_shared<TestPreparedCreature>();
	g_game().resetPreparedCastPublicationMetrics();

	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(2000, {})));
	EXPECT_EQ(0u, g_game().getPreparedCastPublicationMetrics().startSpatialQueries);
	ASSERT_NE(nullptr, creature->completePreparedCast(2000));
}

TEST(PreparedCastPublicationTest, AcceptedPositiveDurationPublishesOneStart) {
	auto creature = std::make_shared<TestPreparedCreature>();
	g_game().resetPreparedCastPublicationMetrics();

	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(2001)));
	EXPECT_EQ(1u, g_game().getPreparedCastPublicationMetrics().startSpatialQueries);
	ASSERT_NE(nullptr, creature->completePreparedCast(2001));
}

TEST(PreparedCastPublicationTest, RejectedSecondCastPublishesNoAdditionalStart) {
	auto creature = std::make_shared<TestPreparedCreature>();
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(2002)));
	g_game().resetPreparedCastPublicationMetrics();

	EXPECT_FALSE(creature->beginPreparedCast(makePreparedState(2003)));
	EXPECT_EQ(0u, g_game().getPreparedCastPublicationMetrics().startSpatialQueries);
	ASSERT_NE(nullptr, creature->completePreparedCast(2002));
}

TEST(PreparedCastPublicationTest, NormalCompletionPublishesNoCancel) {
	auto creature = std::make_shared<TestPreparedCreature>();
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(2004)));
	g_game().resetPreparedCastPublicationMetrics();

	ASSERT_NE(nullptr, creature->completePreparedCast(2004));
	EXPECT_EQ(0u, g_game().getPreparedCastPublicationMetrics().cancelSpatialQueries);
}

TEST(PreparedCastPublicationTest, EarlyInterruptionPublishesMatchingCancel) {
	auto creature = std::make_shared<TestPreparedCreature>();
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(2005)));
	g_game().resetPreparedCastPublicationMetrics();

	const auto interrupted = creature->interruptPreparedCast(PreparedCastInterruptReason::Removal);
	ASSERT_NE(nullptr, interrupted);
	EXPECT_EQ(2005u, interrupted->context.id);
	EXPECT_EQ(1u, g_game().getPreparedCastPublicationMetrics().cancelSpatialQueries);
}

TEST(PreparedCastPublicationTest, InterruptionAtOrAfterDeadlinePublishesNoCancel) {
	auto state = makePreparedState(2006);
	state.deadline = PreparedCastClock::now();
	auto creature = std::make_shared<TestPreparedCreature>();
	ASSERT_TRUE(creature->beginPreparedCast(std::move(state)));
	g_game().resetPreparedCastPublicationMetrics();

	ASSERT_NE(nullptr, creature->interruptPreparedCast(PreparedCastInterruptReason::Removal));
	EXPECT_EQ(0u, g_game().getPreparedCastPublicationMetrics().cancelSpatialQueries);
}

TEST(PreparedCastPublicationTest, NonPlayerCasterWithNoViewersHasZeroRecipients) {
	const Position position { 300, 300, 7 };
	auto creature = std::make_shared<TestPreparedCreature>();
	placePreparedCreature(creature, position);
	g_game().resetPreparedCastPublicationMetrics();

	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(2007)));
	const auto &metrics = g_game().getPreparedCastPublicationMetrics();
	EXPECT_EQ(1u, metrics.startSpatialQueries);
	EXPECT_EQ(0u, metrics.startRecipients);
	ASSERT_NE(nullptr, creature->completePreparedCast(2007));
	removePreparedCreature(creature);
}

TEST(PreparedCastPublicationTest, PlayerCasterReceivesItsOwnStart) {
	const Position position { 320, 320, 7 };
	auto caster = makePreparedPlayer();
	placePreparedCreature(caster, position);
	caster->resetTestPreparedCastDeliveries();
	g_game().resetPreparedCastPublicationMetrics();

	ASSERT_TRUE(caster->beginPreparedCast(makePreparedState(2008)));
	const auto &metrics = g_game().getPreparedCastPublicationMetrics();
	EXPECT_EQ(1u, metrics.startSpatialQueries);
	EXPECT_EQ(1u, metrics.startRecipients);
	ASSERT_EQ(1u, caster->getTestPreparedCastDeliveries().starts);
	ASSERT_TRUE(caster->getTestPreparedCastDeliveries().lastStart.has_value());
	EXPECT_EQ(2008u, caster->getTestPreparedCastDeliveries().lastStart->id);
	ASSERT_NE(nullptr, caster->completePreparedCast(2008));
	removePreparedCreature(caster);
}

TEST(PreparedCastPublicationTest, OneVisibleSpectatorReceivesTheStart) {
	const Position casterPosition { 340, 340, 7 };
	auto caster = std::make_shared<TestPreparedCreature>();
	auto spectator = makePreparedPlayer();
	placePreparedCreature(caster, casterPosition);
	placePreparedCreature(spectator, Position { 341, 340, 7 });
	spectator->resetTestPreparedCastDeliveries();
	g_game().resetPreparedCastPublicationMetrics();

	ASSERT_TRUE(caster->beginPreparedCast(makePreparedState(2009)));
	EXPECT_EQ(1u, g_game().getPreparedCastPublicationMetrics().startRecipients);
	EXPECT_EQ(1u, spectator->getTestPreparedCastDeliveries().starts);
	ASSERT_NE(nullptr, caster->completePreparedCast(2009));
	removePreparedCreature(spectator);
	removePreparedCreature(caster);
}

TEST(PreparedCastPublicationTest, SeveralVisibleSpectatorsExcludeOutOfViewPlayer) {
	const Position casterPosition { 360, 360, 7 };
	auto caster = std::make_shared<TestPreparedCreature>();
	auto first = makePreparedPlayer();
	auto second = makePreparedPlayer();
	auto outside = makePreparedPlayer();
	placePreparedCreature(caster, casterPosition);
	placePreparedCreature(first, Position { 361, 360, 7 });
	placePreparedCreature(second, Position { 360, 361, 7 });
	placePreparedCreature(outside, Position { 500, 500, 7 });
	first->resetTestPreparedCastDeliveries();
	second->resetTestPreparedCastDeliveries();
	outside->resetTestPreparedCastDeliveries();
	g_game().resetPreparedCastPublicationMetrics();

	ASSERT_TRUE(caster->beginPreparedCast(makePreparedState(2010)));
	const auto &metrics = g_game().getPreparedCastPublicationMetrics();
	EXPECT_EQ(1u, metrics.startSpatialQueries);
	EXPECT_EQ(2u, metrics.startRecipients);
	EXPECT_EQ(1u, first->getTestPreparedCastDeliveries().starts);
	EXPECT_EQ(1u, second->getTestPreparedCastDeliveries().starts);
	EXPECT_EQ(0u, outside->getTestPreparedCastDeliveries().starts);
	ASSERT_NE(nullptr, caster->completePreparedCast(2010));
	removePreparedCreature(outside);
	removePreparedCreature(second);
	removePreparedCreature(first);
	removePreparedCreature(caster);
}

TEST(PreparedCastPublicationTest, EveryRecipientGetsTheSameStartSnapshot) {
	const Position casterPosition { 380, 380, 7 };
	auto caster = std::make_shared<TestPreparedCreature>();
	auto first = makePreparedPlayer();
	auto second = makePreparedPlayer();
	placePreparedCreature(caster, casterPosition);
	placePreparedCreature(first, Position { 381, 380, 7 });
	placePreparedCreature(second, Position { 380, 381, 7 });
	first->resetTestPreparedCastDeliveries();
	second->resetTestPreparedCastDeliveries();

	ASSERT_TRUE(caster->beginPreparedCast(makePreparedState(2011)));
	const auto &firstSnapshot = first->getTestPreparedCastDeliveries().lastStart;
	const auto &secondSnapshot = second->getTestPreparedCastDeliveries().lastStart;
	ASSERT_TRUE(firstSnapshot.has_value());
	ASSERT_TRUE(secondSnapshot.has_value());
	EXPECT_EQ(firstSnapshot->id, secondSnapshot->id);
	EXPECT_EQ(firstSnapshot->durationMs, secondSnapshot->durationMs);
	EXPECT_EQ(firstSnapshot->remainingMs, secondSnapshot->remainingMs);
	ASSERT_NE(nullptr, caster->completePreparedCast(2011));
	removePreparedCreature(second);
	removePreparedCreature(first);
	removePreparedCreature(caster);
}

TEST(PreparedCastPublicationTest, CancelIsPublishedBeforeInterruptionCallback) {
	preparedCancelQueriesBeforeCallback = 0;
	const auto spell = makeObservedPreparedInstant("prepared cancel ordering");
	auto state = makePreparedState(2012);
	state.spell = spell;
	auto caster = std::make_shared<TestPreparedCreature>();
	ASSERT_TRUE(caster->beginPreparedCast(std::move(state)));
	g_game().resetPreparedCastPublicationMetrics();

	ASSERT_NE(nullptr, caster->interruptPreparedCast(PreparedCastInterruptReason::Removal));
	EXPECT_EQ(1u, preparedCancelQueriesBeforeCallback);
}

TEST(PreparedCastInstantSpellTest, UsesTheLegacySynchronousConfigurationByDefault) {
	const auto spell = std::make_shared<InstantSpell>();

	EXPECT_FALSE(spell->usesPreparedCast());
	EXPECT_EQ(0u, spell->getPreparedCastConfig().durationMs);
	EXPECT_FALSE(spell->hasPrepareStartCallback());
	EXPECT_FALSE(spell->hasPrepareInterruptCallback());
}

TEST(PreparedCastInstantSpellTest, RetainsPreparedConfigurationAndCallbackIdentity) {
	const auto spell = std::make_shared<InstantSpell>();
	spell->setPreparedCastConfig({ 700, true, true, true });
	spell->setPrepareStartScriptId(120);
	spell->setPrepareInterruptScriptId(121);

	EXPECT_TRUE(spell->usesPreparedCast());
	EXPECT_EQ(700u, spell->getPreparedCastConfig().durationMs);
	EXPECT_TRUE(spell->getPreparedCastConfig().lockMovement);
	EXPECT_TRUE(spell->getPreparedCastConfig().lockDirection);
	EXPECT_TRUE(spell->getPreparedCastConfig().interruptOnPositionChange);
	EXPECT_TRUE(spell->hasPrepareStartCallback());
	EXPECT_TRUE(spell->hasPrepareInterruptCallback());
	EXPECT_EQ(120, spell->getPrepareStartScriptId());
	EXPECT_EQ(121, spell->getPrepareInterruptScriptId());
}

TEST(PreparedCastInstantSpellTest, MissingOptionalPreflightAcceptsWithoutLua) {
	const auto spell = std::make_shared<InstantSpell>();
	const auto creature = std::make_shared<TestPreparedCreature>();
	const LuaVariant variant;
	const PreparedCastContext context { 1100, Position { 100, 200, 7 }, DIRECTION_NORTH };

	EXPECT_TRUE(spell->executePrepareStart(creature, variant, context));
}

TEST(PreparedCastInstantSpellTest, StaleCompletionTokenLeavesTheActiveCastUntouched) {
	const auto spell = std::make_shared<InstantSpell>();
	const auto creature = std::make_shared<TestPreparedCreature>();
	auto state = makePreparedState(1200);
	state.spell = spell;
	ASSERT_TRUE(creature->beginPreparedCast(std::move(state)));

	InstantSpell::completePreparedCast(creature, 1201);

	EXPECT_TRUE(creature->hasPreparedCast());
}

TEST(PreparedCastInstantSpellTest, CompletionReleasesStateWhenSpellIsNotRegistered) {
	const auto spell = std::make_shared<InstantSpell>();
	const auto creature = std::make_shared<TestPreparedCreature>();
	auto state = makePreparedState(1300);
	state.spell = spell;
	ASSERT_TRUE(creature->beginPreparedCast(std::move(state)));

	InstantSpell::completePreparedCast(creature, 1300);

	EXPECT_FALSE(creature->hasPreparedCast());
}

TEST(PreparedCastInstantSpellTest, CompletionReleasesStateAfterSpellExpires) {
	const auto creature = std::make_shared<TestPreparedCreature>();
	auto state = makePreparedState(1400);
	{
		const auto obsoleteSpell = std::make_shared<InstantSpell>();
		state.spell = obsoleteSpell;
	}
	ASSERT_TRUE(state.spell.expired());
	ASSERT_TRUE(creature->beginPreparedCast(std::move(state)));

	InstantSpell::completePreparedCast(creature, 1400);

	EXPECT_FALSE(creature->hasPreparedCast());
}

TEST(PreparedCastInstantSpellTest, ReplacementMakesTheOldRegistrationObsolete) {
	const auto oldSpell = std::make_shared<InstantSpell>();
	oldSpell->setName("prepared replacement old");
	oldSpell->setWords("prepared replacement");
	const auto newSpell = std::make_shared<InstantSpell>();
	newSpell->setName("prepared replacement new");
	newSpell->setWords("prepared replacement");
	g_spells().setInstantSpell("prepared replacement", oldSpell);
	EXPECT_TRUE(oldSpell->isCurrentRegistration());

	g_spells().setInstantSpell("prepared replacement", newSpell);

	EXPECT_FALSE(oldSpell->isCurrentRegistration());
	EXPECT_TRUE(newSpell->isCurrentRegistration());
}

TEST(PreparedCastInstantSpellTest, AcceptedStartOwnsStateAndAppliesCooldownExactlyOnce) {
	const auto player = makePreparedPlayer();
	const auto spell = makePreparedInstant("prepared accepted", 61000);
	spell->setCooldown(5000);
	spell->setPreparedCastConfig({ 60000, true, true, true });
	g_spells().setInstantSpell(spell->getWords(), spell);
	std::string param;

	ASSERT_TRUE(spell->playerCastInstant(player, param));
	EXPECT_TRUE(player->hasPreparedCast());
	EXPECT_TRUE(player->preparedCastLocksMovement());
	EXPECT_TRUE(player->preparedCastLocksDirection());
	const auto cooldown = player->getCondition(CONDITION_SPELLCOOLDOWN, CONDITIONID_DEFAULT, spell->getSpellId());
	ASSERT_NE(nullptr, cooldown);
	const auto firstCooldownEnd = cooldown->getEndTime();

	EXPECT_FALSE(spell->playerCastInstant(player, param));
	EXPECT_TRUE(player->hasPreparedCast());
	EXPECT_EQ(firstCooldownEnd, cooldown->getEndTime());
	ASSERT_NE(nullptr, player->interruptPreparedCast(PreparedCastInterruptReason::Removal));
	EXPECT_FALSE(player->hasPreparedCast());
	EXPECT_NE(nullptr, player->getCondition(CONDITION_SPELLCOOLDOWN, CONDITIONID_DEFAULT, spell->getSpellId()));
}

TEST(PreparedCastInstantSpellTest, UnownedPreparedSpellRejectsWithoutStateOrCooldown) {
	const auto player = makePreparedPlayer();
	InstantSpell spell;
	spell.setName("prepared unowned");
	spell.setWords("prepared unowned");
	spell.setSpellId(61001);
	spell.setSelfTarget(true);
	spell.setAggressive(false);
	spell.soundCastEffect = SoundEffect_t::SILENCE;
	spell.setCooldown(5000);
	spell.setPreparedCastConfig({ 700, true, true, true });
	std::string param;

	EXPECT_FALSE(spell.playerCastInstant(player, param));
	EXPECT_FALSE(player->hasPreparedCast());
	EXPECT_EQ(nullptr, player->getCondition(CONDITION_SPELLCOOLDOWN, CONDITIONID_DEFAULT, spell.getSpellId()));
}

TEST(PreparedCastGameTest, PreparedDirectionLockBlocksTurnWithoutMutatingLegacyState) {
	const auto creature = std::make_shared<TestPreparedCreature>();
	creature->setDirection(DIRECTION_EAST);
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(1500, { 700, false, true, false })));

	EXPECT_TRUE(g_game().internalCreatureTurn(creature, DIRECTION_WEST));
	EXPECT_EQ(DIRECTION_EAST, creature->getDirection());
	EXPECT_FALSE(creature->isDirectionLocked());
	ASSERT_NE(nullptr, creature->interruptPreparedCast(PreparedCastInterruptReason::Removal));
	EXPECT_FALSE(creature->isDirectionLocked());

	EXPECT_TRUE(g_game().internalCreatureTurn(creature, DIRECTION_WEST));
	EXPECT_EQ(DIRECTION_WEST, creature->getDirection());
	creature->setDirectionLocked(true);
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(1501, { 700, false, true, false })));
	ASSERT_NE(nullptr, creature->interruptPreparedCast(PreparedCastInterruptReason::Removal));
	EXPECT_TRUE(creature->isDirectionLocked());
	EXPECT_TRUE(g_game().internalCreatureTurn(creature, DIRECTION_NORTH));
	EXPECT_EQ(DIRECTION_WEST, creature->getDirection());
}

TEST(PreparedCastGameTest, LogoutInterruptsBeforeTeardownExactlyOnce) {
	preparedInterruptCallbackCount = 0;
	preparedInterruptCallbackReason.clear();
	const auto creature = std::make_shared<TestPreparedCreature>();
	const Position position { 200, 200, 7 };
	ASSERT_NE(nullptr, g_game().map.getOrCreateTile(position, true));
	ASSERT_TRUE(g_game().internalPlaceCreature(creature, position, false, true));
	const auto spell = makeObservedPreparedInstant("prepared logout guard");
	auto state = makePreparedState(1600);
	state.spell = spell;
	ASSERT_TRUE(creature->beginPreparedCast(std::move(state)));
	g_game().resetPreparedCastPublicationMetrics();

	ASSERT_TRUE(g_game().removeCreature(creature, true));
	EXPECT_FALSE(creature->hasPreparedCast());
	EXPECT_TRUE(creature->isRemoved());
	EXPECT_EQ(1, preparedInterruptCallbackCount);
	EXPECT_EQ("logout", preparedInterruptCallbackReason);
	EXPECT_EQ(1u, g_game().getPreparedCastPublicationMetrics().cancelSpatialQueries);
	EXPECT_FALSE(g_game().removeCreature(creature, true));
	InstantSpell::completePreparedCast(creature, 1600);
	EXPECT_EQ(1, preparedInterruptCallbackCount);
}

TEST(PreparedCastGameTest, RemovalUsesItsDistinctStableReason) {
	preparedInterruptCallbackCount = 0;
	preparedInterruptCallbackReason.clear();
	const auto creature = std::make_shared<TestPreparedCreature>();
	const Position position { 201, 200, 7 };
	ASSERT_NE(nullptr, g_game().map.getOrCreateTile(position, true));
	ASSERT_TRUE(g_game().internalPlaceCreature(creature, position, false, true));
	const auto spell = makeObservedPreparedInstant("prepared removal guard");
	auto state = makePreparedState(1700);
	state.spell = spell;
	ASSERT_TRUE(creature->beginPreparedCast(std::move(state)));
	g_game().resetPreparedCastPublicationMetrics();

	ASSERT_TRUE(g_game().removeCreature(creature, false));
	EXPECT_FALSE(creature->hasPreparedCast());
	EXPECT_EQ(1, preparedInterruptCallbackCount);
	EXPECT_EQ("removal", preparedInterruptCallbackReason);
	EXPECT_EQ(1u, g_game().getPreparedCastPublicationMetrics().cancelSpatialQueries);
}

TEST(PreparedCastGameTest, DamageAndNonPositionalControlLeavePreparationActive) {
	const auto creature = std::make_shared<TestPreparedCreature>();
	ASSERT_TRUE(creature->beginPreparedCast(makePreparedState(1800)));
	const int32_t healthBefore = creature->getHealth();

	creature->changeHealth(-10, false);
	EXPECT_EQ(healthBefore - 10, creature->getHealth());
	EXPECT_TRUE(creature->hasPreparedCast());

	const auto condition = Condition::createCondition(CONDITIONID_COMBAT, CONDITION_PARALYZE, 1000, 0);
	ASSERT_TRUE(creature->addCondition(condition));
	EXPECT_TRUE(creature->hasPreparedCast());
	ASSERT_NE(nullptr, creature->interruptPreparedCast(PreparedCastInterruptReason::Removal));
}
