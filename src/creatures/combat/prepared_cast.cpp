/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/prepared_cast.hpp"

std::string_view preparedCastInterruptReason(PreparedCastInterruptReason reason) {
	switch (reason) {
		case PreparedCastInterruptReason::PositionChange:
			return "position_change";
		case PreparedCastInterruptReason::Death:
			return "death";
		case PreparedCastInterruptReason::Logout:
			return "logout";
		case PreparedCastInterruptReason::Removal:
			return "removal";
		case PreparedCastInterruptReason::SchedulerRejected:
			return "scheduler_rejected";
	}

	return "scheduler_rejected";
}
