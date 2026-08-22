/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/spells.hpp"

#include "config/configmanager.hpp"
#include "creatures/players/grouping/groups.hpp"
#include "creatures/players/player.hpp"
#include "enums/account_group_type.hpp"
#include "kv/value_wrapper.hpp"

#include <gtest/gtest.h>

#include "kv/in_memory_kv.hpp"
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

		bool check(const std::shared_ptr<Player> &player) const {
			return playerSpellCheck(player);
		}
	};

	class SpellRequirementsTest : public ::testing::Test {
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
			temporaryDirectory = std::filesystem::temp_directory_path() / ("canary-spell-requirements-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
			std::filesystem::create_directories(temporaryDirectory);
			const auto configFile = temporaryDirectory / "config.lua";
			std::ofstream config(configFile);
			config << "toggleLearnSpells = true\nworldType = \"retro-pvp\"\n";
			config.close();
			configManager().setConfigFileLua(configFile.string());
			ASSERT_TRUE(configManager().load());

			const auto catalogFile = temporaryDirectory / "disciplines.xml";
			std::ofstream catalog(catalogFile);
			catalog << R"xml(<disciplines><discipline id="1" name="Armamento"/><discipline id="2" name="Second"/></disciplines>)xml";
			catalog.close();
			ASSERT_TRUE(g_disciplines().loadFromXml(catalogFile));

			player = std::make_shared<Player>();
			auto group = std::make_shared<Group>();
			group->id = GROUP_TYPE_NORMAL;
			group->access = false;
			player->setGroup(group);
			player->setLevel(1);
		}

		void TearDown() override {
			std::error_code error;
			std::filesystem::remove_all(temporaryDirectory, error);
		}

		static KVMemory &kvMemory() {
			return dynamic_cast<KVMemory &>(injector.create<KVStore &>());
		}

		static ConfigManager &configManager() {
			return injector.create<ConfigManager &>();
		}

		void setRank(uint16_t id, uint32_t rank) const {
			player->kv()->scoped("disciplines")->set("ranks", ValueWrapper {
																  { std::to_string(id), ValueWrapper(static_cast<IntType>(rank)) },
															  });
		}

		std::shared_ptr<Player> player;
		std::filesystem::path temporaryDirectory;

		inline static di::extension::injector<> injector {};
		inline static di::extension::injector<>* previousTestContainer = nullptr;
	};
}

TEST_F(SpellRequirementsTest, AcceptsAValidDisciplineRequirement) {
	SpellRequirementSet requirements;
	EXPECT_EQ(SpellRequirementDefinitionResult::Added, requirements.addDiscipline(1, 1));
	ASSERT_EQ(1u, requirements.disciplines().size());
	EXPECT_EQ((DisciplineRequirement { 1, 1 }), requirements.disciplines().front());
}

TEST_F(SpellRequirementsTest, AllowsAnEmptyRequirementSet) {
	SpellRequirementSet requirements;
	EXPECT_EQ(SpellRequirementResult::Allowed, requirements.evaluate(*player));
}

TEST_F(SpellRequirementsTest, AppliesAllOfSemantics) {
	setRank(1, 2);
	ASSERT_TRUE(player->disciplines().addRank(2).success());
	SpellRequirementSet requirements;
	ASSERT_EQ(SpellRequirementDefinitionResult::Added, requirements.addDiscipline(1, 2));
	ASSERT_EQ(SpellRequirementDefinitionResult::Added, requirements.addDiscipline(2, 1));
	EXPECT_EQ(SpellRequirementResult::Allowed, requirements.evaluate(*player));
}

TEST_F(SpellRequirementsTest, RejectsAMissingRequiredDiscipline) {
	SpellRequirementSet requirements;
	ASSERT_EQ(SpellRequirementDefinitionResult::Added, requirements.addDiscipline(1, 1));
	EXPECT_EQ(SpellRequirementResult::DisciplineRequirementNotMet, requirements.evaluate(*player));
}

TEST_F(SpellRequirementsTest, RejectsAnInsufficientRank) {
	setRank(1, 1);
	SpellRequirementSet requirements;
	ASSERT_EQ(SpellRequirementDefinitionResult::Added, requirements.addDiscipline(1, 2));
	EXPECT_EQ(SpellRequirementResult::DisciplineRequirementNotMet, requirements.evaluate(*player));
}

