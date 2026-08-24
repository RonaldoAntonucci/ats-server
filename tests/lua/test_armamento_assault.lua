-- Headless contract tests for data/scripts/spells/attack/armamento_assault.lua.

local passed, failed, errors = 0, 0, {}

local function test(name, fn)
	local ok, err = pcall(fn)
	if ok then
		passed = passed + 1
	else
		failed = failed + 1
		table.insert(errors, { name = name, err = err })
	end
end

local function assert_equal(expected, actual, message)
	if expected ~= actual then
		error((message or "values differ") .. ": expected " .. tostring(expected) .. ", got " .. tostring(actual), 2)
	end
end

local function assert_array(expected, actual, message)
	assert_equal(#expected, #actual, (message or "array") .. " length")
	for index, value in ipairs(expected) do
		assert_equal(value, actual[index], (message or "array") .. " at " .. index)
	end
end

COMBAT_PHYSICALDAMAGE = 1
COMBAT_PARAM_TYPE = 2
COMBAT_PARAM_EFFECT = 3
COMBAT_PARAM_BLOCKARMOR = 4
COMBAT_PARAM_BLOCKSHIELD = 5
COMBAT_PARAM_DISTANCEEFFECT = 6
CALLBACK_PARAM_LEVELMAGICVALUE = 7
CONST_ME_DRAWBLOOD = 10
CONST_ME_HITAREA = 11
CONST_ME_BLOCKHIT = 12
CONST_ANI_ARROW = 20
CONST_ANI_BOLT = 21
WEAPON_NONE = 0
WEAPON_SWORD = 1
WEAPON_DISTANCE = 2
WEAPON_AXE = 3
WEAPON_CLUB = 4
WEAPON_SHIELD = 5
AMMO_NONE = 0
AMMO_ARROW = 1
AMMO_BOLT = 2
AMMO_SPEAR = 3
CONST_SLOT_LEFT = 5
CONST_SLOT_RIGHT = 6

local registration = { tags = {} }
local combats = {}
local itemTypes = {}
local state = {}

logger = {
	error = function(message, value)
		table.insert(state.logs, message .. "|" .. tostring(value))
	end,
}

function Combat()
	local combat = { parameters = {} }
	function combat:setParameter(parameter, value)
		self.parameters[parameter] = value
	end
	function combat:setCallback(parameter, callback)
		self.callbackParameter = parameter
		self.callbackName = callback
		self.callback = _G[callback]
		_G[callback] = nil
		return self.callback ~= nil
	end
	function combat:setArea(area)
		self.area = area
	end
	function combat:execute(player, variant)
		table.insert(state.events, "combat")
		if state.combatError then
			error(state.combatError)
		end
		local minimum, maximum = self.callback(player, 999999, 888888)
		table.insert(state.executions, {
			combat = self,
			player = player,
			variant = variant,
			minimum = minimum,
			maximum = maximum,
		})
		return state.combatResult ~= false
	end
	table.insert(combats, combat)
	return combat
end

function createCombatArea(cardinal, diagonal)
	return { cardinal = cardinal, diagonal = diagonal }
end

function Spell(kind)
	registration.kind = kind
	local spell = {}
	for _, method in ipairs({
		"name",
		"words",
		"id",
		"needTarget",
		"isAggressive",
		"blockWalls",
		"mana",
		"soul",
		"cooldown",
		"groupCooldown",
		"disciplineRequirement",
	}) do
		spell[method] = function(_, ...)
			registration[method] = { ... }
			return true
		end
	end
	spell.tag = function(_, tag)
		table.insert(registration.tags, tag)
		return tag ~= ""
	end
	spell.register = function()
		registration.registered = true
		return true
	end
	registration.spell = spell
	return spell
end

function ItemType(id)
	return itemTypes[id]
end

function Creature(id)
	return state.targets[id]
end

local function position(x, y, z)
	local value = { x = x, y = y, z = z or 7 }
	function value:getDistance(other)
		return math.max(math.abs(self.x - other.x), math.abs(self.y - other.y), math.abs(self.z - other.z))
	end
	return value
end

local function item(id, weaponType, ammoType, attack, shootRange)
	itemTypes[id] = {
		getWeaponType = function()
			return weaponType
		end,
		getAmmoType = function()
			return ammoType or AMMO_NONE
		end,
		getShootRange = function()
			return shootRange or 1
		end,
	}
	return {
		getId = function()
			return id
		end,
		getAttack = function()
			state.attackReads = state.attackReads + 1
			return attack
		end,
	}
end

local function reset(options)
	options = options or {}
	state = {
		events = {},
		executions = {},
		logs = {},
		targets = {},
		attackReads = 0,
		combatResult = options.combatResult,
		combatError = options.combatError,
	}
	local target = {
		getPosition = function()
			return position(options.targetX or 1, options.targetY or 0, options.targetZ or 7)
		end,
	}
	if options.unresolved ~= true then
		state.targets[77] = target
	end
	local slots = options.slots or {}
	local player = {
		getId = function()
			return options.playerId or 42
		end,
		getStatPhysicalAttack = function()
			return options.physicalAttack == nil and 20 or options.physicalAttack
		end,
		getStatMagicalAttack = function()
			return options.magicalAttack == nil and 50 or options.magicalAttack
		end,
		getPosition = function()
			return position(options.casterX or 0, options.casterY or 0, options.casterZ or 7)
		end,
		getSlotItem = function(_, slot)
			if slot ~= CONST_SLOT_LEFT and slot ~= CONST_SLOT_RIGHT then
				error("Assault consulted a non-hand inventory slot")
			end
			table.insert(state.events, "slot:" .. slot)
			return slots[slot]
		end,
	}
	local variant = {
		getNumber = function()
			return options.targetId == nil and 77 or options.targetId
		end,
	}
	return player, target, variant
end

local function findCombat(distanceEffect)
	for _, combat in ipairs(combats) do
		if combat.parameters[COMBAT_PARAM_DISTANCEEFFECT] == distanceEffect then
			return combat
		end
	end
	return nil
end

local function findCombatByEffect(effect)
	for _, combat in ipairs(combats) do
		if combat.parameters[COMBAT_PARAM_EFFECT] == effect then
			return combat
		end
	end
	return nil
end

dofile("data/scripts/spells/attack/armamento_assault.lua")

test("registers Assault with stable identity", function()
	assert_equal("instant", registration.kind)
	assert_array({ "Assault" }, registration.name)
	assert_array({ "assault" }, registration.words)
	assert_array({ 298 }, registration.id)
	assert_equal(true, registration.registered)
end)

test("declares target aggression and wall blocking", function()
	assert_array({ true }, registration.needTarget)
	assert_array({ true }, registration.isAggressive)
	assert_array({ true }, registration.blockWalls)
end)

test("requires Armamento id 1 at rank 1", function()
	assert_array({ 1, 1 }, registration.disciplineRequirement)
end)

test("declares zero resources and only individual cooldown", function()
	assert_array({ 0 }, registration.mana)
	assert_array({ 0 }, registration.soul)
	assert_array({ 1000 }, registration.cooldown)
	assert_array({ 0 }, registration.groupCooldown)
	assert_equal(nil, registration.group)
end)

test("adds the six free-form tags in normative order", function()
	assert_array({
		"category.art",
		"discipline.armament",
		"execution.attack",
		"function.offensive",
		"damage.physical",
		"damage.neutral",
	}, registration.tags)
end)

test("does not declare legacy progression or learning gates", function()
	for _, forbidden in ipairs({ "vocation", "level", "magicLevel", "needLearn", "needWeapon", "premium" }) do
		assert_equal(nil, registration[forbidden], forbidden)
	end
end)

test("configures every Assault Combat for legacy armor but not shield blocking", function()
	assert_equal(8, #combats)
	for _, combat in ipairs(combats) do
		assert_equal(COMBAT_PHYSICALDAMAGE, combat.parameters[COMBAT_PARAM_TYPE])
		assert_equal(1, combat.parameters[COMBAT_PARAM_BLOCKARMOR])
		assert_equal(0, combat.parameters[COMBAT_PARAM_BLOCKSHIELD])
		assert_equal(CALLBACK_PARAM_LEVELMAGICVALUE, combat.callbackParameter)
		assert_equal(true, combat.callbackName == "onGetArmamentoAssaultPrimaryValues" or combat.callbackName == "onGetArmamentoAssaultSecondaryValues")
		assert_equal("function", type(combat.callback))
	end
end)

test("configures sword bow and crossbow effects on Combat", function()
	assert_equal(CONST_ME_DRAWBLOOD, findCombatByEffect(CONST_ME_DRAWBLOOD).parameters[COMBAT_PARAM_EFFECT])
	assert_equal(CONST_ME_HITAREA, findCombat(CONST_ANI_ARROW).parameters[COMBAT_PARAM_EFFECT])
	assert_equal(CONST_ME_HITAREA, findCombat(CONST_ANI_BOLT).parameters[COMBAT_PARAM_EFFECT])
end)

test("rejects a missing target before equipment lookup", function()
	local player, _, variant = reset({ targetId = 0 })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(0, #state.events)
	assert_equal(0, #state.executions)
end)

test("rejects an unresolved target before equipment lookup", function()
	local player, _, variant = reset({ unresolved = true })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(0, #state.events)
end)

test("rejects unsupported hand equipment before Combat", function()
	local unsupported = item(100, WEAPON_NONE, AMMO_NONE, 90, 1)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = unsupported } })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(0, #state.executions)
	assert_equal(0, state.attackReads)
end)

test("selects the supported left weapon before the right", function()
	local sword = item(101, WEAPON_SWORD, AMMO_NONE, 30, 1)
	local crossbow = item(102, WEAPON_DISTANCE, AMMO_BOLT, 99, 7)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = sword, [CONST_SLOT_RIGHT] = crossbow } })
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_array({ "slot:" .. CONST_SLOT_LEFT, "combat" }, state.events)
	assert_equal(-60, state.executions[1].minimum)
	assert_equal(CONST_ME_DRAWBLOOD, state.executions[1].combat.parameters[COMBAT_PARAM_EFFECT])
end)

