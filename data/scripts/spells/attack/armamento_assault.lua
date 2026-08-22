local BASE_POWER = 10
local PHYSICAL_COEFFICIENT = 1.0
local MAGICAL_COEFFICIENT = 0.0
local EQUIPMENT_COEFFICIENT = 1.0

local requiredVisualResources = {
	{ name = "CONST_ME_DRAWBLOOD", value = CONST_ME_DRAWBLOOD },
	{ name = "CONST_ME_HITAREA", value = CONST_ME_HITAREA },
	{ name = "CONST_ME_BLOCKHIT", value = CONST_ME_BLOCKHIT },
	{ name = "CONST_ANI_ARROW", value = CONST_ANI_ARROW },
	{ name = "CONST_ANI_BOLT", value = CONST_ANI_BOLT },
}

for _, resource in ipairs(requiredVisualResources) do
	if type(resource.value) ~= "number" then
		logger.error("[ArmamentoAssault] Missing visual resource: {}", resource.name)
		return false
	end
end

local pendingEquipmentPower = {}

local function calculateDamage(player, equipmentPower, multiplier)
	local power = BASE_POWER
		+ player:getStatPhysicalAttack() * PHYSICAL_COEFFICIENT
		+ player:getStatMagicalAttack() * MAGICAL_COEFFICIENT
		+ equipmentPower * EQUIPMENT_COEFFICIENT
	return math.floor(power * multiplier)
end

function onGetArmamentoAssaultPrimaryValues(player, level, magicLevel)
	local equipmentPower = pendingEquipmentPower[player:getId()]
	if equipmentPower == nil then
		return 0, 0
	end
	local damage = calculateDamage(player, equipmentPower, 1.0)
	return -damage, -damage
end

function onGetArmamentoAssaultSecondaryValues(player, level, magicLevel)
	local equipmentPower = pendingEquipmentPower[player:getId()]
	if equipmentPower == nil then
		return 0, 0
	end
	local damage = calculateDamage(player, equipmentPower, 0.5)
	return -damage, -damage
end

local function createCombat(effect, callback, distanceEffect, area)
	local combat = Combat()
	combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_PHYSICALDAMAGE)
	combat:setParameter(COMBAT_PARAM_EFFECT, effect)
	combat:setParameter(COMBAT_PARAM_BLOCKARMOR, 1)
	combat:setParameter(COMBAT_PARAM_BLOCKSHIELD, 0)
	if distanceEffect then
		combat:setParameter(COMBAT_PARAM_DISTANCEEFFECT, distanceEffect)
	end
	if area then
		combat:setArea(area)
	end
	combat:setCallback(CALLBACK_PARAM_LEVELMAGICVALUE, callback)
	return combat
end

local axeSecondaryArea = createCombatArea({
	{ 1, 2, 1 },
}, {
	{ 0, 0, 1 },
	{ 0, 2, 0 },
	{ 1, 0, 0 },
})

local clubSecondaryArea = createCombatArea({
	{ 0, 1, 0 },
	{ 1, 2, 1 },
	{ 0, 1, 0 },
})

local profiles = {
	sword = {
		combat = createCombat(CONST_ME_DRAWBLOOD, "onGetArmamentoAssaultPrimaryValues"),
		range = 1,
	},
	bow = {
		combat = createCombat(CONST_ME_HITAREA, "onGetArmamentoAssaultPrimaryValues", CONST_ANI_ARROW),
	},
	crossbow = {
		combat = createCombat(CONST_ME_HITAREA, "onGetArmamentoAssaultPrimaryValues", CONST_ANI_BOLT),
	},
	axe = {
		combat = createCombat(CONST_ME_HITAREA, "onGetArmamentoAssaultPrimaryValues"),
		secondaryCombat = createCombat(CONST_ME_HITAREA, "onGetArmamentoAssaultSecondaryValues", nil, axeSecondaryArea),
		range = 1,
	},
	club = {
		combat = createCombat(CONST_ME_HITAREA, "onGetArmamentoAssaultPrimaryValues"),
		secondaryCombat = createCombat(CONST_ME_HITAREA, "onGetArmamentoAssaultSecondaryValues", nil, clubSecondaryArea),
		range = 1,
	},
}

local function classifyWeapon(item)
	if not item then
		return nil
	end
	local itemType = ItemType(item:getId())
	if not itemType then
		return nil
	end

	local weaponType = itemType:getWeaponType()
	if weaponType == WEAPON_SWORD then
		return "sword", profiles.sword.range
	elseif weaponType == WEAPON_AXE then
		return "axe", profiles.axe.range
	elseif weaponType == WEAPON_CLUB then
		return "club", profiles.club.range
	end
	if weaponType ~= WEAPON_DISTANCE then
		return nil
	end

	local ammoType = itemType:getAmmoType()
	if ammoType == AMMO_ARROW then
		return "bow", itemType:getShootRange()
	elseif ammoType == AMMO_BOLT then
		return "crossbow", itemType:getShootRange()
	end
	return nil
end

local function selectWeapon(player)
	for _, slot in ipairs({ CONST_SLOT_LEFT, CONST_SLOT_RIGHT }) do
		local item = player:getSlotItem(slot)
		local profile, range = classifyWeapon(item)
		if profile then
			return item, profile, range
		end
	end
	return nil
end

local function executeProfileCombats(player, variant, profile, equipmentPower)
	local playerId = player:getId()
	pendingEquipmentPower[playerId] = equipmentPower
	local primarySucceeded, primaryResult = pcall(profile.combat.execute, profile.combat, player, variant)
	if not primarySucceeded then
		pendingEquipmentPower[playerId] = nil
		logger.error("[ArmamentoAssault] Combat execution failed: {}", primaryResult)
		return false
	end
	if not primaryResult then
		pendingEquipmentPower[playerId] = nil
		return false
	end

	if profile.secondaryCombat then
		local secondarySucceeded, secondaryError = pcall(profile.secondaryCombat.execute, profile.secondaryCombat, player, variant)
		pendingEquipmentPower[playerId] = nil
		if not secondarySucceeded then
			logger.error("[ArmamentoAssault] Secondary Combat execution failed: {}", secondaryError)
		end
		return true
	end

	pendingEquipmentPower[playerId] = nil
	return true
end

local spell = Spell("instant")

function spell.onCastSpell(player, variant)
	local targetId = variant:getNumber()
	if targetId == 0 then
		return false
	end
	local target = Creature(targetId)
	if not target then
		return false
	end

	local weapon, profile, range = selectWeapon(player)
	if not weapon then
		return false
	end
	if player:getPosition():getDistance(target:getPosition()) > range then
		return false
	end

	return executeProfileCombats(player, variant, profiles[profile], weapon:getAttack())
end

spell:name("Assault")
spell:words("assault")
spell:id(298)
spell:needTarget(true)
spell:isAggressive(true)
spell:blockWalls(true)
spell:mana(0)
spell:soul(0)
spell:cooldown(1000)
spell:groupCooldown(0)
spell:disciplineRequirement(1, 1)
for _, tag in ipairs({
	"category.art",
	"discipline.armament",
	"execution.attack",
	"function.offensive",
	"damage.physical",
	"damage.neutral",
}) do
	spell:tag(tag)
end
spell:register()
