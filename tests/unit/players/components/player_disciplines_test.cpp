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
#include "config/configmanager.hpp"
#include "items/items.hpp"
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
			temporaryDirectory = std::filesystem::temp_directory_path() / ("canary-player-disciplines-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
			std::filesystem::create_directories(temporaryDirectory);
			configFile = temporaryDirectory / "config.lua";
			writeConfig("");
			configManager().setConfigFileLua(configFile.string());
			ASSERT_TRUE(configManager().load());
			logger().reset();
			player = std::make_shared<Player>();
			loadCatalog();
		}

		void TearDown() override {
			std::error_code error;
			std::filesystem::remove_all(temporaryDirectory, error);
		}

		static KVMemory &kvMemory() {
			return dynamic_cast<KVMemory &>(injector.create<KVStore &>());
		}

		static InMemoryLogger &logger() {
			return dynamic_cast<InMemoryLogger &>(injector.create<Logger &>());
		}

		static ConfigManager &configManager() {
			return injector.create<ConfigManager &>();
		}

		void writeConfig(std::string_view content) const {
			std::ofstream output(configFile);
			ASSERT_TRUE(output.is_open());
			output << content;
		}

		void writeUniformConfig(double value) const {
			std::ofstream output(configFile);
			ASSERT_TRUE(output.is_open());
			for (const auto key : {
					 "characterPotToPhysicalAttackMultiplier",
					 "characterPotToPhysicalDefenseMultiplier",
					 "characterTecToPrecisionMultiplier",
					 "characterVigToMaximumHealthMultiplier",
					 "characterVigToPhysicalDefenseMultiplier",
					 "characterSinToMagicalAttackMultiplier",
					 "characterSinToMagicalDefenseMultiplier",
					 "characterEspToMaximumManaMultiplier",
					 "characterEspToMagicalDefenseMultiplier",
				 }) {
				output << key << " = " << value << '\n';
			}
		}

		bool reloadConfig() const {
			auto &manager = configManager();
			auto* testContainer = DI::getTestContainer();
			DI::setTestContainer(previousTestContainer);
			const auto result = manager.reload();
			DI::setTestContainer(testContainer);
			return result;
		}

		void loadCatalog(std::string_view xml = R"xml(<disciplines><discipline id="1" name="Armamento"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="1"/><attribute id="vig" perLevel="1"/><attribute id="sin" perLevel="0"/><attribute id="esp" perLevel="0"/></discipline></disciplines>)xml") {
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
		std::filesystem::path temporaryDirectory;
		std::filesystem::path configFile;

		inline static di::extension::injector<> injector {};
		inline static di::extension::injector<>* previousTestContainer = nullptr;
	};

	[[nodiscard]] uint64_t attribute(const DisciplineProfile &profile, CharacterAttribute type) {
		return profile.attributes.at(static_cast<size_t>(type));
	}

	[[nodiscard]] uint64_t stat(const DisciplineProfile &profile, DerivedStat type) {
		return profile.stats.at(static_cast<size_t>(type));
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
	EXPECT_EQ(DerivedStatTotals {}, profile.stats);
	EXPECT_TRUE(profile.disciplines.empty());
	EXPECT_FALSE(persistedRanks().has_value());
}

TEST_F(PlayerDisciplinesTest, DerivesAllSevenStatsFromDefaultSnapshot) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="All"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="1"/><attribute id="vig" perLevel="1"/><attribute id="sin" perLevel="1"/><attribute id="esp" perLevel="1"/></discipline></disciplines>)xml");
	player->setLevel(2);
	ASSERT_TRUE(player->disciplines().addRank(1).success());

	EXPECT_EQ((DerivedStatTotals { 2, 2, 2, 2, 2, 10, 10 }), player->disciplines().profile(player->getLevel()).stats);
}

TEST_F(PlayerDisciplinesTest, UsesOneCustomSnapshotForAllSevenStats) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="All"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="1"/><attribute id="vig" perLevel="1"/><attribute id="sin" perLevel="1"/><attribute id="esp" perLevel="1"/></discipline></disciplines>)xml");
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	writeUniformConfig(2.0);
	ASSERT_TRUE(configManager().load());

	EXPECT_EQ((DerivedStatTotals { 2, 2, 2, 4, 4, 2, 2 }), player->disciplines().profile(player->getLevel()).stats);
}

TEST_F(PlayerDisciplinesTest, ZeroSnapshotDisablesEveryDerivedContribution) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	writeUniformConfig(0.0);
	ASSERT_TRUE(configManager().load());

	EXPECT_EQ(DerivedStatTotals {}, player->disciplines().profile(player->getLevel()).stats);
}

