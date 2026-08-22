/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019-2024 OpenTibiaBR
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include <gtest/gtest.h>

#define private public
#include "creatures/players/grouping/party.hpp"
#undef private

#include "creatures/players/player.hpp"
#include "lib/di/container.hpp"
#include "lib/logging/in_memory_logger.hpp"

class PartyTest : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		previousTestContainer_ = DI::getTestContainer();
		injector_ = std::make_unique<di::extension::injector<>>();
		InMemoryLogger::install(*injector_);
		DI::setTestContainer(injector_.get());
	}

	static void TearDownTestSuite() {
		DI::setTestContainer(previousTestContainer_);
		injector_.reset();
	}

private:
	inline static std::unique_ptr<di::extension::injector<>> injector_;
	inline static di::extension::injector<>* previousTestContainer_ = nullptr;
};

TEST_F(PartyTest, GetPlayersAndDisbandHandleNullEntries) {
	auto party = std::make_shared<Party>();
	auto leader = std::make_shared<Player>();
	auto member = std::make_shared<Player>();
	auto invitee = std::make_shared<Player>();

	party->m_leader = leader;
	leader->setParty(party);
	member->setParty(party);

	party->memberList.push_back(nullptr);
	party->memberList.push_back(member);
	party->inviteList.push_back(nullptr);
	party->inviteList.push_back(invitee);

	const auto players = party->getPlayers();
	ASSERT_EQ(players.size(), 2U);
	EXPECT_EQ(players[0], member);
	EXPECT_EQ(players[1], leader);

	EXPECT_NO_THROW(party->disband());
	EXPECT_EQ(party->getLeader(), nullptr);
	EXPECT_EQ(party->getMemberCount(), 0U);
	EXPECT_EQ(party->getInvitationCount(), 0U);
	EXPECT_EQ(leader->getParty(), nullptr);
	EXPECT_EQ(member->getParty(), nullptr);
}
