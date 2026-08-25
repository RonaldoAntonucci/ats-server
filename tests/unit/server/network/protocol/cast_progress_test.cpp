/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/prepared_cast.hpp"
#include "server/network/message/networkmessage.hpp"
#include "server/network/protocol/protocolgame.hpp"

#include <gtest/gtest.h>

namespace {
	PreparedCastSnapshot makeSnapshot(uint64_t id = 0x0102030405060708, uint32_t durationMs = 700, uint32_t remainingMs = 350) {
		return { id, durationMs, remainingMs };
	}

	void rewind(NetworkMessage &msg) {
		msg.setBufferPosition(NetworkMessage::INITIAL_BUFFER_POSITION);
	}
}

TEST(ProtocolGameCastProgressTest, ReservesTheApprovedNumericContract) {
	EXPECT_EQ(136u, CastProgressProtocol::Feature);
	EXPECT_EQ(15u, CastProgressProtocol::CreatureDataSubtype);
	EXPECT_EQ(1u, static_cast<uint8_t>(CastProgressProtocol::Action::Start));
	EXPECT_EQ(2u, static_cast<uint8_t>(CastProgressProtocol::Action::Cancel));
}

TEST(ProtocolGameCastProgressTest, FindsTheFeatureOnlyInTheEnabledCatalog) {
	EXPECT_FALSE(CastProgressProtocol::isEnabled({ 101, 118 }));
	EXPECT_TRUE(CastProgressProtocol::isEnabled({ 101, 136, 118 }));
}

TEST(ProtocolGameCastProgressTest, FeatureOffLeavesStartStreamUnchanged) {
	NetworkMessage msg;
	msg.addByte(0xAA);
	CastProgressProtocol::addStart(msg, 0x11223344, makeSnapshot(), false);

	EXPECT_EQ(1u, msg.getLength());
	rewind(msg);
	EXPECT_EQ(0xAA, msg.getByte());
}

TEST(ProtocolGameCastProgressTest, WritesTheExactStartPayload) {
	NetworkMessage msg;
	CastProgressProtocol::addStart(msg, 0x11223344, makeSnapshot(), true);

	EXPECT_EQ(23u, msg.getLength());
	rewind(msg);
	EXPECT_EQ(0x8B, msg.getByte());
	EXPECT_EQ(0x11223344u, msg.get<uint32_t>());
	EXPECT_EQ(15u, msg.getByte());
	EXPECT_EQ(1u, msg.getByte());
	EXPECT_EQ(0x0102030405060708u, msg.get<uint64_t>());
	EXPECT_EQ(700u, msg.get<uint32_t>());
	EXPECT_EQ(350u, msg.get<uint32_t>());
}

TEST(ProtocolGameCastProgressTest, OmitsStartWithZeroDuration) {
	NetworkMessage msg;
	CastProgressProtocol::addStart(msg, 1, makeSnapshot(2, 0, 0), true);

	EXPECT_EQ(0u, msg.getLength());
}

TEST(ProtocolGameCastProgressTest, OmitsStartAtTheExpiredBoundary) {
	NetworkMessage msg;
	CastProgressProtocol::addStart(msg, 1, makeSnapshot(2, 700, 0), true);

	EXPECT_EQ(0u, msg.getLength());
}

TEST(ProtocolGameCastProgressTest, ClampsStartRemainingToDuration) {
	NetworkMessage msg;
	CastProgressProtocol::addStart(msg, 1, makeSnapshot(2, 700, 701), true);

	rewind(msg);
	msg.skipBytes(1 + sizeof(uint32_t) + 1 + 1 + sizeof(uint64_t) + sizeof(uint32_t));
	EXPECT_EQ(700u, msg.get<uint32_t>());
}

TEST(ProtocolGameCastProgressTest, FeatureOffLeavesCancelStreamUnchanged) {
	NetworkMessage msg;
	msg.addByte(0xAA);
	CastProgressProtocol::addCancel(msg, 0x11223344, 0x0102030405060708, false);

	EXPECT_EQ(1u, msg.getLength());
	rewind(msg);
	EXPECT_EQ(0xAA, msg.getByte());
}

TEST(ProtocolGameCastProgressTest, WritesTheExactCancelPayload) {
	NetworkMessage msg;
	CastProgressProtocol::addCancel(msg, 0x11223344, 0x0102030405060708, true);

	EXPECT_EQ(15u, msg.getLength());
	rewind(msg);
	EXPECT_EQ(0x8B, msg.getByte());
	EXPECT_EQ(0x11223344u, msg.get<uint32_t>());
	EXPECT_EQ(15u, msg.getByte());
	EXPECT_EQ(2u, msg.getByte());
	EXPECT_EQ(0x0102030405060708u, msg.get<uint64_t>());
}

TEST(ProtocolGameCastProgressTest, FeatureOffLeavesSnapshotTailAbsent) {
	NetworkMessage msg;
	msg.addByte(0xAA);
	const auto snapshot = makeSnapshot();
	CastProgressProtocol::addSnapshot(msg, &snapshot, false);

	EXPECT_EQ(1u, msg.getLength());
	rewind(msg);
	EXPECT_EQ(0xAA, msg.getByte());
}

TEST(ProtocolGameCastProgressTest, WritesInactiveSnapshotMarker) {
	NetworkMessage msg;
	CastProgressProtocol::addSnapshot(msg, nullptr, true);

	EXPECT_EQ(1u, msg.getLength());
	rewind(msg);
	EXPECT_EQ(0u, msg.getByte());
}

TEST(ProtocolGameCastProgressTest, WritesTheExactActiveSnapshotPayload) {
	NetworkMessage msg;
	const auto snapshot = makeSnapshot();
	CastProgressProtocol::addSnapshot(msg, &snapshot, true);

	EXPECT_EQ(17u, msg.getLength());
	rewind(msg);
	EXPECT_EQ(1u, msg.getByte());
	EXPECT_EQ(0x0102030405060708u, msg.get<uint64_t>());
	EXPECT_EQ(700u, msg.get<uint32_t>());
	EXPECT_EQ(350u, msg.get<uint32_t>());
}

TEST(ProtocolGameCastProgressTest, InvalidSnapshotWritesInactiveMarker) {
	NetworkMessage msg;
	const auto snapshot = makeSnapshot(2, 0, 0);
	CastProgressProtocol::addSnapshot(msg, &snapshot, true);

	EXPECT_EQ(1u, msg.getLength());
	rewind(msg);
	EXPECT_EQ(0u, msg.getByte());
}

TEST(ProtocolGameCastProgressTest, ClampsSnapshotRemainingToDuration) {
	NetworkMessage msg;
	const auto snapshot = makeSnapshot(2, 700, 701);
	CastProgressProtocol::addSnapshot(msg, &snapshot, true);

	rewind(msg);
	msg.skipBytes(1 + sizeof(uint64_t) + sizeof(uint32_t));
	EXPECT_EQ(700u, msg.get<uint32_t>());
}
