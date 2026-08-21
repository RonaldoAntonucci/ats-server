/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/disciplines/discipline.hpp"

#include "config/configmanager.hpp"
#include "lib/di/container.hpp"
#include "utils/tools.hpp"

namespace {
	[[nodiscard]] std::string trimCopy(std::string value) {
		const auto first = value.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) {
			return {};
		}

		const auto last = value.find_last_not_of(" \t\r\n");
		return value.substr(first, last - first + 1);
	}

	template <typename T>
	[[nodiscard]] bool parseUnsigned(std::string_view value, T &parsed) {
		if (value.empty() || value.front() == '+' || value.front() == '-') {
			return false;
		}

		const auto [ptr, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
		return error == std::errc {} && ptr == value.data() + value.size();
	}

	void logCatalogError(const std::filesystem::path &path, std::string_view discipline, std::string_view field, std::string_view reason) {
		g_logger().error(
			"[DisciplineCatalog] path={} discipline={} field={} reason={}",
			path.string(),
			discipline,
			field,
			reason
		);
	}

	[[nodiscard]] bool hasOnlyAttributes(const pugi::xml_node &node, std::initializer_list<std::string_view> expected, const std::filesystem::path &path, std::string_view discipline) {
		for (const auto &attribute : node.attributes()) {
			const auto name = std::string_view(attribute.name());
			if (std::ranges::find(expected, name) == expected.end()) {
				logCatalogError(path, discipline, name, "unknown attribute");
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] std::optional<CharacterAttribute> toCharacterAttribute(std::string_view id) {
		for (size_t index = 0; index < static_cast<size_t>(CharacterAttribute::Last); ++index) {
			const auto attribute = static_cast<CharacterAttribute>(index);
			if (characterAttributeId(attribute) == id) {
				return attribute;
			}
		}
		return std::nullopt;
	}
} // namespace

DisciplineCatalog &DisciplineCatalog::getInstance() {
	return inject<DisciplineCatalog>();
}

bool DisciplineCatalog::loadFromXml() {
	return loadFromXml(std::filesystem::path(g_configManager().getString(CORE_DIRECTORY)) / "XML/disciplines.xml");
}

bool DisciplineCatalog::loadFromXml(const std::filesystem::path &path) {
	pugi::xml_document document;
	const auto pathString = path.string();
	const auto result = document.load_file(pathString.c_str());
	if (!result) {
		logCatalogError(path, "-", "document", result.description());
		return false;
	}

	const auto root = document.document_element();
	if (!root || std::string_view(root.name()) != "disciplines") {
		logCatalogError(path, "-", "root", "expected disciplines");
		return false;
	}
	if (!hasOnlyAttributes(root, {}, path, "-")) {
		return false;
	}

	std::map<uint16_t, Discipline> loaded;
	for (const auto &disciplineNode : root.children()) {
		if (disciplineNode.type() != pugi::node_element) {
			continue;
		}
		if (std::string_view(disciplineNode.name()) != "discipline") {
			logCatalogError(path, "-", disciplineNode.name(), "unknown node");
			return false;
		}
		if (!hasOnlyAttributes(disciplineNode, { "id", "name" }, path, "-")) {
			return false;
		}

		const auto idAttribute = disciplineNode.attribute("id");
		uint16_t id = 0;
		if (!idAttribute || !parseUnsigned(idAttribute.value(), id) || id == 0) {
			logCatalogError(path, "-", "id", "must be a positive uint16");
			return false;
		}

		const auto nameAttribute = disciplineNode.attribute("name");
		const auto name = nameAttribute ? trimCopy(nameAttribute.value()) : std::string {};
		if (name.empty()) {
			logCatalogError(path, std::to_string(id), "name", "must not be empty");
			return false;
		}

		Discipline discipline { .id = id, .name = name };
		std::array<bool, static_cast<size_t>(CharacterAttribute::Last)> seenAttributes {};
		for (const auto &attributeNode : disciplineNode.children()) {
			if (attributeNode.type() != pugi::node_element) {
				continue;
			}
			if (std::string_view(attributeNode.name()) != "attribute") {
				logCatalogError(path, std::to_string(id), attributeNode.name(), "unknown node");
				return false;
			}
			if (!hasOnlyAttributes(attributeNode, { "id", "perLevel" }, path, std::to_string(id))) {
				return false;
			}

			const auto attributeId = attributeNode.attribute("id");
			const auto attribute = attributeId ? toCharacterAttribute(attributeId.value()) : std::nullopt;
			if (!attribute) {
				const auto invalidId = attributeId ? std::string(attributeId.value()) : std::string("<missing>");
				logCatalogError(path, std::to_string(id), "attribute.id", "unknown attribute id " + invalidId);
				return false;
			}

			const auto perLevel = attributeNode.attribute("perLevel");
			uint32_t contribution = 0;
			if (!perLevel || !parseUnsigned(perLevel.value(), contribution)) {
				logCatalogError(path, std::to_string(id), "attribute.perLevel", "must be a non-negative uint32");
				return false;
			}

			const auto attributeIndex = static_cast<size_t>(*attribute);
			if (seenAttributes[attributeIndex]) {
				logCatalogError(path, std::to_string(id), "attribute.id", "duplicate attribute");
				return false;
			}
			seenAttributes[attributeIndex] = true;
			discipline.perLevel[attributeIndex] = contribution;
		}

		if (!loaded.emplace(id, std::move(discipline)).second) {
			logCatalogError(path, std::to_string(id), "id", "duplicate discipline id");
			return false;
		}
	}

	disciplines = std::move(loaded);
	return true;
}

const Discipline* DisciplineCatalog::get(uint16_t id) const {
	const auto it = disciplines.find(id);
	return it == disciplines.end() ? nullptr : &it->second;
}

const std::map<uint16_t, Discipline> &DisciplineCatalog::all() const {
	return disciplines;
}
