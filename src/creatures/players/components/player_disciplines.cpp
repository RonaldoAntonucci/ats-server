/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/components/player_disciplines.hpp"

#include "creatures/players/disciplines/discipline.hpp"
#include "creatures/players/player.hpp"
#include "kv/kv.hpp"
#include "kv/value_wrapper.hpp"
#include "lib/logging/logger.hpp"

#include <charconv>
#include <limits>

namespace {
	constexpr std::string_view ranksKey = "ranks";

	[[nodiscard]] bool parsePositiveUint16(std::string_view value, uint16_t &parsed) {
		const auto [ptr, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
		return error == std::errc {} && ptr == value.data() + value.size() && parsed != 0;
	}
}

PlayerDisciplines::PlayerDisciplines(Player &player) :
	player(player) { }

const std::map<uint16_t, uint32_t> &PlayerDisciplines::ranks() const {
	std::scoped_lock lock(mutex);
	loadLocked();
	return storedRanks;
}

DisciplineMutation PlayerDisciplines::addRank(uint16_t id) {
	std::scoped_lock lock(mutex);
	loadLocked();
	if (!g_disciplines().get(id)) {
		return { .result = DisciplineMutationResult::UnknownDiscipline };
	}

	const auto before = storedRanks.contains(id) ? storedRanks.at(id) : 0;
	if (before == std::numeric_limits<IntType>::max()) {
		return { .result = DisciplineMutationResult::RankLimit, .before = before, .after = before };
	}

	auto nextRanks = storedRanks;
	nextRanks[id] = before + 1;
	persistLocked(nextRanks);
	storedRanks = std::move(nextRanks);
	return { .result = DisciplineMutationResult::Success, .before = before, .after = before + 1 };
}

DisciplineMutation PlayerDisciplines::removeRank(uint16_t id) {
	std::scoped_lock lock(mutex);
	loadLocked();
	if (!g_disciplines().get(id)) {
		return { .result = DisciplineMutationResult::UnknownDiscipline };
	}
	const auto it = storedRanks.find(id);
	if (it == storedRanks.end()) {
		return { .result = DisciplineMutationResult::NotOwned };
	}

	const auto before = it->second;
	auto nextRanks = storedRanks;
	if (before == 1) {
		nextRanks.erase(id);
	} else {
		nextRanks[id] = before - 1;
	}
	persistLocked(nextRanks);
	storedRanks = std::move(nextRanks);
	return { .result = DisciplineMutationResult::Success, .before = before, .after = before - 1 };
}

void PlayerDisciplines::loadLocked() const {
	if (loaded) {
		return;
	}
	loaded = true;

	const auto persisted = player.kv()->scoped("disciplines")->get(std::string(ranksKey));
	if (!persisted) {
		return;
	}
	const auto values = persisted->get<MapType>();
	for (const auto &[key, value] : values) {
		uint16_t id = 0;
		if (!parsePositiveUint16(key, id)) {
			g_logger().error("[PlayerDisciplines] player={} key={} reason=invalid discipline id", player.getName(), key);
			continue;
		}
		const auto* rank = value ? std::get_if<IntType>(&value->getVariant()) : nullptr;
		if (!rank || *rank <= 0) {
			g_logger().error("[PlayerDisciplines] player={} key={} reason=invalid rank", player.getName(), key);
			continue;
		}
		storedRanks.emplace(id, static_cast<uint32_t>(*rank));
	}
}

void PlayerDisciplines::persistLocked(const std::map<uint16_t, uint32_t> &nextRanks) const {
	MapType serialized;
	for (const auto &[id, rank] : nextRanks) {
		serialized.emplace(std::to_string(id), std::make_shared<ValueWrapper>(static_cast<IntType>(rank)));
	}
	player.kv()->scoped("disciplines")->set(std::string(ranksKey), ValueWrapper(serialized));
}
