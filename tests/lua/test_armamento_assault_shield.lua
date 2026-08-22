-- Headless shield impact tests for data/scripts/spells/attack/armamento_assault.lua.

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
CONST_ME_NONE = 0
ORIGIN_SPELL = 2
CONST_ME_DRAWBLOOD = 10
CONST_ME_HITAREA = 11
CONST_ME_BLOCKHIT = 12
CONST_ANI_ARROW = 20
CONST_ANI_BOLT = 21

local registration = { profileTags = {} }
local state = {}

function Spell()
	local spell = {}
	for _, method in ipairs({
		"name",
		"words",
		"id",
		"needTarget",
		"isAggressive",
		"mana",
		"soul",
		"cooldown",
		"groupCooldown",
		"disciplineRequirement",
		"offensiveParameters",
		"baseTags",
	}) do
		spell[method] = function()
			return true
		end
	end
	spell.profileTags = function(_, profile, tags)
		registration.profileTags[profile] = tags
		return true
	end
	spell.createOffensiveContext = function()
		table.insert(state.events, "create")
		return state.context, "created"
	end
	spell.register = function()
		return true
	end
	registration.spell = spell
	return spell
end

function Position(x, y, z)
	return { x = x, y = y, z = z }
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

function doTargetCombatHealth(_, target, _, minimum, maximum, effect)
	table.insert(state.events, "damage")
	state.damage = { target = target, minimum = minimum, maximum = maximum, effect = effect }
	target.health = state.healthAfterDamage
	return true
end

function addEvent()
	state.scheduled = state.scheduled + 1
end

dofile("data/scripts/spells/attack/armamento_assault.lua")

local function reset(options)
	options = options or {}
	state = {
		events = {},
		tiles = {},
		scheduled = 0,
		healthAfterDamage = options.healthAfterDamage == nil and 40 or options.healthAfterDamage,
	}
	local casterPosition = Position(options.casterX or 10, options.casterY or 10, 7)
	local targetPosition = Position(options.targetX or 11, options.targetY or 10, 7)
	local player = {
		getPosition = function()
			return casterPosition
		end,
	}
	local target = {
		health = 100,
		position = targetPosition,
		getId = function()
			return 77
		end,
		getPosition = function(self)
			return self.position
		end,
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
			if options.moveSucceeds ~= false then
				self.position = tile.position
				return 0
			end
			return 1
		end,
	}
	state.target = target
	state.context = {
		getProfile = function()
			return "shield"
		end,
		getPrimaryBaseDamage = function()
			return 60
		end,
		validatePrimaryTarget = function()
			table.insert(state.events, "validate")
			return true, "created"
		end,
		commit = function()
			table.insert(state.events, "commit")
			return true, "created"
		end,
	}
	local variant = {
		getNumber = function()
			return 77
		end,
	}
	return player, target, variant
end

local function destination(x, y)
	local tile = { position = Position(x, y, 7) }
	state.tiles[positionKey(tile.position)] = tile
	return tile
end

local function execute(options)
	local player, target, variant = reset(options)
	return registration.spell.onCastSpell(player, variant), target
end

test("shield applies blockhit damage before one successful opposite move", function()
	local player, target, variant = reset()
	local tile = destination(12, 10)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(CONST_ME_BLOCKHIT, state.damage.effect)
	assert_equal(-60, state.damage.minimum)
	assert_equal(-60, state.damage.maximum)
	assert_equal(tile, state.move.tile)
	assert_equal(0, state.move.flags)
	assert_equal(tile.position, target.position)
	assert_array({ "create", "validate", "commit", "damage", "removed-check", "health-check", "tile:12,10,7", "move" }, state.events)
end)

test("blocked shield destination preserves completed damage and target position", function()
	local player, target, variant = reset({ moveSucceeds = false })
	local original = target.position
	destination(12, 10)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(target, state.damage.target)
	assert_equal(original, target.position)
	assert_equal(0, state.move.flags)
end)

test("missing shield destination preserves damage without attempting movement", function()
	local result, target = execute()
	assert_equal(true, result)
	assert_equal(target, state.damage.target)
	assert_equal(nil, state.move)
	assert_equal("tile:12,10,7", state.events[#state.events])
end)

test("shield skips knockback when damage leaves the target dead", function()
	local player, target, variant = reset({ healthAfterDamage = 0 })
	destination(12, 10)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(target, state.damage.target)
	assert_equal(nil, state.move)
	assert_array({ "create", "validate", "commit", "damage", "removed-check", "health-check" }, state.events)
end)

test("shield skips health and movement checks for a removed target", function()
	local player, target, variant = reset({ removed = true })
	destination(12, 10)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(target, state.damage.target)
	assert_equal(nil, state.move)
	assert_array({ "create", "validate", "commit", "damage", "removed-check" }, state.events)
end)

test("shield profile retains the knockback capability tag", function()
	assert_array({ "equipment.shield", "execution.contact", "function.control", "mechanic.knockback" }, registration.profileTags.shield)
end)

test("shield impact remains synchronous with exactly one move and no delayed work", function()
	local player, _, variant = reset({ casterX = 10, casterY = 10, targetX = 9, targetY = 9 })
	destination(8, 8)
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	local moves = 0
	for _, event in ipairs(state.events) do
		if event == "move" then
			moves = moves + 1
		end
	end
	assert_equal(1, moves)
	assert_equal(0, state.scheduled)
	assert_equal("damage", state.events[4])
	assert_equal("move", state.events[#state.events])
end)

print(string.format("\n%d passed, %d failed", passed, failed))
if #errors > 0 then
	for _, entry in ipairs(errors) do
		print(string.format("  FAIL: %s\n        %s", entry.name, entry.err))
	end
	os.exit(1)
end