TEST_F(SpellRequirementsTest, AcceptsTheExactRankBoundary) {
	setRank(1, 1);
	SpellRequirementSet requirements;
	ASSERT_EQ(SpellRequirementDefinitionResult::Added, requirements.addDiscipline(1, 1));
	EXPECT_EQ(SpellRequirementResult::Allowed, requirements.evaluate(*player));
}

TEST_F(SpellRequirementsTest, RankRemovalFailsTheNextEvaluation) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	SpellRequirementSet requirements;
	ASSERT_EQ(SpellRequirementDefinitionResult::Added, requirements.addDiscipline(1, 1));
	ASSERT_EQ(SpellRequirementResult::Allowed, requirements.evaluate(*player));
	ASSERT_TRUE(player->disciplines().removeRank(1).success());
	EXPECT_EQ(SpellRequirementResult::DisciplineRequirementNotMet, requirements.evaluate(*player));
}

TEST_F(SpellRequirementsTest, CollapsesAnIdenticalDuplicate) {
	SpellRequirementSet requirements;
	ASSERT_EQ(SpellRequirementDefinitionResult::Added, requirements.addDiscipline(1, 1));
	EXPECT_EQ(SpellRequirementDefinitionResult::Duplicate, requirements.addDiscipline(1, 1));
	EXPECT_EQ(1u, requirements.disciplines().size());
}

TEST_F(SpellRequirementsTest, RejectsAConflictingDuplicate) {
	SpellRequirementSet requirements;
	ASSERT_EQ(SpellRequirementDefinitionResult::Added, requirements.addDiscipline(1, 1));
	EXPECT_EQ(SpellRequirementDefinitionResult::ConflictingDuplicate, requirements.addDiscipline(1, 2));
	ASSERT_EQ(1u, requirements.disciplines().size());
	EXPECT_EQ(1u, requirements.disciplines().front().minimumRank);
}

TEST_F(SpellRequirementsTest, RejectsUnknownAndInvalidDefinitions) {
	SpellRequirementSet requirements;
	EXPECT_EQ(SpellRequirementDefinitionResult::InvalidDisciplineId, requirements.addDiscipline(0, 1));
	EXPECT_EQ(SpellRequirementDefinitionResult::InvalidMinimumRank, requirements.addDiscipline(1, 0));
	EXPECT_EQ(SpellRequirementDefinitionResult::UnknownDiscipline, requirements.addDiscipline(99, 1));
	EXPECT_TRUE(requirements.empty());
}

TEST_F(SpellRequirementsTest, UsesTheStableFailureReason) {
	EXPECT_EQ("discipline_requirement_not_met", spellRequirementReason(SpellRequirementResult::DisciplineRequirementNotMet));
}

TEST_F(SpellRequirementsTest, DisciplinePolicySkipsLegacyEligibilityGates) {
	ASSERT_TRUE(player->disciplines().addRank(1).success());
	TestSpell spell;
	spell.setName("Synthetic Discipline Spell");
	spell.setLevel(1000);
	spell.setMagicLevel(1000);
	spell.setNeedLearn(true);
	spell.addVocMap(999, false);
	ASSERT_EQ(SpellRequirementDefinitionResult::Added, spell.addDisciplineRequirement(1, 1));
	EXPECT_TRUE(spell.check(player));
}

TEST_F(SpellRequirementsTest, IgnoreSpellCheckStillBypassesDisciplinePolicy) {
	player->setFlag(PlayerFlags_t::IgnoreSpellCheck);
	TestSpell spell;
	ASSERT_EQ(SpellRequirementDefinitionResult::Added, spell.addDisciplineRequirement(1, 1));
	EXPECT_TRUE(spell.check(player));
}

TEST_F(SpellRequirementsTest, GeneralChecksStillPrecedeIgnoreSpellCheck) {
	player->setFlag(PlayerFlags_t::IgnoreSpellCheck);
	player->setFlag(PlayerFlags_t::CannotUseSpells);
	TestSpell spell;
	ASSERT_EQ(SpellRequirementDefinitionResult::Added, spell.addDisciplineRequirement(1, 1));
	EXPECT_FALSE(spell.check(player));
}
