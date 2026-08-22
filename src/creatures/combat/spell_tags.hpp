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
	#include <cstddef>
	#include <cstdint>
	#include <optional>
	#include <span>
	#include <string>
	#include <string_view>
	#include <vector>
#endif

enum class SpellTag : uint8_t {
	CategoryArt,
	DamageNeutral,
	DamagePhysical,
	DisciplineArmament,
	EquipmentShield,
	ExecutionArea,
	ExecutionAttack,
	ExecutionContact,
	ExecutionProjectile,
	FunctionControl,
	FunctionOffensive,
	MechanicKnockback,
	WeaponAxe,
	WeaponBow,
	WeaponClub,
	WeaponCrossbow,
	WeaponSword,
};

inline constexpr size_t spellTagCount = static_cast<size_t>(SpellTag::WeaponSword) + 1;

enum class SpellTagValidationResult : uint8_t {
	Allowed,
	EmptyTag,
	MissingNamespace,
	UnsupportedTag,
};

[[nodiscard]] std::optional<SpellTag> parseSpellTag(std::string_view name);
[[nodiscard]] std::string_view canonicalSpellTagName(SpellTag tag);
[[nodiscard]] std::string_view spellTagValidationReason(SpellTagValidationResult result);

class SpellTagSet {
public:
	SpellTagSet() = default;

	[[nodiscard]] const std::vector<SpellTag> &values() const;
	[[nodiscard]] std::vector<std::string_view> names() const;
	[[nodiscard]] bool contains(SpellTag tag) const;
	[[nodiscard]] bool empty() const;

private:
	explicit SpellTagSet(std::vector<SpellTag> tags);
	friend SpellTagSet normalizeSpellTags(std::vector<SpellTag> tags);

	std::vector<SpellTag> normalizedTags;
};

[[nodiscard]] SpellTagSet normalizeSpellTags(std::vector<SpellTag> tags);

struct SpellTagSetBuildResult {
	std::optional<SpellTagSet> tags;
	SpellTagValidationResult result = SpellTagValidationResult::Allowed;
	std::string rejectedTag;

	[[nodiscard]] bool success() const;
};

[[nodiscard]] SpellTagSetBuildResult buildSpellTagSet(std::span<const std::string_view> names);
