/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "server/signals.hpp"

#include <gtest/gtest.h>

#include "lib/di/container.hpp"
#include "lib/logging/in_memory_logger.hpp"

namespace {
	class ConfigReloadSignalTest : public ::testing::Test {
	protected:
		void SetUp() override {
			logger().reset();
		}

		static InMemoryLogger &logger() {
			return dynamic_cast<InMemoryLogger &>(DI::get<Logger>());
		}
	};
} // namespace

TEST_F(ConfigReloadSignalTest, ReportsSuccessfulReloadOnlyForSuccess) {
	reportConfigReload(true);
	ASSERT_EQ(1u, logger().logCount());
	const auto [level, message] = logger().getLogEntry(0);
	EXPECT_EQ("info", level);
	EXPECT_EQ("Reloaded config", message);
}

TEST_F(ConfigReloadSignalTest, ReportsFailureWithoutSuccessMessage) {
	reportConfigReload(false);
	ASSERT_EQ(1u, logger().logCount());
	const auto [level, message] = logger().getLogEntry(0);
	EXPECT_EQ("error", level);
	EXPECT_EQ("Failed to reload config", message);
}
