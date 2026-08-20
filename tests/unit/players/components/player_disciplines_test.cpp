/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/player.hpp"
#include "creatures/players/disciplines/discipline.hpp"
#include "kv/value_wrapper.hpp"

#include <gtest/gtest.h>

#include "kv/in_memory_kv.hpp"
#include "lib/logging/in_memory_logger.hpp"

namespace {
	class PlayerDisciplinesTest : public ::testing::Test {
	protected:
		static void SetUpTestSuite() {
			previousTestContainer = DI::getTestContainer();
			InMemoryLogger::install(injector);
			KVMemory::install(injector);
			DI::setTestContainer(&injector);
		}

		static void TearDownTestSuite() {
			DI::setTestContainer(previousTestContainer);
		}

		void SetUp() override {
			kvMemory().reset();
			logger().reset();
			player = std::make_shared<Player>();
			loadCatalog();
		}

		static KVMemory &kvMemory() {
			return dynamic_cast<KVMemory &>(injector.create<KVStore &>());
		}

		static InMemoryLogger &logger() {
			return dynamic_cast<InMemoryLogger &>(injector.create<Logger &>());
		}

		void loadCatalog() {
			const auto file = std::filesystem::temp_directory_path() / "canary-player-disciplines.xml";
			std::ofstream output(file);
			output << R"xml(<disciplines><discipline id="1" name="Armamento"/></disciplines>)xml";
			output.close();
			ASSERT_TRUE(g_disciplines().loadFromXml(file));
			std::error_code error;
			std::filesystem::remove(file, error);
		}

		std::optional<ValueWrapper> persistedRanks() const {
			return player->kv()->scoped("disciplines")->get("ranks");
		}

		std::shared_ptr<Player> player;

		inline static di::extension::injector<> injector {};
		inline static di::extension::injector<>* previousTestContainer = nullptr;
	};
}

TEST_F(PlayerDisciplinesTest, StartsEmptyWithoutCreatingKvData) {
	EXPECT_TRUE(player->disciplines().ranks().empty());
	EXPECT_FALSE(persistedRanks().has_value());
}

TEST_F(PlayerDisciplinesTest, AddsFirstRankAndPersistsNumericMap) {
	const auto mutation = player->disciplines().addRank(1);
	ASSERT_TRUE(mutation.success());
	EXPECT_EQ(0u, mutation.before);
	EXPECT_EQ(1u, mutation.after);
	ASSERT_EQ(1u, player->disciplines().ranks().at(1));
	const auto persisted = persistedRanks();
	ASSERT_TRUE(persisted.has_value());
	const auto map = persisted->get<MapType>();
	ASSERT_TRUE(map.contains("1"));
	EXPECT_EQ(1, map.at("1")->get<IntType>());
	EXPECT_EQ(1u, PlayerDisciplinesTest::kvMemory().writes());
}

TEST_F(PlayerDisciplinesTest, AddsAndRemovesOneRankAtATime) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	EXPECT_EQ(1u, player->disciplines().addRank(1).before);
	EXPECT_EQ(2u, player->disciplines().ranks().at(1));
	EXPECT_EQ(2u, player->disciplines().removeRank(1).before);
	EXPECT_EQ(1u, player->disciplines().ranks().at(1));
	const auto removed = player->disciplines().removeRank(1);
	EXPECT_TRUE(removed.success());
	EXPECT_EQ(1u, removed.before);
	EXPECT_EQ(0u, removed.after);
	EXPECT_TRUE(player->disciplines().ranks().empty());
}

TEST_F(PlayerDisciplinesTest, RejectsUnknownDisciplineWithoutPersisting) {
	const auto mutation = player->disciplines().addRank(2);
	EXPECT_EQ(DisciplineMutationResult::UnknownDiscipline, mutation.result);
	EXPECT_TRUE(player->disciplines().ranks().empty());
	EXPECT_FALSE(persistedRanks().has_value());
}

TEST_F(PlayerDisciplinesTest, RejectsRemovingDisciplineNotOwned) {
	const auto mutation = player->disciplines().removeRank(1);
	EXPECT_EQ(DisciplineMutationResult::NotOwned, mutation.result);
	EXPECT_FALSE(persistedRanks().has_value());
}

TEST_F(PlayerDisciplinesTest, RejectsRemovingUnknownDisciplineWithoutPersisting) {
	const auto mutation = player->disciplines().removeRank(2);
	EXPECT_EQ(DisciplineMutationResult::UnknownDiscipline, mutation.result);
	EXPECT_FALSE(persistedRanks().has_value());
}

TEST_F(PlayerDisciplinesTest, PersistsAnEmptyMapAfterRemovingTheLastRank) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	ASSERT_TRUE(player->disciplines().removeRank(1).success());
	const auto persisted = persistedRanks();
	ASSERT_TRUE(persisted.has_value());
	EXPECT_TRUE(persisted->get<MapType>().empty());
}

TEST_F(PlayerDisciplinesTest, LoadsPersistedRanksLazily) {
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
		{ "1", ValueWrapper(2) },
	});
	EXPECT_EQ(2u, player->disciplines().ranks().at(1));
}

TEST_F(PlayerDisciplinesTest, RejectsCorruptPersistedEntriesAndKeepsValidOnes) {
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
		{ "1", ValueWrapper(2) },
		{ "0", ValueWrapper(1) },
		{ "bad", ValueWrapper(1) },
		{ "2", ValueWrapper(0) },
		{ "3", ValueWrapper("bad") },
	});
	const auto &ranks = player->disciplines().ranks();
	ASSERT_EQ(1u, ranks.size());
	EXPECT_EQ(2u, ranks.at(1));
	EXPECT_GE(PlayerDisciplinesTest::logger().logCount(), 4u);
}

TEST_F(PlayerDisciplinesTest, LoadsNumericIdsInAscendingOrder) {
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
		{ "2", ValueWrapper(3) },
		{ "1", ValueWrapper(2) },
	});
	const auto &ranks = player->disciplines().ranks();
	auto it = ranks.begin();
	ASSERT_NE(ranks.end(), it);
	EXPECT_EQ(1u, it->first);
	EXPECT_EQ(2u, std::next(it)->first);
}

TEST_F(PlayerDisciplinesTest, RejectsZeroAndOutOfRangeStoredIds) {
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
		{ "0", ValueWrapper(1) },
		{ "65536", ValueWrapper(1) },
	});
	EXPECT_TRUE(player->disciplines().ranks().empty());
}

TEST_F(PlayerDisciplinesTest, PreservesExistingRankWhenUnknownMutationIsRequested) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	const auto mutation = player->disciplines().addRank(2);
	EXPECT_EQ(DisciplineMutationResult::UnknownDiscipline, mutation.result);
	EXPECT_EQ(1u, player->disciplines().ranks().at(1));
	const auto persisted = persistedRanks();
	ASSERT_TRUE(persisted.has_value());
	EXPECT_EQ(1, persisted->get<MapType>().at("1")->get<IntType>());
}

TEST_F(PlayerDisciplinesTest, RejectsRankAtTechnicalLimitWithoutMutating) {
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
		{ "1", ValueWrapper(ValueVariant { std::numeric_limits<IntType>::max() }) },
	});
	const auto mutation = player->disciplines().addRank(1);
	EXPECT_EQ(DisciplineMutationResult::RankLimit, mutation.result);
	EXPECT_EQ(static_cast<uint32_t>(std::numeric_limits<IntType>::max()), mutation.before);
	EXPECT_EQ(static_cast<uint32_t>(std::numeric_limits<IntType>::max()), player->disciplines().ranks().at(1));
}
