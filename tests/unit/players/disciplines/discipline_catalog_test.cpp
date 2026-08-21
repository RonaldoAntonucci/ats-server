/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/players/disciplines/discipline.hpp"

#include <gtest/gtest.h>

#include "lib/di/container.hpp"
#include "lib/logging/in_memory_logger.hpp"

namespace {
	class DisciplineCatalogTest : public ::testing::Test {
	protected:
		static void SetUpTestSuite() {
			previousTestContainer = DI::getTestContainer();
			InMemoryLogger::install(injector);
			DI::setTestContainer(&injector);
		}

		static void TearDownTestSuite() {
			DI::setTestContainer(previousTestContainer);
		}

		void SetUp() override {
			dynamic_cast<InMemoryLogger &>(DI::get<Logger>()).reset();
			temporaryDirectory = std::filesystem::temp_directory_path() / ("canary-discipline-catalog-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
			std::filesystem::create_directories(temporaryDirectory);
			file = temporaryDirectory / "disciplines.xml";
		}

		void TearDown() override {
			std::error_code error;
			std::filesystem::remove_all(temporaryDirectory, error);
		}

		void write(std::string_view content) const {
			std::ofstream output(file);
			ASSERT_TRUE(output.is_open());
			output << content;
		}

		DisciplineCatalog catalog;
		std::filesystem::path temporaryDirectory;
		std::filesystem::path file;

		inline static di::extension::injector<> injector {};
		inline static di::extension::injector<>* previousTestContainer = nullptr;
	};

	constexpr std::string_view validCatalog = R"xml(
<disciplines>
	<discipline id="1" name="Armamento">
		<attribute id="for" perLevel="1" />
		<attribute id="des" perLevel="1" />
		<attribute id="vit" perLevel="1" />
	</discipline>
</disciplines>
)xml";
} // namespace

TEST_F(DisciplineCatalogTest, LoadsArmamentoWithConfiguredContributions) {
	write(validCatalog);

	ASSERT_TRUE(catalog.loadFromXml(file));
	const auto* armamento = catalog.get(1);
	ASSERT_NE(nullptr, armamento);
	EXPECT_EQ("Armamento", armamento->name);
	EXPECT_EQ(1u, armamento->perLevel[static_cast<size_t>(CharacterAttribute::Potency)]);
	EXPECT_EQ(1u, armamento->perLevel[static_cast<size_t>(CharacterAttribute::Technique)]);
	EXPECT_EQ(1u, armamento->perLevel[static_cast<size_t>(CharacterAttribute::Vigor)]);
}

TEST_F(DisciplineCatalogTest, DefaultsOmittedAttributesToZero) {
	write(validCatalog);

	ASSERT_TRUE(catalog.loadFromXml(file));
	const auto* armamento = catalog.get(1);
	ASSERT_NE(nullptr, armamento);
	EXPECT_EQ(0u, armamento->perLevel[static_cast<size_t>(CharacterAttribute::Attunement)]);
	EXPECT_EQ(0u, armamento->perLevel[static_cast<size_t>(CharacterAttribute::Spirit)]);
}

TEST_F(DisciplineCatalogTest, PreservesAscendingNumericIdOrder) {
	write(R"xml(<disciplines><discipline id="2" name="Second"/><discipline id="1" name="First"/></disciplines>)xml");

	ASSERT_TRUE(catalog.loadFromXml(file));
	auto it = catalog.all().begin();
	ASSERT_NE(catalog.all().end(), it);
	EXPECT_EQ(1u, it->first);
	EXPECT_EQ("First", it->second.name);
	EXPECT_EQ(2u, std::next(it)->first);
}

TEST_F(DisciplineCatalogTest, RejectsMissingFile) {
	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsMalformedXml) {
	write("<disciplines><discipline id=\"1\" name=\"Armamento\">");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsInvalidRoot) {
	write("<not-disciplines/>");

	EXPECT_FALSE(catalog.loadFromXml(file));
	const auto &logger = dynamic_cast<InMemoryLogger &>(DI::get<Logger>());
	ASSERT_EQ(1u, logger.logCount());
	const auto [level, message] = logger.getLogEntry(0);
	EXPECT_EQ("error", level);
	EXPECT_NE(std::string::npos, message.find("path=" + file.string()));
	EXPECT_NE(std::string::npos, message.find("discipline=-"));
	EXPECT_NE(std::string::npos, message.find("field=root"));
	EXPECT_NE(std::string::npos, message.find("reason=expected disciplines"));
}