test("falls through unsupported left equipment to a supported right weapon", function()
	local unsupported = item(103, WEAPON_NONE, AMMO_NONE, 90, 1)
	local bow = item(104, WEAPON_DISTANCE, AMMO_ARROW, 30, 6)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = unsupported, [CONST_SLOT_RIGHT] = bow }, targetX = 6 })
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_array({ "slot:" .. CONST_SLOT_LEFT, "slot:" .. CONST_SLOT_RIGHT, "combat" }, state.events)
	assert_equal(CONST_ANI_ARROW, state.executions[1].combat.parameters[COMBAT_PARAM_DISTANCEEFFECT])
end)

test("sword requires an adjacent primary target", function()
	local sword = item(105, WEAPON_SWORD, AMMO_NONE, 30, 1)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = sword }, targetX = 2 })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(0, #state.executions)
	assert_equal(0, state.attackReads)
end)

test("bow works at its exact shoot range without ammunition inventory", function()
	local bow = item(106, WEAPON_DISTANCE, AMMO_ARROW, 30, 6)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = bow }, targetX = 6 })
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(CONST_ANI_ARROW, state.executions[1].combat.parameters[COMBAT_PARAM_DISTANCEEFFECT])
	assert_equal(1, state.attackReads)
end)

test("bow rejects a target beyond its current shoot range", function()
	local bow = item(107, WEAPON_DISTANCE, AMMO_ARROW, 30, 6)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = bow }, targetX = 7 })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(0, #state.executions)
end)

