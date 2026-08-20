/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once

#ifndef USE_PRECOMPILED_HEADERS
	#include <cstdint>
	#include <string_view>
#endif

class Player;

struct CharacterAttributeValues {
	uint8_t strength = 0;
	uint8_t dexterity = 0;
	uint8_t vitality = 0;
	uint8_t intelligence = 0;
	uint8_t willpower = 0;

	bool operator==(const CharacterAttributeValues &) const = default;
};

struct CharacterAttributesSnapshot {
	CharacterAttributeValues values {};
	uint16_t freePoints = 100;
	bool valid = true;
};

enum class CharacterAttributeAddResult : uint8_t {
	Success,
	UnknownAttribute,
	InvalidAmount,
	InsufficientPoints,
	InvalidState,
};

class CharacterAttributes {
public:
	explicit CharacterAttributes(Player &player);

	[[nodiscard]] CharacterAttributesSnapshot snapshot() const;
	[[nodiscard]] CharacterAttributeAddResult add(std::string_view key, uint16_t amount) const;
	void reset() const;

private:
	void store(const CharacterAttributeValues &values) const;

	Player &m_player;
};
