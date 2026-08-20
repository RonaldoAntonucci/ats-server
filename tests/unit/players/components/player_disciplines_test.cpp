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
#include "creatures/players/vocations/vocation.hpp"
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

		void loadCatalog(std::string_view xml = R"xml(<disciplines><discipline id="1" name="Armamento"><attribute id="for" perLevel="1"/><attribute id="des" perLevel="1"/><attribute id="vit" perLevel="1"/></discipline></disciplines>)xml") {
			const auto file = std::filesystem::temp_directory_path() / "canary-player-disciplines.xml";
			std::ofstream output(file);
			output << xml;
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

	[[nodiscard]] uint64_t attribute(const DisciplineProfile &profile, CharacterAttribute type) {
		return profile.attributes.at(static_cast<size_t>(type));
	}
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

TEST_F(PlayerDisciplinesTest, EmptyProfileContainsFiveZeroAttributes) {
	const auto profile = player->disciplines().profile(player->getLevel());
	EXPECT_EQ(AttributeTotals {}, profile.attributes);
	EXPECT_TRUE(profile.disciplines.empty());
	EXPECT_FALSE(persistedRanks().has_value());
}

TEST_F(PlayerDisciplinesTest, DerivesArmamentoAttributesFromLevelAndRank) {
	player->setLevel(7);
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	ASSERT_TRUE(player->disciplines().addRank(1).success());

	const auto profile = player->disciplines().profile(player->getLevel());
	ASSERT_EQ(1u, profile.disciplines.size());
	EXPECT_EQ(1u, profile.disciplines.front().id);
	EXPECT_EQ("Armamento", profile.disciplines.front().name);
	EXPECT_EQ(2u, profile.disciplines.front().rank);
	EXPECT_EQ(14u, attribute(profile, CharacterAttribute::Strength));
	EXPECT_EQ(14u, attribute(profile, CharacterAttribute::Dexterity));
	EXPECT_EQ(14u, attribute(profile, CharacterAttribute::Vitality));
	EXPECT_EQ(0u, attribute(profile, CharacterAttribute::Intelligence));
	EXPECT_EQ(0u, attribute(profile, CharacterAttribute::Willpower));
}

TEST_F(PlayerDisciplinesTest, DerivesIdenticalProfilesForDifferentVocations) {
	player->setLevel(7);
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	player->setTestVocation(std::make_shared<Vocation>(1));
	ASSERT_EQ(1u, player->getVocationId());
	const auto firstProfile = player->disciplines().profile(player->getLevel());

	player->setTestVocation(std::make_shared<Vocation>(2));
	ASSERT_EQ(2u, player->getVocationId());
	const auto secondProfile = player->disciplines().profile(player->getLevel());
	EXPECT_EQ(firstProfile.attributes, secondProfile.attributes);
	ASSERT_EQ(firstProfile.disciplines.size(), secondProfile.disciplines.size());
	EXPECT_EQ(firstProfile.disciplines.front().id, secondProfile.disciplines.front().id);
	EXPECT_EQ(firstProfile.disciplines.front().name, secondProfile.disciplines.front().name);
	EXPECT_EQ(firstProfile.disciplines.front().rank, secondProfile.disciplines.front().rank);
}

TEST_F(PlayerDisciplinesTest, SerializesConcurrentRankTransitions) {
	constexpr size_t transitionCount = 32;
	std::mutex startMutex;
	std::condition_variable startCondition;
	bool start = false;
	std::vector<DisciplineMutation> mutations(transitionCount);
	std::vector<std::thread> workers;
	workers.reserve(transitionCount);

	for (size_t index = 0; index < transitionCount; ++index) {
		workers.emplace_back([&, index] {
			{
				std::unique_lock lock(startMutex);
				startCondition.wait(lock, [&] { return start; });
			}
			mutations[index] = player->disciplines().addRank(1);
		});
	}
	{
		std::scoped_lock lock(startMutex);
		start = true;
	}
	startCondition.notify_all();
	for (auto &worker : workers) {
		worker.join();
	}

	std::vector<uint32_t> observedBefore;
	observedBefore.reserve(transitionCount);
	for (const auto &mutation : mutations) {
		ASSERT_TRUE(mutation.success());
		EXPECT_EQ(mutation.before + 1, mutation.after);
		observedBefore.push_back(mutation.before);
	}
	std::ranges::sort(observedBefore);
	for (size_t index = 0; index < transitionCount; ++index) {
		EXPECT_EQ(index, observedBefore[index]);
	}

	EXPECT_EQ(transitionCount, player->disciplines().ranks().at(1));
	const auto persisted = persistedRanks();
	ASSERT_TRUE(persisted.has_value());
	EXPECT_EQ(transitionCount, static_cast<size_t>(persisted->get<MapType>().at("1")->get<IntType>()));
	EXPECT_EQ(transitionCount, kvMemory().writes());
}

TEST_F(PlayerDisciplinesTest, ReflectsLevelChangesWithoutPersistingRanksAgain) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	ASSERT_EQ(1u, kvMemory().writes());
	player->setLevel(3);
	EXPECT_EQ(3u, attribute(player->disciplines().profile(player->getLevel()), CharacterAttribute::Strength));
	player->setLevel(8);
	EXPECT_EQ(8u, attribute(player->disciplines().profile(player->getLevel()), CharacterAttribute::Strength));
	EXPECT_EQ(1u, kvMemory().writes());
}