TEST_F(PlayerDisciplinesTest, ValidReloadAffectsTheNextProfileWithoutCacheInvalidation) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="All"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="1"/><attribute id="vig" perLevel="1"/><attribute id="sin" perLevel="1"/><attribute id="esp" perLevel="1"/></discipline></disciplines>)xml");
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	writeUniformConfig(1.0);
	ASSERT_TRUE(reloadConfig());
	EXPECT_EQ((DerivedStatTotals { 1, 1, 1, 2, 2, 1, 1 }), player->disciplines().profile(player->getLevel()).stats);

	writeUniformConfig(3.0);
	ASSERT_TRUE(reloadConfig());
	EXPECT_EQ((DerivedStatTotals { 3, 3, 3, 6, 6, 3, 3 }), player->disciplines().profile(player->getLevel()).stats);
}

TEST_F(PlayerDisciplinesTest, InvalidReloadKeepsTheNextProfileOnTheCompletePreviousSnapshot) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="All"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="1"/><attribute id="vig" perLevel="1"/><attribute id="sin" perLevel="1"/><attribute id="esp" perLevel="1"/></discipline></disciplines>)xml");
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	writeUniformConfig(2.0);
	ASSERT_TRUE(configManager().load());
	const auto previous = player->disciplines().profile(player->getLevel()).stats;
	ASSERT_EQ((DerivedStatTotals { 2, 2, 2, 4, 4, 2, 2 }), previous);

	writeConfig(R"lua(
characterPotToPhysicalAttackMultiplier = 9
characterPotToPhysicalDefenseMultiplier = 9
characterTecToPrecisionMultiplier = 9
characterVigToMaximumHealthMultiplier = 9
characterVigToPhysicalDefenseMultiplier = 9
characterSinToMagicalAttackMultiplier = 9
characterSinToMagicalDefenseMultiplier = 9
characterEspToMaximumManaMultiplier = -1
characterEspToMagicalDefenseMultiplier = 9
)lua");
	EXPECT_FALSE(reloadConfig());
	EXPECT_EQ(previous, player->disciplines().profile(player->getLevel()).stats);
}

TEST_F(PlayerDisciplinesTest, RankChangesAffectTheNextStatsWithoutPersistingDerivedValues) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	ASSERT_EQ(1u, kvMemory().writes());
	EXPECT_EQ(1u, stat(player->disciplines().profile(player->getLevel()), DerivedStat::PhysicalAttack));
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	ASSERT_EQ(2u, kvMemory().writes());
	EXPECT_EQ(2u, stat(player->disciplines().profile(player->getLevel()), DerivedStat::PhysicalAttack));
	EXPECT_EQ(2u, kvMemory().writes());
}

TEST_F(PlayerDisciplinesTest, RepeatedProfileReadsDoNotPersistAttributesOrStats) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	const auto persistedBefore = persistedRanks();
	ASSERT_TRUE(persistedBefore.has_value());
	ASSERT_EQ(1u, kvMemory().writes());

	(void)player->disciplines().profile(player->getLevel());
	(void)player->disciplines().profile(player->getLevel());

	EXPECT_EQ(1u, kvMemory().writes());
	EXPECT_EQ(persistedBefore->get<MapType>(), persistedRanks()->get<MapType>());
}

TEST_F(PlayerDisciplinesTest, ConcurrentProfilesObserveOnlyCompleteMultiplierSnapshots) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="All"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="1"/><attribute id="vig" perLevel="1"/><attribute id="sin" perLevel="1"/><attribute id="esp" perLevel="1"/></discipline></disciplines>)xml");
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	writeUniformConfig(1.0);
	ASSERT_TRUE(configManager().load());
	const DerivedStatTotals first { 1, 1, 1, 2, 2, 1, 1 };
	const DerivedStatTotals second { 2, 2, 2, 4, 4, 2, 2 };
	std::atomic_bool stop = false;
	std::atomic_bool mixed = false;
	std::jthread reader([&] {
		while (!stop.load(std::memory_order_acquire)) {
			const auto stats = player->disciplines().profile(player->getLevel()).stats;
			if (stats != first && stats != second) {
				mixed.store(true, std::memory_order_release);
				return;
			}
		}
	});

	for (int iteration = 0; iteration < 10; ++iteration) {
		writeUniformConfig(iteration % 2 == 0 ? 2.0 : 1.0);
		if (!configManager().load()) {
			mixed.store(true, std::memory_order_release);
			break;
		}
	}
	stop.store(true, std::memory_order_release);
	reader.join();
	EXPECT_FALSE(mixed.load(std::memory_order_acquire));
}

