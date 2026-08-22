-- Headless area-composition tests for data/scripts/spells/attack/armamento_assault.lua.

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
WEAPON_SWORD = 1
WEAPON_DISTANCE = 2
WEAPON_AXE = 3
WEAPON_CLUB = 4
AMMO_NONE = 0
AMMO_ARROW = 1
AMMO_BOLT = 2
CONST_SLOT_LEFT = 5
CONST_SLOT_RIGHT = 6

local registration = {}
local state = {}
local itemTypes = {}
local areas = {}
local combats = {}

logger = { error = function() end }

local function positionKey(x, y, z)
	return string.format("%d,%d,%d", x, y, z or 7)
end

local function sign(value)
	if value < 0 then
		return -1
	elseif value > 0 then
		return 1
	end
	return 0
end

local function secondaryOffsets(area, casterPosition, targetPosition)
	if #area.cardinal == 1 then
		local dx = sign(targetPosition.x - casterPosition.x)
		local dy = sign(targetPosition.y - casterPosition.y)
		return {
			{ x = -dy, y = dx },
			{ x = dy, y = -dx },
		}
	end
	return {
		{ x = 1, y = 0 },
		{ x = -1, y = 0 },
		{ x = 0, y = 1 },
		{ x = 0, y = -1 },
	}
end

function createCombatArea(cardinal, diagonal)
	local area = { cardinal = cardinal, diagonal = diagonal }
	table.insert(areas, area)
	return area
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
		local minimum, maximum = _G[self.callback](player, 999999, 999999)
		if not self.area then
			table.insert(state.events, "primary")
			table.insert(state.damage, { id = variant:getNumber(), minimum = minimum, maximum = maximum, secondary = false })
			return true
		end

		table.insert(state.events, "secondary")
		local targetPosition = state.target:getPosition()
		for _, offset in ipairs(secondaryOffsets(self.area, player:getPosition(), targetPosition)) do
			local candidate = state.tiles[positionKey(targetPosition.x + offset.x, targetPosition.y + offset.y, targetPosition.z)]
			if candidate and state.denied[candidate:getId()] ~= true then
				table.insert(state.damage, { id = candidate:getId(), minimum = minimum, maximum = maximum, secondary = true })
			end
		end
		return state.secondaryResult ~= false
	end
	table.insert(combats, combat)
	return combat
end

function Spell()
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
		spell[method] = function()
			return true
		end
	end
	spell.tag = function()
		return true
	end
	spell.register = function()
		return true
	end
	registration.spell = spell
	return spell
end

function ItemType(id)
	return itemTypes[id]
end

function Creature(id)
	return id == 1 and state.target or nil
end

local function position(x, y, z)
	local value = { x = x, y = y, z = z or 7 }
	function value:getDistance(other)
		return math.max(math.abs(self.x - other.x), math.abs(self.y - other.y), math.abs(self.z - other.z))
	end
	return value
end

local function creature(id, x, y)
	local creaturePosition = position(x, y, 7)
	return {
		getId = function()
			return id
		end,
		getPosition = function()
			return creaturePosition
		end,
	}
end

local function weapon(id, weaponType, attack)
	itemTypes[id] = {
		getWeaponType = function()
			return weaponType
		end,
		getAmmoType = function()
			return AMMO_NONE
		end,
		getShootRange = function()
			return 1
		end,
	}
	return {
		getId = function()
			return id
		end,
		getAttack = function()
			return attack
		end,
	}
end

local function reset(profile, targetX, targetY, attack)
	state = {
		events = {},
		damage = {},
		tiles = {},
		denied = {},
		secondaryResult = true,
	}
	state.target = creature(1, targetX or 1, targetY or 0)
	local weaponType = profile == "axe" and WEAPON_AXE or WEAPON_CLUB
	local equipped = weapon(profile == "axe" and 200 or 201, weaponType, attack or 30)
	local playerPosition = position(0, 0, 7)
	local player = {
		getId = function()
			return 42
		end,
		getStatPhysicalAttack = function()
			return 20
		end,
		getStatMagicalAttack = function()
			return 0
		end,
		getPosition = function()
			return playerPosition
		end,
		getSlotItem = function(_, slot)
			return slot == CONST_SLOT_LEFT and equipped or nil
		end,
	}
	local variant = { getNumber = function() return 1 end }
	return player, variant
end

local function place(id, x, y)
	local candidate = creature(id, x, y)
	state.tiles[positionKey(x, y, 7)] = candidate
	return candidate
end

local function execute(profile, targetX, targetY, attack)
	local player, variant = reset(profile, targetX, targetY, attack)
	return registration.spell.onCastSpell(player, variant)
end

dofile("data/scripts/spells/attack/armamento_assault.lua")

test("defines axe cardinal area with center excluded", function()
	assert_array({ 1, 2, 1 }, areas[1].cardinal[1])
	assert_equal(2, areas[1].cardinal[1][2])
end)

test("defines axe diagonal area with only perpendicular cells", function()
	assert_array({ 0, 0, 1 }, areas[1].diagonal[1])
	assert_array({ 0, 2, 0 }, areas[1].diagonal[2])
	assert_array({ 1, 0, 0 }, areas[1].diagonal[3])
end)