TEST_F(PlayerDisciplinesTest, DerivesProfileFromRanksLoadedFromKv) {
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
		{ "1", ValueWrapper(2) },
	});
	player->setLevel(4);

	const auto profile = player->disciplines().profile(player->getLevel());
	EXPECT_EQ(8u, attribute(profile, CharacterAttribute::Strength));
	ASSERT_EQ(1u, profile.disciplines.size());
	EXPECT_EQ(2u, profile.disciplines.front().rank);
}

TEST_F(PlayerDisciplinesTest, SumsMultipleDisciplinesInAscendingIdOrder) {
	loadCatalog(R"xml(<disciplines>
		<discipline id="2" name="Defesa"><attribute id="vit" perLevel="2"/><attribute id="von" perLevel="1"/></discipline>
		<discipline id="1" name="Armamento"><attribute id="for" perLevel="1"/></discipline>
	</disciplines>)xml");
	player->setLevel(5);
	ASSERT_TRUE(player->disciplines().addRank(2).success());
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	ASSERT_TRUE(player->disciplines().addRank(1).success());

	const auto profile = player->disciplines().profile(player->getLevel());
	ASSERT_EQ(2u, profile.disciplines.size());
	EXPECT_EQ(1u, profile.disciplines[0].id);
	EXPECT_EQ(2u, profile.disciplines[1].id);
	EXPECT_EQ(10u, attribute(profile, CharacterAttribute::Strength));
	EXPECT_EQ(10u, attribute(profile, CharacterAttribute::Vitality));
	EXPECT_EQ(5u, attribute(profile, CharacterAttribute::Willpower));
}

TEST_F(PlayerDisciplinesTest, UsesRenamedCatalogEntryWithoutChangingRanks) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	player->setLevel(3);
	loadCatalog(R"xml(<disciplines><discipline id="1" name="Novo Armamento"><attribute id="for" perLevel="3"/></discipline></disciplines>)xml");

	const auto profile = player->disciplines().profile(player->getLevel());
	ASSERT_EQ(1u, profile.disciplines.size());
	EXPECT_EQ("Novo Armamento", profile.disciplines.front().name);
	EXPECT_EQ(9u, attribute(profile, CharacterAttribute::Strength));
	EXPECT_EQ(1u, player->disciplines().ranks().at(1));
}

TEST_F(PlayerDisciplinesTest, AppliesChangedCatalogContributionsWithoutChangingRanks) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	player->setLevel(2);
	loadCatalog(R"xml(<disciplines><discipline id="1" name="Armamento"><attribute id="des" perLevel="4"/></discipline></disciplines>)xml");

	const auto profile = player->disciplines().profile(player->getLevel());
	EXPECT_EQ(0u, attribute(profile, CharacterAttribute::Strength));
	EXPECT_EQ(8u, attribute(profile, CharacterAttribute::Dexterity));
	EXPECT_EQ(1u, player->disciplines().ranks().at(1));
}

TEST_F(PlayerDisciplinesTest, OmitsRanksMissingFromTheCurrentCatalog) {
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
		{ "2", ValueWrapper(1) },
	});

	const auto profile = player->disciplines().profile(player->getLevel());
	EXPECT_TRUE(profile.disciplines.empty());
	EXPECT_EQ(AttributeTotals {}, profile.attributes);
}

TEST_F(PlayerDisciplinesTest, SaturatesOverflowingDerivedAttributeAndLogsIt) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="Armamento"><attribute id="for" perLevel="4294967295"/></discipline></disciplines>)xml");
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
		{ "1", ValueWrapper(ValueVariant { std::numeric_limits<IntType>::max() }) },
	});

	const auto profile = player->disciplines().profile(std::numeric_limits<uint32_t>::max());
	EXPECT_EQ(std::numeric_limits<uint64_t>::max(), attribute(profile, CharacterAttribute::Strength));
	EXPECT_EQ(0u, attribute(profile, CharacterAttribute::Dexterity));
	EXPECT_GE(logger().logCount(), 1u);
}

TEST_F(PlayerDisciplinesTest, SaturatesAccumulatedDerivedAttributeAndLogsIt) {
	loadCatalog(R"xml(<disciplines>
		<discipline id="1" name="Armamento"><attribute id="for" perLevel="2"/></discipline>
		<discipline id="2" name="Defesa"><attribute id="for" perLevel="2"/></discipline>
	</disciplines>)xml");
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
		{ "1", ValueWrapper(ValueVariant { std::numeric_limits<IntType>::max() }) },
		{ "2", ValueWrapper(ValueVariant { std::numeric_limits<IntType>::max() }) },
	});

	const auto profile = player->disciplines().profile(std::numeric_limits<uint32_t>::max());
	EXPECT_EQ(std::numeric_limits<uint64_t>::max(), attribute(profile, CharacterAttribute::Strength));
	EXPECT_GE(logger().logCount(), 1u);
}