TEST_F(PlayerDisciplinesTest, SaturatedStatsReturnPublicMaximumAndLogPlayerAndStatus) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="Armamento"><attribute id="pot" perLevel="4294967295"/></discipline></disciplines>)xml");
	player->setName("Overflow Hero");
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
												  { "1", ValueWrapper(ValueVariant { std::numeric_limits<IntType>::max() }) },
											  });
	logger().reset();

	const auto profile = player->disciplines().profile(std::numeric_limits<uint32_t>::max());
	EXPECT_EQ(maxPublicDerivedStat, stat(profile, DerivedStat::PhysicalAttack));
	EXPECT_TRUE(std::ranges::any_of(logger().logs, [](const auto &entry) {
		return entry.message.find("[CharacterDerivedStats] player=Overflow Hero") != std::string::npos && entry.message.find("status=physicalAttack") != std::string::npos && entry.message.find("reason=derived status overflow") != std::string::npos;
	}));
}

TEST_F(PlayerDisciplinesTest, ProfileCalculationLeavesRuntimeHealthAndManaUntouched) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	const auto health = player->getHealth();
	const auto maximumHealth = player->getMaxHealth();
	const auto mana = player->getMana();
	const auto maximumMana = player->getMaxMana();

	const auto profile = player->disciplines().profile(player->getLevel());
	EXPECT_GT(stat(profile, DerivedStat::MaximumHealth), 0u);
	EXPECT_EQ(health, player->getHealth());
	EXPECT_EQ(maximumHealth, player->getMaxHealth());
	EXPECT_EQ(mana, player->getMana());
	EXPECT_EQ(maximumMana, player->getMaxMana());
}

TEST_F(PlayerDisciplinesTest, ProfileCalculationLeavesCombatBehaviorUntouched) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="All"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="1"/><attribute id="vig" perLevel="1"/><attribute id="sin" perLevel="1"/><attribute id="esp" perLevel="1"/></discipline></disciplines>)xml");
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	player->setTestVocation(std::make_shared<Vocation>(1));

	CombatDamage damage;
	damage.primary.type = COMBAT_PHYSICALDAMAGE;
	damage.primary.value = 137;
	damage.critical = true;
	damage.criticalChance = 23;
	player->setCombatDamage(damage);
	ItemType accuracyFixture;

	const auto damageBefore = player->getCombatDamage();
	const auto mitigationBefore = player->getMitigation();
	const auto armorBefore = player->getArmor();
	const auto defenseBefore = player->getDefense();
	const auto accuracyBefore = player->getDamageAccuracy(accuracyFixture);
	const auto evasionBefore = player->getDodgeChance();
	const auto criticalChanceBefore = player->getBaseCritical().chance;
	const auto criticalDamageBefore = player->getBaseCritical().damage;

	const auto profile = player->disciplines().profile(player->getLevel());
	EXPECT_GT(stat(profile, DerivedStat::PhysicalAttack), 0u);
	EXPECT_GT(stat(profile, DerivedStat::MagicalAttack), 0u);
	EXPECT_GT(stat(profile, DerivedStat::Precision), 0u);
	EXPECT_GT(stat(profile, DerivedStat::PhysicalDefense), 0u);
	EXPECT_GT(stat(profile, DerivedStat::MagicalDefense), 0u);
	const auto damageAfter = player->getCombatDamage();
	EXPECT_EQ(damageBefore.primary.type, damageAfter.primary.type);
	EXPECT_EQ(damageBefore.primary.value, damageAfter.primary.value);
	EXPECT_EQ(damageBefore.critical, damageAfter.critical);
	EXPECT_EQ(damageBefore.criticalChance, damageAfter.criticalChance);
	EXPECT_EQ(mitigationBefore, player->getMitigation());
	EXPECT_EQ(armorBefore, player->getArmor());
	EXPECT_EQ(defenseBefore, player->getDefense());
	EXPECT_EQ(accuracyBefore, player->getDamageAccuracy(accuracyFixture));
	EXPECT_EQ(evasionBefore, player->getDodgeChance());
	EXPECT_EQ(criticalChanceBefore, player->getBaseCritical().chance);
	EXPECT_EQ(criticalDamageBefore, player->getBaseCritical().damage);
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
	EXPECT_EQ(14u, attribute(profile, CharacterAttribute::Potency));
	EXPECT_EQ(14u, attribute(profile, CharacterAttribute::Technique));
	EXPECT_EQ(14u, attribute(profile, CharacterAttribute::Vigor));
	EXPECT_EQ(0u, attribute(profile, CharacterAttribute::Attunement));
	EXPECT_EQ(0u, attribute(profile, CharacterAttribute::Spirit));
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
	EXPECT_EQ(3u, attribute(player->disciplines().profile(player->getLevel()), CharacterAttribute::Potency));
	player->setLevel(8);
	EXPECT_EQ(8u, attribute(player->disciplines().profile(player->getLevel()), CharacterAttribute::Potency));
	EXPECT_EQ(1u, kvMemory().writes());
}

