/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#pragma once

enum class CharacterAttribute : uint8_t {
	Strength,
	Dexterity,
	Vitality,
	Intelligence,
	Willpower,
	Last,
};

using AttributeContributions = std::array<uint32_t, static_cast<size_t>(CharacterAttribute::Last)>;
using AttributeTotals = std::array<uint64_t, static_cast<size_t>(CharacterAttribute::Last)>;

struct Discipline {
	uint16_t id = 0;
	std::string name;
	AttributeContributions perLevel {};
};

class DisciplineCatalog {
public:
	DisciplineCatalog() = default;

	DisciplineCatalog(const DisciplineCatalog &) = delete;
	void operator=(const DisciplineCatalog &) = delete;

	static DisciplineCatalog &getInstance();

	bool loadFromXml();
	bool loadFromXml(const std::filesystem::path &path);

	[[nodiscard]] const Discipline* get(uint16_t id) const;
	[[nodiscard]] const std::map<uint16_t, Discipline> &all() const;

private:
	std::map<uint16_t, Discipline> disciplines;
};

constexpr auto g_disciplines = DisciplineCatalog::getInstance;
