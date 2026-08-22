local spell = Spell("instant")

function spell.onCastSpell(player, variant)
	local targetId = variant:getNumber()
	if targetId == 0 then
		return false
	end

	local primaryTarget = Creature(targetId)
	if not primaryTarget then
		return false
	end

	local context = spell:createOffensiveContext(player)
	if not context then
		return false
	end

	local validTarget = context:validatePrimaryTarget(primaryTarget)
	if not validTarget then
		return false
	end

	local committed = context:commit(primaryTarget)
	if not committed then
		return false
	end

	local baseDamage = context:getPrimaryBaseDamage()
	doTargetCombatHealth(player, primaryTarget, COMBAT_PHYSICALDAMAGE, -baseDamage, -baseDamage, CONST_ME_NONE, ORIGIN_SPELL, nil, "Assault")
	return true
end

spell:name("Assault")
spell:words("assault")
spell:id(298)
spell:needTarget(true)
spell:isAggressive(true)
spell:mana(0)
spell:soul(0)
spell:cooldown(1000)
spell:groupCooldown(0)
spell:disciplineRequirement(1, 1)
spell:offensiveParameters({
	basePower = 10,
	physicalCoefficient = 1.0,
	magicalCoefficient = 0.0,
	equipmentCoefficient = 1.0,
	secondaryMultiplier = 0.5,
	cooldownMilliseconds = 1000,
})
spell:baseTags({
	"category.art",
	"damage.neutral",
	"damage.physical",
	"discipline.armament",
	"execution.attack",
	"function.offensive",
})
spell:profileTags("sword", { "execution.contact", "weapon.sword" })
spell:profileTags("axe", { "execution.area", "execution.contact", "weapon.axe" })
spell:profileTags("club", { "execution.area", "execution.contact", "weapon.club" })
spell:profileTags("bow", { "execution.projectile", "weapon.bow" })
spell:profileTags("crossbow", { "execution.projectile", "weapon.crossbow" })
spell:profileTags("shield", { "equipment.shield", "execution.contact", "function.control", "mechanic.knockback" })
spell:register()
