/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "config/configmanager.hpp"
#include "creatures/players/disciplines/discipline.hpp"
#include "creatures/players/player.hpp"

#include <gtest/gtest.h>

#include "kv/in_memory_kv.hpp"
#include "lib/logging/in_memory_logger.hpp"

namespace {
	class PlayerCharacterStatsTest : public ::testing::Test {
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
			configFile = std::filesystem::temp_directory_path() / ("canary-player-character-stats-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".lua");
			std::ofstream output(configFile);
			ASSERT_TRUE(output.is_open());
			output.close();
			configManager().setConfigFileLua(configFile.string());
			ASSERT_TRUE(configManager().load());
			logger().reset();
			player = std::make_shared<Player>();
			loadCatalog();
		}

		void TearDown() override {
			std::error_code error;
			std::filesystem::remove(configFile, error);
		}

		void loadCatalog(std::string_view xml = R"xml(<disciplines><discipline id="1" name="Armamento"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="1"/><attribute id="vig" perLevel="1"/><attribute id="sin" perLevel="0"/><attribute id="esp" perLevel="0"/></discipline></disciplines>)xml") {
			const auto file = std::filesystem::temp_directory_path() / "canary-player-character-stats.xml";
			std::ofstream output(file);
			output << xml;
			output.close();
			ASSERT_TRUE(g_disciplines().loadFromXml(file));
			std::error_code error;
			std::filesystem::remove(file, error);
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

		std::shared_ptr<Player> player;
		std::filesystem::path configFile;

		inline static di::extension::injector<> injector {};
		inline static di::extension::injector<>* previousTestContainer = nullptr;
	};
} // namespace

TEST_F(PlayerCharacterStatsTest, AggregatesEveryDisciplineAttributeAndMatchesScalarReads) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="All"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="2"/><attribute id="vig" perLevel="3"/><attribute id="sin" perLevel="4"/><attribute id="esp" perLevel="5"/></discipline></disciplines>)xml");
	player->setLevel(2);
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	ASSERT_TRUE(player->disciplines().addRank(1).success());

	const AttributeTotals expected { 4, 8, 12, 16, 20 };
	EXPECT_EQ(expected, player->characterStats().attributes());
	EXPECT_EQ(4u, player->characterStats().attribute(CharacterAttribute::Potency));
	EXPECT_EQ(8u, player->characterStats().attribute(CharacterAttribute::Technique));
	EXPECT_EQ(12u, player->characterStats().attribute(CharacterAttribute::Vigor));
	EXPECT_EQ(16u, player->characterStats().attribute(CharacterAttribute::Attunement));
	EXPECT_EQ(20u, player->characterStats().attribute(CharacterAttribute::Spirit));
}

TEST_F(PlayerCharacterStatsTest, CalculatesEveryDerivedStatAndMatchesScalarReads) {
	loadCatalog(R"xml(<disciplines><discipline id="1" name="All"><attribute id="pot" perLevel="1"/><attribute id="tec" perLevel="1"/><attribute id="vig" perLevel="1"/><attribute id="sin" perLevel="1"/><attribute id="esp" perLevel="1"/></discipline></disciplines>)xml");
	player->setLevel(2);
	ASSERT_TRUE(player->disciplines().addRank(1).success());

	const DerivedStatTotals expected { 2, 2, 2, 2, 2, 10, 10 };
	EXPECT_EQ(expected, player->characterStats().stats());
	EXPECT_EQ(2u, player->characterStats().stat(DerivedStat::PhysicalAttack));
	EXPECT_EQ(2u, player->characterStats().stat(DerivedStat::MagicalAttack));
	EXPECT_EQ(2u, player->characterStats().stat(DerivedStat::Precision));
	EXPECT_EQ(2u, player->characterStats().stat(DerivedStat::PhysicalDefense));
	EXPECT_EQ(2u, player->characterStats().stat(DerivedStat::MagicalDefense));
	EXPECT_EQ(10u, player->characterStats().stat(DerivedStat::MaximumHealth));
	EXPECT_EQ(10u, player->characterStats().stat(DerivedStat::MaximumMana));
}

TEST_F(PlayerCharacterStatsTest, ReadsDoNotPersistDerivedValuesOrMutateRuntimeResources) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	const auto writes = kvMemory().writes();
	const auto health = player->getHealth();
	const auto maximumHealth = player->getMaxHealth();
	const auto mana = player->getMana();
	const auto maximumMana = player->getMaxMana();

	(void)player->characterStats().attributes();
	(void)player->characterStats().stats();
	(void)player->characterStats().attribute(CharacterAttribute::Potency);
	(void)player->characterStats().stat(DerivedStat::MaximumHealth);

	EXPECT_EQ(writes, kvMemory().writes());
	EXPECT_EQ(health, player->getHealth());
	EXPECT_EQ(maximumHealth, player->getMaxHealth());
	EXPECT_EQ(mana, player->getMana());
	EXPECT_EQ(maximumMana, player->getMaxMana());
}
