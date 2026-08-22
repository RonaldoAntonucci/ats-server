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
	#include <memory>
	#include <optional>
	#include <string_view>
#endif

class Cylinder;
class Item;
class Player;

enum class OffensiveProfile : uint8_t {
	Sword,
	Axe,
	Club,
	Bow,
	Crossbow,
	Shield,
};

struct OffensiveEquipmentSnapshot {
	OffensiveProfile profile;
	int32_t equipmentPower;
	uint8_t range;
	bool requiresAmmunition;
	std::shared_ptr<Item> weapon;
	std::shared_ptr<Item> ammunition;
	std::shared_ptr<Cylinder> ammunitionParent;
};

enum class EquipmentResolutionResult : uint8_t {
	Resolved,
	UnsupportedEquipment,
	MissingAmmunition,
};

[[nodiscard]] std::string_view equipmentResolutionReason(EquipmentResolutionResult result);

struct EquipmentResolution {
	std::optional<OffensiveEquipmentSnapshot> snapshot;
	EquipmentResolutionResult result = EquipmentResolutionResult::UnsupportedEquipment;

	[[nodiscard]] bool success() const;
};

class OffensiveEquipmentResolver {
public:
	[[nodiscard]] static EquipmentResolution resolve(const Player &player);
};
