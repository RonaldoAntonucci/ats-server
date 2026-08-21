/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "config/configmanager.hpp"

#include <gtest/gtest.h>

#include "lib/di/container.hpp"
#include "lib/logging/in_memory_logger.hpp"

namespace {
	constexpr std::array<std::string_view, 9> multiplierKeys = {
		"characterPotToPhysicalAttackMultiplier",
		"characterPotToPhysicalDefenseMultiplier",
		"characterTecToPrecisionMultiplier",
		"characterVigToMaximumHealthMultiplier",
		"characterVigToPhysicalDefenseMultiplier",
		"characterSinToMagicalAttackMultiplier",
		"characterSinToMagicalDefenseMultiplier",
		"characterEspToMaximumManaMultiplier",
		"characterEspToMagicalDefenseMultiplier",
	};

	class ConfigManagerDerivedStatsTest : public ::testing::Test {
	protected:
		void SetUp() override {
			logger().reset();
			temporaryDirectory = std::filesystem::temp_directory_path() / ("canary-derived-stats-config-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
			std::filesystem::create_directories(temporaryDirectory);
			file = temporaryDirectory / "config.lua";
			manager.setConfigFileLua(file.string());
		}

		void TearDown() override {
			std::error_code error;
			std::filesystem::remove_all(temporaryDirectory, error);
		}

		void write(std::string_view content) const {
			std::ofstream output(file);
			ASSERT_TRUE(output.is_open());
			output << content;
		}

		void writeUniform(double value, std::string_view extra = {}) const {
			std::ofstream output(file);
			ASSERT_TRUE(output.is_open());
			for (const auto key : multiplierKeys) {
				output << key << " = " << value << '\n';
			}
			output << extra;
		}

		[[nodiscard]] bool hasLog(std::string_view level, std::string_view key, std::string_view reason) const {
			return std::ranges::any_of(logger().logs, [&](const auto &entry) {
				return entry.level == level && entry.message.find(key) != std::string::npos && entry.message.find(reason) != std::string::npos;
			});
		}

		static InMemoryLogger &logger() {
			return dynamic_cast<InMemoryLogger &>(DI::get<Logger>());
		}

		ConfigManager manager;
		std::filesystem::path temporaryDirectory;
		std::filesystem::path file;
	};
} // namespace

TEST_F(ConfigManagerDerivedStatsTest, LoadsAllNineDefaultsWhenKeysAreMissing) {
	write("");
	ASSERT_TRUE(manager.load());
	const auto values = manager.getDerivedStatMultipliers();
	EXPECT_DOUBLE_EQ(1.0, values->potToPhysicalAttack);
	EXPECT_DOUBLE_EQ(0.3, values->potToPhysicalDefense);
	EXPECT_DOUBLE_EQ(1.0, values->tecToPrecision);
	EXPECT_DOUBLE_EQ(5.0, values->vigToMaximumHealth);
	EXPECT_DOUBLE_EQ(0.7, values->vigToPhysicalDefense);
	EXPECT_DOUBLE_EQ(1.0, values->sinToMagicalAttack);
	EXPECT_DOUBLE_EQ(0.3, values->sinToMagicalDefense);
	EXPECT_DOUBLE_EQ(5.0, values->espToMaximumMana);
	EXPECT_DOUBLE_EQ(0.7, values->espToMagicalDefense);
}

TEST_F(ConfigManagerDerivedStatsTest, WarnsWithEveryMissingKey) {
	write("");
	ASSERT_TRUE(manager.load());
	for (const auto key : multiplierKeys) {
		EXPECT_TRUE(hasLog("warning", key, "reason=missing")) << key;
	}
}

TEST_F(ConfigManagerDerivedStatsTest, LoadsNineIndependentCustomValues) {
	write(R"lua(
characterPotToPhysicalAttackMultiplier = 1.1
characterPotToPhysicalDefenseMultiplier = 2.2
characterTecToPrecisionMultiplier = 3.3
characterVigToMaximumHealthMultiplier = 4.4
characterVigToPhysicalDefenseMultiplier = 5.5
characterSinToMagicalAttackMultiplier = 6.6
characterSinToMagicalDefenseMultiplier = 7.7
characterEspToMaximumManaMultiplier = 8.8
characterEspToMagicalDefenseMultiplier = 9.9
)lua");
	ASSERT_TRUE(manager.load());
	const auto values = manager.getDerivedStatMultipliers();
	EXPECT_DOUBLE_EQ(1.1, values->potToPhysicalAttack);
	EXPECT_DOUBLE_EQ(2.2, values->potToPhysicalDefense);
	EXPECT_DOUBLE_EQ(3.3, values->tecToPrecision);
	EXPECT_DOUBLE_EQ(4.4, values->vigToMaximumHealth);
	EXPECT_DOUBLE_EQ(5.5, values->vigToPhysicalDefense);
	EXPECT_DOUBLE_EQ(6.6, values->sinToMagicalAttack);
	EXPECT_DOUBLE_EQ(7.7, values->sinToMagicalDefense);
	EXPECT_DOUBLE_EQ(8.8, values->espToMaximumMana);
	EXPECT_DOUBLE_EQ(9.9, values->espToMagicalDefense);
}

TEST_F(ConfigManagerDerivedStatsTest, AcceptsZeroForOneRelationship) {
	write("characterPotToPhysicalAttackMultiplier = 0\n");
	ASSERT_TRUE(manager.load());
	const auto values = manager.getDerivedStatMultipliers();
	EXPECT_DOUBLE_EQ(0.0, values->potToPhysicalAttack);
	EXPECT_DOUBLE_EQ(0.3, values->potToPhysicalDefense);
}

TEST_F(ConfigManagerDerivedStatsTest, UsesOnlyTheMissingRelationshipDefault) {
	write(R"lua(
characterPotToPhysicalDefenseMultiplier = 2
characterTecToPrecisionMultiplier = 2
characterVigToMaximumHealthMultiplier = 2
characterVigToPhysicalDefenseMultiplier = 2
characterSinToMagicalAttackMultiplier = 2
characterSinToMagicalDefenseMultiplier = 2
characterEspToMaximumManaMultiplier = 2
characterEspToMagicalDefenseMultiplier = 2
)lua");
	ASSERT_TRUE(manager.load());
	const auto values = manager.getDerivedStatMultipliers();
	EXPECT_DOUBLE_EQ(1.0, values->potToPhysicalAttack);
	EXPECT_DOUBLE_EQ(2.0, values->potToPhysicalDefense);
	EXPECT_TRUE(hasLog("warning", multiplierKeys.front(), "reason=missing"));
}

TEST_F(ConfigManagerDerivedStatsTest, RejectsWrongLuaType) {
	write("characterPotToPhysicalAttackMultiplier = {}\n");
	EXPECT_FALSE(manager.load());
	EXPECT_TRUE(hasLog("error", multiplierKeys.front(), "reason=wrong type"));
}

TEST_F(ConfigManagerDerivedStatsTest, RejectsNumericStringAsWrongLuaType) {
	write("characterPotToPhysicalAttackMultiplier = '2.5'\n");
	EXPECT_FALSE(manager.load());
	EXPECT_TRUE(hasLog("error", multiplierKeys.front(), "reason=wrong type"));
}

TEST_F(ConfigManagerDerivedStatsTest, RejectsNegativeValue) {
	write("characterTecToPrecisionMultiplier = -0.1\n");
	EXPECT_FALSE(manager.load());
	EXPECT_TRUE(hasLog("error", multiplierKeys[2], "reason=negative value"));
}

TEST_F(ConfigManagerDerivedStatsTest, RejectsNaN) {
	write("characterVigToMaximumHealthMultiplier = 0 / 0\n");
	EXPECT_FALSE(manager.load());
	EXPECT_TRUE(hasLog("error", multiplierKeys[3], "reason=non-finite value"));
}

TEST_F(ConfigManagerDerivedStatsTest, RejectsInfinity) {
	write("characterEspToMaximumManaMultiplier = math.huge\n");
	EXPECT_FALSE(manager.load());
	EXPECT_TRUE(hasLog("error", multiplierKeys[7], "reason=non-finite value"));
}

TEST_F(ConfigManagerDerivedStatsTest, InvalidInitialLoadDoesNotMarkManagerLoaded) {
	write("characterSinToMagicalAttackMultiplier = false\n");
	EXPECT_FALSE(manager.load());
	EXPECT_FALSE(manager.isLoaded());
	EXPECT_DOUBLE_EQ(1.0, manager.getDerivedStatMultipliers()->sinToMagicalAttack);
}

TEST_F(ConfigManagerDerivedStatsTest, InvalidLoadPreservesThePublishedSnapshot) {
	writeUniform(2.0);
	ASSERT_TRUE(manager.load());
	const auto before = manager.getDerivedStatMultipliers();
	write("characterPotToPhysicalDefenseMultiplier = -1\n");
	EXPECT_FALSE(manager.load());
	const auto after = manager.getDerivedStatMultipliers();
	EXPECT_EQ(before, after);
	EXPECT_DOUBLE_EQ(2.0, after->potToPhysicalDefense);
}

TEST_F(ConfigManagerDerivedStatsTest, InvalidReloadPreservesGenericConfigurationAndSnapshot) {
	writeUniform(2.0, "serverName = 'before'\n");
	ASSERT_TRUE(manager.load());
	const auto before = manager.getDerivedStatMultipliers();
	write("characterPotToPhysicalAttackMultiplier = -1\nserverName = 'after'\n");
	EXPECT_FALSE(manager.reload());
	EXPECT_EQ(before, manager.getDerivedStatMultipliers());
	EXPECT_EQ("before", manager.getString(SERVER_NAME));
}

TEST_F(ConfigManagerDerivedStatsTest, SuccessfulLoadPublishesOneCompleteImmutableSnapshot) {
	writeUniform(2.0);
	ASSERT_TRUE(manager.load());
	const auto before = manager.getDerivedStatMultipliers();
	writeUniform(3.0);
	ASSERT_TRUE(manager.load());
	const auto after = manager.getDerivedStatMultipliers();
	EXPECT_NE(before, after);
	EXPECT_DOUBLE_EQ(2.0, before->potToPhysicalAttack);
	EXPECT_DOUBLE_EQ(2.0, before->espToMagicalDefense);
	EXPECT_DOUBLE_EQ(3.0, after->potToPhysicalAttack);
	EXPECT_DOUBLE_EQ(3.0, after->espToMagicalDefense);
}

TEST_F(ConfigManagerDerivedStatsTest, ConcurrentReadersNeverObserveMixedSnapshots) {
	writeUniform(1.0);
	ASSERT_TRUE(manager.load());
	std::atomic_bool stop = false;
	std::atomic_bool mixed = false;
	std::jthread reader([&] {
		while (!stop.load(std::memory_order_acquire)) {
			const auto values = manager.getDerivedStatMultipliers();
			const auto first = values->potToPhysicalAttack;
			if (values->potToPhysicalDefense != first || values->tecToPrecision != first || values->vigToMaximumHealth != first || values->vigToPhysicalDefense != first || values->sinToMagicalAttack != first || values->sinToMagicalDefense != first || values->espToMaximumMana != first || values->espToMagicalDefense != first) {
				mixed.store(true, std::memory_order_release);
				return;
			}
		}
	});

	for (int iteration = 0; iteration < 10; ++iteration) {
		writeUniform(iteration % 2 == 0 ? 2.0 : 1.0);
		if (!manager.load()) {
			stop.store(true, std::memory_order_release);
			reader.join();
			FAIL() << "valid candidate failed during concurrent publication";
		}
	}
	stop.store(true, std::memory_order_release);
	reader.join();
	EXPECT_FALSE(mixed.load(std::memory_order_acquire));
}
