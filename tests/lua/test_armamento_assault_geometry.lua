-- Headless geometry/effect tests for data/scripts/spells/attack/armamento_assault.lua.

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
CONST_ANI_ARROW = 20
CONST_ANI_BOLT = 21

local registration = {}
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
		"profileTags",
	}) do
		spell[method] = function()
			return true
		end
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
	table.insert(state.tileLookups, positionKey(position))
	return state.tiles[positionKey(position)]
end

function Creature(id)
	return state.creatures[id]
end

function doTargetCombatHealth(_, target, combatType, minimum, maximum, effect, origin, _, spellName)
	table.insert(state.events, "damage:" .. target:getId())
	table.insert(state.damage, {
		target = target,
		combatType = combatType,
		minimum = minimum,
		maximum = maximum,
		effect = effect,
		origin = origin,
		spellName = spellName,
	})
	return true
end

dofile("data/scripts/spells/attack/armamento_assault.lua")

local function creature(id, x, y, z)
	local position = Position(x, y, z or 7)
	return {
		getId = function()
			return id
		end,
		getPosition = function()
			return position
		end,
	}
end

local function reset(profile, targetX, targetY)
	state = {
		events = {},
		tileLookups = {},
		tiles = {},
		creatures = {},
		damage = {},
		affect = {},
	}
	local casterPosition = Position(0, 0, 7)
	local player = {
		getPosition = function()
			return setmetatable(casterPosition, {
				__index = {
					sendDistanceEffect = function(_, destination, effect)
						table.insert(state.events, "distance:" .. effect)
						state.distance = { destination = destination, effect = effect }
					end,
				},
			})
		end,
	}
	local primary = creature(1, targetX or 1, targetY or 0)
	state.creatures[1] = primary
	state.context = {
		getProfile = function()
			return profile
		end,
		getPrimaryBaseDamage = function()
			return 60
		end,
		getSecondaryBaseDamage = function()
			return 30
		end,
		validatePrimaryTarget = function()
			table.insert(state.events, "validate")
			return true, "created"
		end,
		canAffect = function(_, candidate)
			table.insert(state.events, "affect:" .. candidate:getId())
			return state.affect[candidate:getId()] ~= false
		end,
		commit = function()
			table.insert(state.events, "commit")
			state.commitCount = (state.commitCount or 0) + 1
			return true, "created"
		end,
	}
	local variant = {
		getNumber = function()
			return 1
		end,
	}
	return player, primary, variant
end

local function place(candidate, x, y)
	state.creatures[candidate:getId()] = candidate
	state.tiles[positionKey(Position(x, y, 7))] = {
		getTopCreature = function()
			return candidate
		end,
	}
end

local function emptyTile(x, y)
	state.tiles[positionKey(Position(x, y, 7))] = {
		getTopCreature = function()
			return nil
		end,
	}
end

local function cast(profile, targetX, targetY)
	local player, primary, variant = reset(profile, targetX, targetY)
	return player, primary, variant, function()
		return registration.spell.onCastSpell(player, variant)
	end
end