test("defines club orthogonal area with center excluded", function()
	assert_array({ 0, 1, 0 }, areas[2].cardinal[1])
	assert_array({ 1, 2, 1 }, areas[2].cardinal[2])
	assert_array({ 0, 1, 0 }, areas[2].cardinal[3])
end)

test("all area Combats keep legacy armor and disable shield blocking", function()
	local areaCombats = 0
	for _, combat in ipairs(combats) do
		if combat.area then
			areaCombats = areaCombats + 1
			assert_equal(COMBAT_PHYSICALDAMAGE, combat.parameters[COMBAT_PARAM_TYPE])
			assert_equal(1, combat.parameters[COMBAT_PARAM_BLOCKARMOR])
			assert_equal(0, combat.parameters[COMBAT_PARAM_BLOCKSHIELD])
		end
	end
	assert_equal(2, areaCombats)
end)

test("axe facing east hits north and south lateral positions", function()
	local player, variant = reset("axe", 1, 0)
	place(2, 1, 1)
	place(3, 1, -1)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_array({ 1, 2, 3 }, { state.damage[1].id, state.damage[2].id, state.damage[3].id })
end)

test("axe facing west hits south and north lateral positions", function()
	local player, variant = reset("axe", -1, 0)
	place(2, -1, -1)
	place(3, -1, 1)
	registration.spell.onCastSpell(player, variant)
	assert_array({ 1, 2, 3 }, { state.damage[1].id, state.damage[2].id, state.damage[3].id })
end)

test("axe facing north hits west and east lateral positions", function()
	local player, variant = reset("axe", 0, -1)
	place(2, 1, -1)
	place(3, -1, -1)
	registration.spell.onCastSpell(player, variant)
	assert_array({ 1, 2, 3 }, { state.damage[1].id, state.damage[2].id, state.damage[3].id })
end)

test("axe diagonal uses the two discrete perpendicular offsets", function()
	local player, variant = reset("axe", 1, 1)
	place(2, 0, 2)
	place(3, 2, 0)
	registration.spell.onCastSpell(player, variant)
	assert_array({ 1, 2, 3 }, { state.damage[1].id, state.damage[2].id, state.damage[3].id })
end)

test("club hits exactly the four orthogonal neighbors of impact", function()
	local player, variant = reset("club", 1, 0)
	place(2, 2, 0)
	place(3, 0, 0)
	place(4, 1, 1)
	place(5, 1, -1)
	registration.spell.onCastSpell(player, variant)
	assert_array({ 1, 2, 3, 4, 5 }, { state.damage[1].id, state.damage[2].id, state.damage[3].id, state.damage[4].id, state.damage[5].id })
end)

test("club keeps its impact-centered shape for a diagonal target", function()
	local player, variant = reset("club", 1, 1)
	place(2, 2, 1)
	place(3, 0, 1)
	place(4, 1, 2)
	place(5, 1, 0)
	registration.spell.onCastSpell(player, variant)
	assert_array({ 1, 2, 3, 4, 5 }, { state.damage[1].id, state.damage[2].id, state.damage[3].id, state.damage[4].id, state.damage[5].id })
end)

test("primary damage executes once before secondary damage", function()
	local player, variant = reset("axe", 1, 0)
	place(2, 1, 1)
	registration.spell.onCastSpell(player, variant)
	assert_array({ "primary", "secondary" }, state.events)
	assert_equal(1, state.damage[1].id)
	assert_equal(false, state.damage[1].secondary)
end)

test("secondary callback applies one floor after the 0.5 multiplier", function()
	local player, variant = reset("axe", 1, 0, 31)
	place(2, 1, 1)
	registration.spell.onCastSpell(player, variant)
	assert_equal(-61, state.damage[1].minimum)
	assert_equal(-30, state.damage[2].minimum)
	assert_equal(-30, state.damage[2].maximum)
end)

test("secondary area never hits the primary center again", function()
	local player, variant = reset("club", 1, 0)
	registration.spell.onCastSpell(player, variant)
	assert_equal(1, #state.damage)
	assert_equal(1, state.damage[1].id)
end)

test("empty secondary positions do not cancel the primary cast", function()
	assert_equal(true, execute("axe", 1, 0))
	assert_equal(1, #state.damage)
end)

test("a denied secondary target is skipped without canceling legal targets", function()
	local player, variant = reset("axe", 1, 0)
	place(2, 1, 1)
	place(3, 1, -1)
	state.denied[2] = true
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_array({ 1, 3 }, { state.damage[1].id, state.damage[2].id })
end)

test("secondary Combat denial cannot roll back completed primary damage", function()
	local player, variant = reset("axe", 1, 0)
	state.secondaryResult = false
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(1, #state.damage)
end)

test("axe and club reject non-adjacent primary targets before Combat", function()
	for _, profile in ipairs({ "axe", "club" }) do
		assert_equal(false, execute(profile, 2, 0))
		assert_equal(0, #state.damage)
	end
end)

print(string.format("\n%d passed, %d failed", passed, failed))
if #errors > 0 then
	for _, entry in ipairs(errors) do
		print(string.format("  FAIL: %s\n        %s", entry.name, entry.err))
	end
	os.exit(1)
end