TEST_F(PlayerDisciplinesTest, DerivesProfileFromRanksLoadedFromKv) {
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
														  { "1", ValueWrapper(2) },
													  });
	player->setLevel(4);

	const auto profile = player->disciplines().profile(player->getLevel());
	EXPECT_EQ(8u, attribute(profile, CharacterAttribute::Potency));
	ASSERT_EQ(1u, profile.disciplines.size());
	EXPECT_EQ(2u, profile.disciplines.front().rank);
}

TEST_F(PlayerDisciplinesTest, SumsMultipleDisciplinesInAscendingIdOrder) {
	loadCatalog(R"xml(<disciplines>
		<discipline id="2" name="Defesa"><attribute id="vig" perLevel="2"/><attribute id="esp" perLevel="1"/></discipline>
		<discipline id="1" name="Armamento"><attribute id="pot" perLevel="1"/></discipline>
	</disciplines>)xml");
	player->setLevel(5);
	ASSERT_TRUE(player->disciplines().addRank(2).success());
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	ASSERT_TRUE(player->disciplines().addRank(1).success());

	const auto profile = player->disciplines().profile(player->getLevel());
	ASSERT_EQ(2u, profile.disciplines.size());
	EXPECT_EQ(1u, profile.disciplines[0].id);
	EXPECT_EQ(2u, profile.disciplines[1].id);
	EXPECT_EQ(10u, attribute(profile, CharacterAttribute::Potency));
	EXPECT_EQ(10u, attribute(profile, CharacterAttribute::Vigor));
	EXPECT_EQ(5u, attribute(profile, CharacterAttribute::Spirit));
}

TEST_F(PlayerDisciplinesTest, UsesRenamedCatalogEntryWithoutChangingRanks) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	player->setLevel(3);
	loadCatalog(R"xml(<disciplines><discipline id="1" name="Novo Armamento"><attribute id="pot" perLevel="3"/></discipline></disciplines>)xml");

	const auto profile = player->disciplines().profile(player->getLevel());
	ASSERT_EQ(1u, profile.disciplines.size());
	EXPECT_EQ("Novo Armamento", profile.disciplines.front().name);
	EXPECT_EQ(9u, attribute(profile, CharacterAttribute::Potency));
	EXPECT_EQ(1u, player->disciplines().ranks().at(1));
}

TEST_F(PlayerDisciplinesTest, AppliesChangedCatalogContributionsWithoutChangingRanks) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	player->setLevel(2);
	loadCatalog(R"xml(<disciplines><discipline id="1" name="Armamento"><attribute id="tec" perLevel="4"/></discipline></disciplines>)xml");

	const auto profile = player->disciplines().profile(player->getLevel());
	EXPECT_EQ(0u, attribute(profile, CharacterAttribute::Potency));
	EXPECT_EQ(8u, attribute(profile, CharacterAttribute::Technique));
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
	loadCatalog(R"xml(<disciplines><discipline id="1" name="Armamento"><attribute id="pot" perLevel="4294967295"/></discipline></disciplines>)xml");
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
														  { "1", ValueWrapper(ValueVariant { std::numeric_limits<IntType>::max() }) },
													  });

	const auto profile = player->disciplines().profile(std::numeric_limits<uint32_t>::max());
	EXPECT_EQ(std::numeric_limits<uint64_t>::max(), attribute(profile, CharacterAttribute::Potency));
	EXPECT_EQ(0u, attribute(profile, CharacterAttribute::Technique));
	EXPECT_GE(logger().logCount(), 1u);
}

TEST_F(PlayerDisciplinesTest, SaturatesAccumulatedDerivedAttributeAndLogsIt) {
	loadCatalog(R"xml(<disciplines>
		<discipline id="1" name="Armamento"><attribute id="pot" perLevel="2"/></discipline>
		<discipline id="2" name="Defesa"><attribute id="pot" perLevel="2"/></discipline>
	</disciplines>)xml");
	player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
														  { "1", ValueWrapper(ValueVariant { std::numeric_limits<IntType>::max() }) },
														  { "2", ValueWrapper(ValueVariant { std::numeric_limits<IntType>::max() }) },
													  });

	const auto profile = player->disciplines().profile(std::numeric_limits<uint32_t>::max());
	EXPECT_EQ(std::numeric_limits<uint64_t>::max(), attribute(profile, CharacterAttribute::Potency));
	EXPECT_GE(logger().logCount(), 1u);
}
