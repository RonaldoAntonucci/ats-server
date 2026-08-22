/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/offensive_equipment_resolver.hpp"

#include "creatures/players/player.hpp"
#include "items/item.hpp"

#ifndef USE_PRECOMPILED_HEADERS
	#include <algorithm>
	#include <limits>
#endif

namespace {
	std::optional<OffensiveProfile> classifyOffensiveWeapon(const std::shared_ptr<Item> &item) {
		if (!item) {
			return std::nullopt;
		}
		switch (item->getWeaponType()) {
			case WEAPON_SWORD:
				return OffensiveProfile::Sword;
			case WEAPON_AXE:
				return OffensiveProfile::Axe;
			case WEAPON_CLUB:
				return OffensiveProfile::Club;
			case WEAPON_DISTANCE:
				switch (item->getAmmoType()) {
					case AMMO_ARROW:
						return OffensiveProfile::Bow;
					case AMMO_BOLT:
						return OffensiveProfile::Crossbow;
					default:
						return std::nullopt;
				}
			default:
				return std::nullopt;
		}
	}

	bool isShield(const std::shared_ptr<Item> &item) {
		return item && item->getWeaponType() == WEAPON_SHIELD && Item::items[item->getID()].isShield();
	}

	int32_t boundedPowerSum(int32_t first, int32_t second) {
		const auto sum = static_cast<int64_t>(first) + static_cast<int64_t>(second);
		return static_cast<int32_t>(std::clamp<int64_t>(sum, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()));
	}

	EquipmentResolution resolveWeapon(const std::shared_ptr<Item> &weapon, OffensiveProfile profile, std::shared_ptr<Item> ammunition) {
		std::shared_ptr<Cylinder> ammunitionParent;
		int32_t equipmentPower = weapon->getAttack();
		const bool requiresAmmunition = profile == OffensiveProfile::Bow || profile == OffensiveProfile::Crossbow;
		if (requiresAmmunition) {
			if (!ammunition) {
				return { std::nullopt, EquipmentResolutionResult::MissingAmmunition };
			}
			ammunitionParent = ammunition->getParent();
			equipmentPower = boundedPowerSum(equipmentPower, ammunition->getAttack());
		}

		return {
			OffensiveEquipmentSnapshot {
				.profile = profile,
				.equipmentPower = equipmentPower,
				.range = weapon->getShootRange(),
				.requiresAmmunition = requiresAmmunition,
				.weapon = weapon,
				.ammunition = std::move(ammunition),
				.ammunitionParent = std::move(ammunitionParent),
			},
			EquipmentResolutionResult::Resolved,
		};
	}

	EquipmentResolution resolveShield(const std::shared_ptr<Item> &shield) {
		return {
			OffensiveEquipmentSnapshot {
				.profile = OffensiveProfile::Shield,
				.equipmentPower = shield->getDefense(),
				.range = shield->getShootRange(),
				.requiresAmmunition = false,
				.weapon = shield,
				.ammunition = nullptr,
				.ammunitionParent = nullptr,
			},
			EquipmentResolutionResult::Resolved,
		};
	}
}

std::string_view equipmentResolutionReason(EquipmentResolutionResult result) {
	switch (result) {
		case EquipmentResolutionResult::Resolved:
			return "resolved";
		case EquipmentResolutionResult::UnsupportedEquipment:
			return "unsupported_equipment";
		case EquipmentResolutionResult::MissingAmmunition:
			return "missing_ammunition";
	}
	return "unsupported_equipment";
}

bool EquipmentResolution::success() const {
	return result == EquipmentResolutionResult::Resolved && snapshot.has_value();
}

EquipmentResolution OffensiveEquipmentResolver::resolve(const Player &player) {
	const auto left = player.getInventoryItem(CONST_SLOT_LEFT);
	const auto right = player.getInventoryItem(CONST_SLOT_RIGHT);
	const auto resolveSelectedWeapon = [&player](const std::shared_ptr<Item> &weapon, OffensiveProfile profile) {
		std::shared_ptr<Item> ammunition;
		if (profile == OffensiveProfile::Bow || profile == OffensiveProfile::Crossbow) {
			ammunition = player.getQuiverAmmoOfType(Item::items[weapon->getID()]);
		}
		return resolveWeapon(weapon, profile, std::move(ammunition));
	};

	if (const auto profile = classifyOffensiveWeapon(left)) {
		return resolveSelectedWeapon(left, *profile);
	}
	if (const auto profile = classifyOffensiveWeapon(right)) {
		return resolveSelectedWeapon(right, *profile);
	}
	if (isShield(left)) {
		return resolveShield(left);
	}
	if (isShield(right)) {
		return resolveShield(right);
	}
	return { std::nullopt, EquipmentResolutionResult::UnsupportedEquipment };
}