TEST_F(DisciplineCatalogTest, RejectsUnknownRootAttribute) {
	write("<disciplines unexpected=\"true\"/>");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsUnknownRootNode) {
	write("<disciplines><unknown/></disciplines>");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsMissingDisciplineId) {
	write("<disciplines><discipline name=\"Armamento\"/></disciplines>");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsZeroOrNonNumericDisciplineId) {
	write("<disciplines><discipline id=\"0\" name=\"Zero\"/></disciplines>");
	EXPECT_FALSE(catalog.loadFromXml(file));

	write("<disciplines><discipline id=\"one\" name=\"Text\"/></disciplines>");
	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsDuplicateDisciplineId) {
	write("<disciplines><discipline id=\"1\" name=\"First\"/><discipline id=\"1\" name=\"Second\"/></disciplines>");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsEmptyDisciplineName) {
	write("<disciplines><discipline id=\"1\" name=\"  \t\"/></disciplines>");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsUnknownDisciplineAttribute) {
	write("<disciplines><discipline id=\"1\" name=\"Armamento\" extra=\"x\"/></disciplines>");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsUnknownAttributeNode) {
	write("<disciplines><discipline id=\"1\" name=\"Armamento\"><bonus/></discipline></disciplines>");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsUnknownAttributeId) {
	write("<disciplines><discipline id=\"1\" name=\"Armamento\"><attribute id=\"luck\" perLevel=\"1\"/></discipline></disciplines>");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsDuplicateAttribute) {
	write("<disciplines><discipline id=\"1\" name=\"Armamento\"><attribute id=\"for\" perLevel=\"1\"/><attribute id=\"for\" perLevel=\"2\"/></discipline></disciplines>");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsDuplicateAttributeWhenTheFirstContributionIsZero) {
	write("<disciplines><discipline id=\"1\" name=\"Armamento\"><attribute id=\"for\" perLevel=\"0\"/><attribute id=\"for\" perLevel=\"1\"/></discipline></disciplines>");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsUnknownAttributeProperty) {
	write("<disciplines><discipline id=\"1\" name=\"Armamento\"><attribute id=\"for\" perLevel=\"1\" extra=\"x\"/></discipline></disciplines>");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsInvalidAttributeContribution) {
	write("<disciplines><discipline id=\"1\" name=\"Armamento\"><attribute id=\"for\" perLevel=\"-1\"/></discipline></disciplines>");
	EXPECT_FALSE(catalog.loadFromXml(file));

	write("<disciplines><discipline id=\"1\" name=\"Armamento\"><attribute id=\"for\" perLevel=\"invalid\"/></discipline></disciplines>");
	EXPECT_FALSE(catalog.loadFromXml(file));

	write("<disciplines><discipline id=\"1\" name=\"Armamento\"><attribute id=\"for\" perLevel=\"4294967296\"/></discipline></disciplines>");
	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, RejectsMissingAttributeContribution) {
	write("<disciplines><discipline id=\"1\" name=\"Armamento\"><attribute id=\"for\"/></discipline></disciplines>");

	EXPECT_FALSE(catalog.loadFromXml(file));
}

TEST_F(DisciplineCatalogTest, PublishesOnlyAfterFullValidation) {
	write(validCatalog);
	ASSERT_TRUE(catalog.loadFromXml(file));

	write("<disciplines><discipline id=\"2\" name=\"Valid\"/><discipline id=\"3\" name=\"Broken\"><attribute id=\"for\" perLevel=\"invalid\"/></discipline></disciplines>");
	EXPECT_FALSE(catalog.loadFromXml(file));

	ASSERT_NE(nullptr, catalog.get(1));
	EXPECT_EQ(nullptr, catalog.get(2));
	EXPECT_EQ(1u, catalog.all().size());
}
