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
	#include <cstdint>
	#include <memory>
	#include <string_view>
#endif

class InstantSpell;

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
};

[[nodiscard]] std::string_view preparedCastInterruptReason(PreparedCastInterruptReason reason);
