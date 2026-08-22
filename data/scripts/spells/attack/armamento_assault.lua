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

local function createPrimaryCombat(effect, distanceEffect)
	local combat = Combat()
	combat:setParameter(COMBAT_PARAM_TYPE, COMBAT_PHYSICALDAMAGE)
	combat:setParameter(COMBAT_PARAM_EFFECT, effect)
	combat:setParameter(COMBAT_PARAM_BLOCKARMOR, 1)
	combat:setParameter(COMBAT_PARAM_BLOCKSHIELD, 0)
	if distanceEffect then
		combat:setParameter(COMBAT_PARAM_DISTANCEEFFECT, distanceEffect)
	end
	combat:setCallback(CALLBACK_PARAM_LEVELMAGICVALUE, "onGetArmamentoAssaultPrimaryValues")
	return combat
end

local profiles = {
	sword = {
		combat = createPrimaryCombat(CONST_ME_DRAWBLOOD),
		range = 1,
	},
	bow = {
		combat = createPrimaryCombat(CONST_ME_HITAREA, CONST_ANI_ARROW),
	},
	crossbow = {
		combat = createPrimaryCombat(CONST_ME_HITAREA, CONST_ANI_BOLT),
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

local function executePrimaryCombat(player, variant, combat, equipmentPower)
	local playerId = player:getId()
	pendingEquipmentPower[playerId] = equipmentPower
	local succeeded, result = pcall(combat.execute, combat, player, variant)
	pendingEquipmentPower[playerId] = nil
	if not succeeded then
		logger.error("[ArmamentoAssault] Combat execution failed: {}", result)
		return false
	end
	return result
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

	return executePrimaryCombat(player, variant, profiles[profile].combat, weapon:getAttack())
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
