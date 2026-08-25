/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once

#include "lua/global/lua_variant.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <algorithm>
	#include <chrono>
	#include <cstdint>
	#include <memory>
	#include <optional>
	#include <string_view>
#endif

class InstantSpell;

using PreparedCastClock = std::chrono::steady_clock;

struct PreparedCastSnapshot {
	uint64_t id;
	uint32_t durationMs;
	uint32_t remainingMs;
};

struct PreparedCastConfig {
	uint32_t durationMs = 0;
	bool lockMovement = false;
	bool lockDirection = false;
	bool interruptOnPositionChange = false;
};

struct PreparedCastContext {
	PreparedCastContext(uint64_t id = 0, Position origin = {}, Direction direction = DIRECTION_NORTH) :
		id(id), origin(origin), direction(direction) { }

	const uint64_t id;
	const Position origin;
	const Direction direction;
};

enum class PreparedCastInterruptReason : uint8_t {
	PositionChange,
	Death,
	Logout,
	Removal,
	SchedulerRejected,
};

struct PreparedCastState {
	PreparedCastConfig config;
	PreparedCastContext context;
	LuaVariant variant;
	std::weak_ptr<const InstantSpell> spell;
	uint64_t completionEventId = 0;
	PreparedCastClock::time_point deadline;

	[[nodiscard]] std::optional<PreparedCastSnapshot> snapshotAt(PreparedCastClock::time_point now) const {
		if (config.durationMs == 0 || now >= deadline) {
			return std::nullopt;
		}

		const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - now).count();
		return PreparedCastSnapshot {
			.id = context.id,
			.durationMs = config.durationMs,
			.remainingMs = static_cast<uint32_t>(std::min<int64_t>(remaining, config.durationMs)),
		};
	}
};

[[nodiscard]] std::string_view preparedCastInterruptReason(PreparedCastInterruptReason reason);
