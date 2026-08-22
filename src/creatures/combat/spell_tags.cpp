/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/spell_tags.hpp"

#include "lib/logging/log_with_spd_log.hpp"

namespace {
	constexpr std::array spellTagCatalog = {
		std::pair { SpellTag::CategoryArt, std::string_view { "category.art" } },
		std::pair { SpellTag::DamageNeutral, std::string_view { "damage.neutral" } },
		std::pair { SpellTag::DamagePhysical, std::string_view { "damage.physical" } },
		std::pair { SpellTag::DisciplineArmament, std::string_view { "discipline.armament" } },
		std::pair { SpellTag::EquipmentShield, std::string_view { "equipment.shield" } },
		std::pair { SpellTag::ExecutionArea, std::string_view { "execution.area" } },
		std::pair { SpellTag::ExecutionAttack, std::string_view { "execution.attack" } },
		std::pair { SpellTag::ExecutionContact, std::string_view { "execution.contact" } },
		std::pair { SpellTag::ExecutionProjectile, std::string_view { "execution.projectile" } },
		std::pair { SpellTag::FunctionControl, std::string_view { "function.control" } },
		std::pair { SpellTag::FunctionOffensive, std::string_view { "function.offensive" } },
		std::pair { SpellTag::MechanicKnockback, std::string_view { "mechanic.knockback" } },
		std::pair { SpellTag::WeaponAxe, std::string_view { "weapon.axe" } },
		std::pair { SpellTag::WeaponBow, std::string_view { "weapon.bow" } },
		std::pair { SpellTag::WeaponClub, std::string_view { "weapon.club" } },
		std::pair { SpellTag::WeaponCrossbow, std::string_view { "weapon.crossbow" } },
		std::pair { SpellTag::WeaponSword, std::string_view { "weapon.sword" } },
	};
	static_assert(spellTagCatalog.size() == spellTagCount);

	SpellTagValidationResult validateSpellTagName(std::string_view name) {
		if (name.empty()) {
			return SpellTagValidationResult::EmptyTag;
		}

		const auto separator = name.find('.');
		if (separator == std::string_view::npos || separator == 0 || separator == name.size() - 1) {
			return SpellTagValidationResult::MissingNamespace;
		}

		return parseSpellTag(name).has_value() ? SpellTagValidationResult::Allowed : SpellTagValidationResult::UnsupportedTag;
	}
}

std::optional<SpellTag> parseSpellTag(std::string_view name) {
	const auto entry = std::ranges::find_if(spellTagCatalog, [name](const auto &candidate) {
		return candidate.second == name;
	});
	if (entry == spellTagCatalog.end()) {
		return std::nullopt;
	}
	return entry->first;
}

std::string_view canonicalSpellTagName(SpellTag tag) {
	const auto entry = std::ranges::find_if(spellTagCatalog, [tag](const auto &candidate) {
		return candidate.first == tag;
	});
	return entry != spellTagCatalog.end() ? entry->second : std::string_view {};
}

std::string_view spellTagValidationReason(SpellTagValidationResult result) {
	switch (result) {
		case SpellTagValidationResult::Allowed:
			return "allowed";
		case SpellTagValidationResult::EmptyTag:
			return "empty_tag";
		case SpellTagValidationResult::MissingNamespace:
			return "missing_namespace";
		case SpellTagValidationResult::UnsupportedTag:
			return "unsupported_tag";
	}
	return "unsupported_tag";
}

SpellTagSet::SpellTagSet(std::vector<SpellTag> tags) :
	normalizedTags(std::move(tags)) { }

const std::vector<SpellTag> &SpellTagSet::values() const {
	return normalizedTags;
}

std::vector<std::string_view> SpellTagSet::names() const {
	std::vector<std::string_view> result;
	result.reserve(normalizedTags.size());
	for (const auto tag : normalizedTags) {
		result.emplace_back(canonicalSpellTagName(tag));
	}
	return result;
}

bool SpellTagSet::contains(SpellTag tag) const {
	return std::ranges::find(normalizedTags, tag) != normalizedTags.end();
}

bool SpellTagSet::empty() const {
	return normalizedTags.empty();
}

SpellTagSet normalizeSpellTags(std::vector<SpellTag> tags) {
	std::ranges::sort(tags, {}, canonicalSpellTagName);
	const auto uniqueEnd = std::ranges::unique(tags).begin();
	tags.erase(uniqueEnd, tags.end());
	return SpellTagSet(std::move(tags));
}

bool SpellTagSetBuildResult::success() const {
	return result == SpellTagValidationResult::Allowed && tags.has_value();
}

SpellTagSetBuildResult buildSpellTagSet(std::span<const std::string_view> names) {
	std::vector<SpellTag> parsedTags;
	parsedTags.reserve(names.size());

	for (const auto name : names) {
		const auto validation = validateSpellTagName(name);
		if (validation != SpellTagValidationResult::Allowed) {
			g_logger().error("[SpellTagSet::build] tag={} reason={}", name, spellTagValidationReason(validation));
			return { std::nullopt, validation, std::string(name) };
		}
		parsedTags.emplace_back(*parseSpellTag(name));
	}

	return { normalizeSpellTags(std::move(parsedTags)), SpellTagValidationResult::Allowed, {} };
}
