-- Headless shield profile tests for data/scripts/spells/attack/armamento_assault.lua.

local passed, failed, errors = 0, 0, {}
local state = {}

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

local function assert_before(first, second)
	local firstIndex, secondIndex
	for index, event in ipairs(state.events) do
		if event == first and not firstIndex then
			firstIndex = index
		elseif event == second and not secondIndex then
			secondIndex = index
		end
	end
	assert_equal(true, firstIndex ~= nil and secondIndex ~= nil and firstIndex < secondIndex, first .. " before " .. second)
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
CONST_SLOT_LEFT = 5
CONST_SLOT_RIGHT = 6

local registration = {}
local itemTypes = {}

logger = { error = function() end }

function createCombatArea(cardinal, diagonal)
	return { cardinal = cardinal, diagonal = diagonal }
end

function Combat()
	local combat = { parameters = {} }
	function combat:setParameter(parameter, value)
		self.parameters[parameter] = value
	end
	function combat:setCallback(_, callback)
		self.callback = callback
	end
	function combat:setArea(area)
		self.area = area
	end
	function combat:execute(player, variant)
		table.insert(state.events, "damage")
		local minimum, maximum = _G[self.callback](player, 999999, 999999)
		state.damage = { combat = self, minimum = minimum, maximum = maximum, variant = variant }
		if state.combatResult == false then
			return false
		end
		state.target.health = state.healthAfterDamage
		return true
	end
	return combat
end

function Spell()
	local spell = {}
	for _, method in ipairs({
		"name", "words", "id", "needTarget", "isAggressive", "blockWalls", "mana", "soul", "cooldown", "groupCooldown", "disciplineRequirement",
	}) do
		spell[method] = function() return true end
	end
	spell.tag = function() return true end
	spell.register = function() return true end
	registration.spell = spell
	return spell
end

function ItemType(id)
	return itemTypes[id]
end

function Position(x, y, z)
	local value = { x = x, y = y, z = z }
	function value:getDistance(other)
		return math.max(math.abs(self.x - other.x), math.abs(self.y - other.y), math.abs(self.z - other.z))
	end
	return value
end

local function positionKey(position)
	return string.format("%d,%d,%d", position.x, position.y, position.z)
end

function Tile(position)
	table.insert(state.events, "tile:" .. positionKey(position))
	return state.tiles[positionKey(position)]
end

function Creature(id)
	return state.target and state.target:getId() == id and state.target or nil
end

function addEvent()
	state.scheduled = state.scheduled + 1
end

local function item(id, weaponType, attack, defense)
	itemTypes[id] = {
		getWeaponType = function() return weaponType end,
		getAmmoType = function() return AMMO_NONE end,
		getShootRange = function() return 1 end,
	}
	return {
		getId = function() return id end,
		getAttack = function()
			state.attackReads = state.attackReads + 1
			return attack
		end,
		getDefense = function()
			state.defenseReads = state.defenseReads + 1
			return defense
		end,
	}
end

local function reset(options)
	options = options or {}
	state = {
		events = {}, tiles = {}, scheduled = 0, attackReads = 0, defenseReads = 0,
		combatResult = options.combatResult,
		healthAfterDamage = options.healthAfterDamage == nil and 40 or options.healthAfterDamage,
	}
	local casterPosition = Position(options.casterX or 10, options.casterY or 10, 7)
	local targetPosition = Position(options.targetX or 11, options.targetY or 10, 7)
	local target = {
		health = 100,
		position = targetPosition,
		getId = function() return 77 end,
		getPosition = function(self) return self.position end,
		isRemoved = function()
			table.insert(state.events, "removed-check")
			return options.removed == true
		end,
		getHealth = function(self)
			table.insert(state.events, "health-check")
			return self.health
		end,
		move = function(self, tile, flags)
			table.insert(state.events, "move")
			state.move = { tile = tile, flags = flags }
			if options.moveSucceeds == false then return 1 end
			self.position = tile.position
			return 0
		end,
	}
	state.target = target
	local slots = options.slots or {}
	local player = {
		getId = function() return 42 end,
		getStatPhysicalAttack = function() return options.physicalAttack == nil and 20 or options.physicalAttack end,
		getStatMagicalAttack = function() return 999 end,
		getPosition = function() return casterPosition end,
		getSlotItem = function(_, slot)
			table.insert(state.events, "slot:" .. slot)
			return slots[slot]
		end,
	}
	return player, target, { getNumber = function() return 77 end }
end

local function destination(x, y)
	local tile = { position = Position(x, y, 7) }
	state.tiles[positionKey(tile.position)] = tile
	return tile
end

dofile("data/scripts/spells/attack/armamento_assault.lua")

test("supported left weapon wins over a right shield", function()
	local sword = item(100, WEAPON_SWORD, 30, 999)
	local shield = item(101, WEAPON_SHIELD, 999, 80)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = sword, [CONST_SLOT_RIGHT] = shield } })
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(CONST_ME_DRAWBLOOD, state.damage.combat.parameters[COMBAT_PARAM_EFFECT])
	assert_equal(1, state.attackReads)
	assert_equal(0, state.defenseReads)
	assert_equal(nil, state.move)
