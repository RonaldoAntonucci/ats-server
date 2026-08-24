/**
 * Canary - A free and open-source MMORPG server emulator
 * Copyright (©) 2019–present OpenTibiaBR <opentibiabr@outlook.com>
 * Repository: https://github.com/opentibiabr/canary
 * License: https://github.com/opentibiabr/canary/blob/main/LICENSE
 * Contributors: https://github.com/opentibiabr/canary/graphs/contributors
 * Website: https://docs.opentibiabr.com/
 */

#include "creatures/combat/combat.hpp"
#include "creatures/creature.hpp"
#include "lua/callbacks/events_callbacks.hpp"

#include <gtest/gtest.h>

namespace {
	class AssaultCombatTarget final : public Creature {
	public:
		AssaultCombatTarget() {
			position = Position { 100, 100, 7 };
		}

		const std::string &getName() const override {
			return name;
		}

		const std::string &getTypeName() const override {
			return name;
		}

		const std::string &getNameDescription() const override {
			return name;
		}

		std::string getDescription(int32_t) override {
			return name;
		}

		CreatureType_t getType() const override {
			return CREATURETYPE_MONSTER;
		}

		void setID() override {
		}

		void removeList() override {
		}

		void addList() override {
		}

		BlockType_t blockHit(const std::shared_ptr<Creature> &, const CombatType_t &, int32_t &damage, bool checkDefense, bool checkArmor, bool) override {
			shieldChecked = checkDefense;
			armorChecked = checkArmor;
			damageBeforeArmor = damage;
			if (checkArmor) {
				damage = std::max<int32_t>(0, damage - armorReduction);
			}
			damageAfterArmor = damage;
			return BLOCK_NONE;
		}

		void drainHealth(const std::shared_ptr<Creature> &, int32_t damage) override {
			drainedHealth = damage;
			changeHealth(-damage, false);
		}

		bool armorChecked = false;
		bool shieldChecked = false;
		int32_t damageBeforeArmor = 0;
		int32_t damageAfterArmor = 0;
		int32_t drainedHealth = 0;

	private:
		static constexpr int32_t armorReduction = 20;
		const std::string name = "assault combat target";
	};
} // namespace

TEST(ArmamentoAssaultLegacyCombatIntegrationTest, ExistingCombatAppliesArmorAndLegacyDamageModifier) {
	g_callbacks().clear();
	const auto target = std::make_shared<AssaultCombatTarget>();
	target->setBuff(BUFF_DAMAGERECEIVED, 50);

	Combat combat;
	ASSERT_TRUE(combat.setParam(COMBAT_PARAM_TYPE, COMBAT_PHYSICALDAMAGE));
	ASSERT_TRUE(combat.setParam(COMBAT_PARAM_BLOCKARMOR, 1));
	ASSERT_TRUE(combat.setParam(COMBAT_PARAM_BLOCKSHIELD, 0));
	ASSERT_TRUE(combat.setParam(COMBAT_PARAM_AGGRESSIVE, 0));
	combat.setPlayerCombatValues(COMBAT_FORMULA_DAMAGE, -100, 0, -100, 0);

	ASSERT_TRUE(combat.doCombat(nullptr, target));
	EXPECT_TRUE(target->armorChecked);
	EXPECT_FALSE(target->shieldChecked);
	EXPECT_EQ(150, target->damageBeforeArmor);
	EXPECT_EQ(130, target->damageAfterArmor);
	EXPECT_EQ(195, target->drainedHealth);
	EXPECT_EQ(805, target->getHealth());
}