test("crossbow uses bolt metadata and ignores ammunition inventory", function()
	local crossbow = item(108, WEAPON_DISTANCE, AMMO_BOLT, 30, 7)
	local player, _, variant = reset({ slots = { [CONST_SLOT_RIGHT] = crossbow }, targetX = 7 })
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(CONST_ANI_BOLT, state.executions[1].combat.parameters[COMBAT_PARAM_DISTANCEEFFECT])
	assert_equal(1, state.attackReads)
end)

test("rejects unsupported distance ammunition metadata", function()
	local spear = item(109, WEAPON_DISTANCE, AMMO_SPEAR, 90, 5)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = spear } })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(0, #state.executions)
end)

test("calculates exact deterministic Lua damage and ignores callback level values", function()
	local sword = item(110, WEAPON_SWORD, AMMO_NONE, 30, 1)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = sword }, physicalAttack = 20, magicalAttack = 999 })
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(-60, state.executions[1].minimum)
	assert_equal(-60, state.executions[1].maximum)
end)

test("uses the effective attack returned by the item instance", function()
	local overriddenSword = item(111, WEAPON_SWORD, AMMO_NONE, 77, 1)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = overriddenSword }, physicalAttack = 20 })
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(-107, state.executions[1].minimum)
end)

test("submits zero deterministic base damage through Combat", function()
	local sword = item(112, WEAPON_SWORD, AMMO_NONE, 0, 1)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = sword }, physicalAttack = -10 })
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(0, state.executions[1].minimum)
	assert_equal(0, state.executions[1].maximum)
end)

test("clears the formula bridge after a successful execution", function()
	local sword = item(113, WEAPON_SWORD, AMMO_NONE, 30, 1)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = sword } })
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	local minimum, maximum = combats[1].callback(player, 1, 1)
	assert_equal(0, minimum)
	assert_equal(0, maximum)
end)

test("returns Combat denial and still clears the formula bridge", function()
	local sword = item(114, WEAPON_SWORD, AMMO_NONE, 30, 1)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = sword }, combatResult = false })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	local minimum = combats[1].callback(player, 1, 1)
	assert_equal(0, minimum)
end)

test("contains a Combat error and clears the formula bridge", function()
	local sword = item(115, WEAPON_SWORD, AMMO_NONE, 30, 1)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = sword }, combatError = "synthetic failure" })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(1, #state.logs)
	local minimum = combats[1].callback(player, 1, 1)
	assert_equal(0, minimum)
end)

print(string.format("\n%d passed, %d failed", passed, failed))
if #errors > 0 then
	for _, entry in ipairs(errors) do
		print(string.format("  FAIL: %s\n        %s", entry.name, entry.err))
	end
	os.exit(1)
end
