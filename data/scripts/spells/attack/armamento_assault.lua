local spell = Spell("instant")

local profileEffects = {
	sword = { impact = CONST_ME_DRAWBLOOD },
	axe = { impact = CONST_ME_HITAREA },
	club = { impact = CONST_ME_HITAREA },
	bow = { impact = CONST_ME_HITAREA, distance = CONST_ANI_ARROW },
	crossbow = { impact = CONST_ME_HITAREA, distance = CONST_ANI_BOLT },
	shield = { impact = CONST_ME_NONE },
}

local function sign(value)
	if value < 0 then
		return -1
	elseif value > 0 then
		return 1
	end
	return 0
end

local function secondaryOffsets(profile, casterPosition, targetPosition)
	if profile == "axe" then
		local dx = sign(targetPosition.x - casterPosition.x)
		local dy = sign(targetPosition.y - casterPosition.y)
		return {
			{ x = -dy, y = dx },
			{ x = dy, y = -dx },
		}
	elseif profile == "club" then
		return {
			{ x = 1, y = 0 },
			{ x = -1, y = 0 },
			{ x = 0, y = 1 },
			{ x = 0, y = -1 },
		}
	end
	return {}
end

local function collectTargets(player, primaryTarget, context, profile)
	local targets = {
		primaryTarget,
	}
	if profile ~= "axe" and profile ~= "club" then
		return targets
	end

	local targetPosition = primaryTarget:getPosition()
	local offsets = secondaryOffsets(profile, player:getPosition(), targetPosition)
	local seen = { [primaryTarget:getId()] = true }
	for _, offset in ipairs(offsets) do
		local tile = Tile(Position(targetPosition.x + offset.x, targetPosition.y + offset.y, targetPosition.z))
		local candidate = tile and tile:getTopCreature()
		if candidate then
			local candidateId = candidate:getId()
			if not seen[candidateId] then
				seen[candidateId] = true
				if context:canAffect(candidate) then
					table.insert(targets, candidate)
				end
			end
		end
	end
	return targets
end

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
	local profile = context:getProfile()
	local targets = collectTargets(player, primaryTarget, context, profile)

	local committed = context:commit(primaryTarget)
	if not committed then
		return false
	end

	local effects = profileEffects[profile]
	if effects.distance then
		player:getPosition():sendDistanceEffect(primaryTarget:getPosition(), effects.distance)
	end
	local primaryBaseDamage = context:getPrimaryBaseDamage()
	local secondaryBaseDamage = #targets > 1 and context:getSecondaryBaseDamage() or 0
	for index, target in ipairs(targets) do
		local baseDamage = index == 1 and primaryBaseDamage or secondaryBaseDamage
		doTargetCombatHealth(player, target, COMBAT_PHYSICALDAMAGE, -baseDamage, -baseDamage, effects.impact, ORIGIN_SPELL, nil, "Assault")
	end
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
