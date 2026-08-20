/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/components/character_attributes.hpp"

#include "creatures/players/player.hpp"
#include "kv/kv.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <algorithm>
	#include <array>
	#include <variant>
#endif

namespace {
	constexpr uint16_t kAttributeBudget = 100;
	constexpr std::string_view kAllocationScope = "character-attributes";
	constexpr std::string_view kAllocationKey = "allocation";

	using AttributeMember = uint8_t CharacterAttributeValues::*;

	constexpr std::array<std::pair<std::string_view, AttributeMember>, 5> kAttributeAliases { {
		{ "str", &CharacterAttributeValues::strength },
		{ "dex", &CharacterAttributeValues::dexterity },
		{ "vit", &CharacterAttributeValues::vitality },
		{ "int", &CharacterAttributeValues::intelligence },
		{ "wil", &CharacterAttributeValues::willpower },
	} };

	constexpr std::array<std::pair<std::string_view, AttributeMember>, 5> kStoredAttributes { {
		{ "strength", &CharacterAttributeValues::strength },
		{ "dexterity", &CharacterAttributeValues::dexterity },
		{ "vitality", &CharacterAttributeValues::vitality },
		{ "intelligence", &CharacterAttributeValues::intelligence },
		{ "willpower", &CharacterAttributeValues::willpower },
	} };

	CharacterAttributesSnapshot invalidSnapshot() {
		return { .values = {}, .freePoints = 0, .valid = false };
	}
} // namespace

CharacterAttributes::CharacterAttributes(Player &player) :
	m_player(player) { }

CharacterAttributesSnapshot CharacterAttributes::snapshot() const {
	const auto allocation = m_player.kv()->scoped(std::string(kAllocationScope))->get(std::string(kAllocationKey));
	if (!allocation.has_value()) {
		return {};
	}

	if (!std::holds_alternative<MapType>(allocation->getVariant())) {
		return invalidSnapshot();
	}

	const auto map = allocation->get<MapType>();
	CharacterAttributeValues values;
	uint16_t spentPoints = 0;

	for (const auto &[name, member] : kStoredAttributes) {
		const auto it = map.find(std::string(name));
		if (it == map.end() || !it->second || !std::holds_alternative<IntType>(it->second->getVariant())) {
			return invalidSnapshot();
		}

		const auto value = it->second->get<IntType>();
		if (value < 0 || value > kAttributeBudget) {
			return invalidSnapshot();
		}

		values.*member = static_cast<uint8_t>(value);
		spentPoints += static_cast<uint16_t>(value);
	}

	if (spentPoints > kAttributeBudget) {
		return invalidSnapshot();
	}

	return {
		.values = values,
		.freePoints = static_cast<uint16_t>(kAttributeBudget - spentPoints),
		.valid = true,
	};
}

CharacterAttributeAddResult CharacterAttributes::add(std::string_view key, uint16_t amount) const {
	const auto attribute = std::ranges::find_if(kAttributeAliases, [key](const auto &entry) {
		return entry.first == key;
	});
	if (attribute == kAttributeAliases.end()) {
		return CharacterAttributeAddResult::UnknownAttribute;
	}

	if (amount == 0 || amount > kAttributeBudget) {
		return CharacterAttributeAddResult::InvalidAmount;
	}

	auto current = snapshot();
	if (!current.valid) {
		return CharacterAttributeAddResult::InvalidState;
	}

	if (amount > current.freePoints) {
		return CharacterAttributeAddResult::InsufficientPoints;
	}

	current.values.*attribute->second = static_cast<uint8_t>(current.values.*attribute->second + amount);
	store(current.values);
	return CharacterAttributeAddResult::Success;
}

void CharacterAttributes::reset() const {
	store({});
}

void CharacterAttributes::store(const CharacterAttributeValues &values) const {
	ValueWrapper allocation {
		{ "strength", static_cast<int>(values.strength) },
		{ "dexterity", static_cast<int>(values.dexterity) },
		{ "vitality", static_cast<int>(values.vitality) },
		{ "intelligence", static_cast<int>(values.intelligence) },
		{ "willpower", static_cast<int>(values.willpower) },
	};
	m_player.kv()->scoped(std::string(kAllocationScope))->set(std::string(kAllocationKey), allocation);
}