test("sword selects only the primary and emits drawblood", function()
	local _, primary, _, execute = cast("sword", 1, 0)
	assert_equal(true, execute())
	assert_equal(1, #state.damage)
	assert_equal(primary, state.damage[1].target)
	assert_equal(CONST_ME_DRAWBLOOD, state.damage[1].effect)
	assert_equal(0, #state.tileLookups)
end)

test("axe facing east adds north then south lateral tiles", function()
	local _, _, _, execute = cast("axe", 1, 0)
	place(creature(2, 1, 1), 1, 1)
	place(creature(3, 1, -1), 1, -1)
	assert_equal(true, execute())
	assert_array({ 1, 2, 3 }, { state.damage[1].target:getId(), state.damage[2].target:getId(), state.damage[3].target:getId() })
	assert_array({ "1,1,7", "1,-1,7" }, state.tileLookups)
end)

test("axe facing west rotates lateral order with direction", function()
	local _, _, _, execute = cast("axe", -1, 0)
	place(creature(2, -1, -1), -1, -1)
	place(creature(3, -1, 1), -1, 1)
	execute()
	assert_array({ "-1,-1,7", "-1,1,7" }, state.tileLookups)
	assert_array({ 1, 2, 3 }, { state.damage[1].target:getId(), state.damage[2].target:getId(), state.damage[3].target:getId() })
end)

test("axe diagonal rotates the two discrete perpendicular offsets", function()
	local _, _, _, execute = cast("axe", 1, 1)
	place(creature(2, 0, 2), 0, 2)
	place(creature(3, 2, 0), 2, 0)
	execute()
	assert_array({ "0,2,7", "2,0,7" }, state.tileLookups)
end)

test("club selects east west south north around impact in order", function()
	local _, _, _, execute = cast("club", 1, 0)
	place(creature(2, 2, 0), 2, 0)
	place(creature(3, 0, 0), 0, 0)
	place(creature(4, 1, 1), 1, 1)
	place(creature(5, 1, -1), 1, -1)
	execute()
	assert_array({ "2,0,7", "0,0,7", "1,1,7", "1,-1,7" }, state.tileLookups)
	assert_array({ 1, 2, 3, 4, 5 }, { state.damage[1].target:getId(), state.damage[2].target:getId(), state.damage[3].target:getId(), state.damage[4].target:getId(), state.damage[5].target:getId() })
end)

test("club keeps its impact-centered shape on a diagonal primary", function()
	local _, _, _, execute = cast("club", 1, 1)
	place(creature(2, 2, 1), 2, 1)
	place(creature(3, 0, 1), 0, 1)
	place(creature(4, 1, 2), 1, 2)
	place(creature(5, 1, 0), 1, 0)
	execute()
	assert_array({ "2,1,7", "0,1,7", "1,2,7", "1,0,7" }, state.tileLookups)
end)

test("bow selects only primary and emits arrow immediately before damage", function()
	local _, _, _, execute = cast("bow", 4, 0)
	execute()
	assert_equal(1, #state.damage)
	assert_equal(CONST_ME_HITAREA, state.damage[1].effect)
	assert_array({ "create", "validate", "commit", "distance:" .. CONST_ANI_ARROW, "damage:1" }, state.events)
	assert_equal(CONST_ANI_ARROW, state.distance.effect)
end)

test("crossbow selects only primary and emits bolt immediately before damage", function()
	local _, _, _, execute = cast("crossbow", 5, 0)
	execute()
	assert_equal(1, #state.damage)
	assert_array({ "create", "validate", "commit", "distance:" .. CONST_ANI_BOLT, "damage:1" }, state.events)
	assert_equal(CONST_ANI_BOLT, state.distance.effect)
end)

test("secondary targets use the frozen secondary damage exactly once", function()
	local _, _, _, execute = cast("axe", 1, 0)
	place(creature(2, 1, 1), 1, 1)
	emptyTile(1, -1)
	execute()
	assert_equal(-60, state.damage[1].minimum)
	assert_equal(-60, state.damage[1].maximum)
	assert_equal(-30, state.damage[2].minimum)
	assert_equal(-30, state.damage[2].maximum)
end)

test("primary is processed before every secondary", function()
	local _, _, _, execute = cast("axe", 1, 0)
	place(creature(2, 1, 1), 1, 1)
	place(creature(3, 1, -1), 1, -1)
	execute()
	assert_array({ "damage:1", "damage:2", "damage:3" }, { state.events[#state.events - 2], state.events[#state.events - 1], state.events[#state.events] })
end)

test("a secondary tile resolving to primary is deduplicated", function()
	local _, primary, _, execute = cast("axe", 1, 0)
	place(primary, 1, 1)
	emptyTile(1, -1)
	execute()
	assert_equal(1, #state.damage)
	assert_equal(1, state.damage[1].target:getId())
end)

test("two secondary positions resolving to one creature are deduplicated", function()
	local _, _, _, execute = cast("axe", 1, 0)
	local shared = creature(2, 1, 1)
	place(shared, 1, 1)
	place(shared, 1, -1)
	execute()
	assert_equal(2, #state.damage)
	assert_equal(2, state.damage[2].target:getId())
end)

test("an illegal secondary is skipped without canceling legal targets", function()
	local _, _, _, execute = cast("axe", 1, 0)
	place(creature(2, 1, 1), 1, 1)
	place(creature(3, 1, -1), 1, -1)
	state.affect[2] = false
	assert_equal(true, execute())
	assert_array({ 1, 3 }, { state.damage[1].target:getId(), state.damage[2].target:getId() })
end)

test("a missing map tile is ignored without canceling the cast", function()
	local _, _, _, execute = cast("axe", 1, 0)
	place(creature(3, 1, -1), 1, -1)
	assert_equal(true, execute())
	assert_array({ 1, 3 }, { state.damage[1].target:getId(), state.damage[2].target:getId() })
end)

test("an empty tile is ignored without canceling the cast", function()
	local _, _, _, execute = cast("axe", 1, 0)
	emptyTile(1, 1)
	place(creature(3, 1, -1), 1, -1)
	assert_equal(true, execute())
	assert_array({ 1, 3 }, { state.damage[1].target:getId(), state.damage[2].target:getId() })
end)

test("selection finishes before the single synchronous commit and all damage", function()
	local _, _, _, execute = cast("axe", 1, 0)
	place(creature(2, 1, 1), 1, 1)
	place(creature(3, 1, -1), 1, -1)
	execute()
	assert_equal(1, state.commitCount)
	assert_array({ "create", "validate", "affect:2", "affect:3", "commit", "damage:1", "damage:2", "damage:3" }, state.events)
end)

print(string.format("\n%d passed, %d failed", passed, failed))
if #errors > 0 then
	for _, entry in ipairs(errors) do
		print(string.format("  FAIL: %s\n        %s", entry.name, entry.err))
	end
	os.exit(1)
end