end)

test("supported right weapon wins over a left shield", function()
	local shield = item(102, WEAPON_SHIELD, 999, 80)
	local sword = item(103, WEAPON_SWORD, 30, 999)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = shield, [CONST_SLOT_RIGHT] = sword } })
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(CONST_ME_DRAWBLOOD, state.damage.combat.parameters[COMBAT_PARAM_EFFECT])
	assert_equal(1, state.attackReads)
	assert_equal(0, state.defenseReads)
end)

test("left shield has fallback priority over right shield", function()
	local leftShield = item(104, WEAPON_SHIELD, 999, 31)
	local rightShield = item(105, WEAPON_SHIELD, 999, 90)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = leftShield, [CONST_SLOT_RIGHT] = rightShield } })
	destination(12, 10)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(-61, state.damage.minimum)
	assert_equal(1, state.defenseReads)
	assert_equal(0, state.attackReads)
end)

test("right shield is selected after unsupported left equipment", function()
	local unsupported = item(106, WEAPON_NONE, 999, 999)
	local rightShield = item(107, WEAPON_SHIELD, 999, 40)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = unsupported, [CONST_SLOT_RIGHT] = rightShield } })
	destination(12, 10)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(-70, state.damage.minimum)
	assert_equal(1, state.defenseReads)
end)

test("unsupported hands cancel before damage or power reads", function()
	local unsupported = item(108, WEAPON_NONE, 999, 999)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = unsupported } })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(nil, state.damage)
	assert_equal(0, state.attackReads)
	assert_equal(0, state.defenseReads)
end)

test("shield uses effective defense as its only equipment power", function()
	local shield = item(109, WEAPON_SHIELD, 500, 77)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = shield } })
	destination(12, 10)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(-107, state.damage.minimum)
	assert_equal(-107, state.damage.maximum)
	assert_equal(CONST_ME_BLOCKHIT, state.damage.combat.parameters[COMBAT_PARAM_EFFECT])
	assert_equal(1, state.defenseReads)
	assert_equal(0, state.attackReads)
end)

test("shield damages before one successful opposite move", function()
	local shield = item(110, WEAPON_SHIELD, 0, 30)
	local player, target, variant = reset({ slots = { [CONST_SLOT_LEFT] = shield } })
	local tile = destination(12, 10)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(tile, state.move.tile)
	assert_equal(0, state.move.flags)
	assert_equal(tile.position, target.position)
	assert_before("damage", "move")
end)

test("blocked destination preserves completed damage and position", function()
	local shield = item(111, WEAPON_SHIELD, 0, 30)
	local player, target, variant = reset({ slots = { [CONST_SLOT_LEFT] = shield }, moveSucceeds = false })
	local original = target.position
	destination(12, 10)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(-60, state.damage.minimum)
	assert_equal(original, target.position)
	assert_equal(0, state.move.flags)
end)

test("missing destination preserves damage without movement", function()
	local shield = item(112, WEAPON_SHIELD, 0, 30)
	local player, target, variant = reset({ slots = { [CONST_SLOT_LEFT] = shield } })
	local original = target.position
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(-60, state.damage.minimum)
	assert_equal(original, target.position)
	assert_equal(nil, state.move)
end)

test("shield skips movement when damage leaves the target dead", function()
	local shield = item(113, WEAPON_SHIELD, 0, 30)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = shield }, healthAfterDamage = 0 })
	destination(12, 10)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(nil, state.move)
	assert_equal("health-check", state.events[#state.events])
end)

test("shield skips health and movement checks for a removed target", function()
	local shield = item(114, WEAPON_SHIELD, 0, 30)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = shield }, removed = true })
	destination(12, 10)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(nil, state.move)
	assert_equal("removed-check", state.events[#state.events])
end)

test("Combat denial prevents shield movement", function()
	local shield = item(115, WEAPON_SHIELD, 0, 30)
	local player, _, variant = reset({ slots = { [CONST_SLOT_LEFT] = shield }, combatResult = false })
	destination(12, 10)
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(nil, state.move)
end)

test("diagonal shield knockback is synchronous with no delayed work", function()
	local shield = item(116, WEAPON_SHIELD, 0, 30)
	local player, _, variant = reset({
		slots = { [CONST_SLOT_LEFT] = shield }, casterX = 10, casterY = 10, targetX = 9, targetY = 9,
	})
	local tile = destination(8, 8)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(tile, state.move.tile)
	assert_equal(0, state.scheduled)
	assert_before("damage", "move")
end)

print(string.format("\n%d passed, %d failed", passed, failed))
if #errors > 0 then
	for _, entry in ipairs(errors) do
		print(string.format("  FAIL: %s\n        %s", entry.name, entry.err))
	end
	os.exit(1)
end
