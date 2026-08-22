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
CONST_ME_NONE = 0
ORIGIN_SPELL = 2
CONST_ME_DRAWBLOOD = 10
CONST_ME_HITAREA = 11
CONST_ANI_ARROW = 20
CONST_ANI_BOLT = 21

local registration = { profileTags = {} }
local cast = {}
local targets = {}

function Spell(kind)
	registration.kind = kind
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
		spell[method] = function(_, ...)
			registration[method] = { ... }
			return true
		end
	end
	spell.profileTags = function(_, ...)
		table.insert(registration.profileTags, { ... })
		return true
	end
	spell.createOffensiveContext = function(_, player)
		table.insert(cast.events, "create")
		cast.createdFor = player
		return cast.context, cast.contextReason or "created"
	end
	spell.register = function()
		registration.registered = true
		return true
	end
	registration.spell = spell
	return spell
end

function Creature(id)
	cast.resolvedTargetId = id
	return targets[id]
end

function doTargetCombatHealth(...)
	local arguments = { ... }
	table.insert(cast.events, "combat")
	table.insert(cast.damageCalls, arguments)
	return cast.combatResult
end

dofile("data/scripts/spells/attack/armamento_assault.lua")

local function resetCast(options)
	options = options or {}
	cast = {
		events = {},
		damageCalls = {},
		combatResult = options.combatResult,
		contextReason = options.contextReason,
	}
	targets = {}
	local target = { id = 77 }
	targets[77] = target
	local context = {
		getProfile = function()
			return "sword"
		end,
		validatePrimaryTarget = function(_, receivedTarget)
			table.insert(cast.events, "validate")
			cast.validatedTarget = receivedTarget
			return options.valid ~= false, options.validationReason or "created"
		end,
		commit = function(_, receivedTarget)
			table.insert(cast.events, "commit")
			cast.committedTarget = receivedTarget
			return options.committed ~= false, options.commitReason or "created"
		end,
		getPrimaryBaseDamage = function()
			table.insert(cast.events, "damage")
			return options.baseDamage or 60
		end,
	}
	if options.noContext ~= true then
		cast.context = context
	end
	return { id = 42 }, target, {
		getNumber = function()
			return options.targetId == nil and 77 or options.targetId
		end,
	}
end

test("registers Assault as an instant spell with stable identity", function()
	assert_equal("instant", registration.kind)
	assert_array({ "Assault" }, registration.name)
	assert_array({ "assault" }, registration.words)
	assert_array({ 298 }, registration.id)
	assert_equal(true, registration.registered)
end)

test("declares a required hostile target and aggressive behavior", function()
	assert_array({ true }, registration.needTarget)
	assert_array({ true }, registration.isAggressive)
end)

test("requires Armamento id 1 at rank 1", function()
	assert_array({ 1, 1 }, registration.disciplineRequirement)
end)

test("declares all six normative offensive parameters exactly", function()
	local parameters = registration.offensiveParameters[1]
	assert_equal(10, parameters.basePower)
	assert_equal(1.0, parameters.physicalCoefficient)
	assert_equal(0.0, parameters.magicalCoefficient)
	assert_equal(1.0, parameters.equipmentCoefficient)
	assert_equal(0.5, parameters.secondaryMultiplier)
	assert_equal(1000, parameters.cooldownMilliseconds)
end)

test("declares the six normative base tags exactly", function()
	assert_array({
		"category.art",
		"damage.neutral",
		"damage.physical",
		"discipline.armament",
		"execution.attack",
		"function.offensive",
	}, registration.baseTags[1])
end)

test("declares every normative profile tag set", function()
	local expected = {
		{ "sword", "execution.contact", "weapon.sword" },
		{ "axe", "execution.area", "execution.contact", "weapon.axe" },
		{ "club", "execution.area", "execution.contact", "weapon.club" },
		{ "bow", "execution.projectile", "weapon.bow" },
		{ "crossbow", "execution.projectile", "weapon.crossbow" },
		{ "shield", "equipment.shield", "execution.contact", "function.control", "mechanic.knockback" },
	}
	assert_equal(#expected, #registration.profileTags)
	for index, values in ipairs(expected) do
		local actual = registration.profileTags[index]
		assert_equal(values[1], actual[1])
		assert_array({ unpack(values, 2) }, actual[2], values[1])
	end
end)

test("declares zero mana and zero soul", function()
	assert_array({ 0 }, registration.mana)
	assert_array({ 0 }, registration.soul)
end)

test("declares only the individual cooldown and a zero group cooldown", function()
	assert_array({ 1000 }, registration.cooldown)
	assert_array({ 0 }, registration.groupCooldown)
	assert_equal(nil, registration.group)
end)

test("does not declare legacy progression or learning gates", function()
	for _, forbidden in ipairs({ "vocation", "level", "magicLevel", "needLearn", "needWeapon", "premium" }) do
		assert_equal(nil, registration[forbidden], forbidden)
	end
end)

test("rejects a missing target before creating a context", function()
	local player, _, variant = resetCast({ targetId = 0 })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(0, #cast.events)
	assert_equal(0, #cast.damageCalls)
end)

test("rejects an unresolved target before creating a context", function()
	local player, _, variant = resetCast({ targetId = 99 })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(99, cast.resolvedTargetId)
	assert_equal(0, #cast.events)
end)

test("rejects unsupported equipment when context creation fails", function()
	local player, _, variant = resetCast({ noContext = true, contextReason = "unsupported_equipment" })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_array({ "create" }, cast.events)
	assert_equal(0, #cast.damageCalls)
end)

test("rejects missing ammunition when context creation fails", function()
	local player, _, variant = resetCast({ noContext = true, contextReason = "missing_ammunition" })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_array({ "create" }, cast.events)
	assert_equal(0, #cast.damageCalls)
end)

test("rejects target validation before commit and damage", function()
	local player, target, variant = resetCast({ valid = false, validationReason = "out_of_range" })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(target, cast.validatedTarget)
	assert_array({ "create", "validate" }, cast.events)
	assert_equal(0, #cast.damageCalls)
end)

test("preserves combat denial as a pre-commit failure", function()
	local player, _, variant = resetCast({ valid = false, validationReason = "combat_denied" })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_array({ "create", "validate" }, cast.events)
	assert_equal(nil, cast.committedTarget)
end)

test("rejects a commit-time ammunition race without damage", function()
	local player, target, variant = resetCast({ committed = false, commitReason = "missing_ammunition" })
	assert_equal(false, registration.spell.onCastSpell(player, variant))
	assert_equal(target, cast.committedTarget)
	assert_array({ "create", "validate", "commit" }, cast.events)
	assert_equal(0, #cast.damageCalls)
end)

test("executes the valid transaction once in strict order", function()
	local player, _, variant = resetCast()
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_array({ "create", "validate", "commit", "damage", "combat" }, cast.events)
	assert_equal(1, #cast.damageCalls)
end)

test("submits exact deterministic neutral physical base damage", function()
	local player, target, variant = resetCast({ baseDamage = 60 })
	registration.spell.onCastSpell(player, variant)
	local call = cast.damageCalls[1]
	assert_equal(player, call[1])
	assert_equal(target, call[2])
	assert_equal(COMBAT_PHYSICALDAMAGE, call[3])
	assert_equal(-60, call[4])
	assert_equal(-60, call[5])
end)

test("submits through the spell origin with the sword impact effect", function()
	local player, _, variant = resetCast()
	registration.spell.onCastSpell(player, variant)
	local call = cast.damageCalls[1]
	assert_equal(CONST_ME_DRAWBLOOD, call[6])
	assert_equal(ORIGIN_SPELL, call[7])
	assert_equal(nil, call[8])
	assert_equal("Assault", call[9])
end)

test("submits zero base damage as a valid committed cast", function()
	local player, _, variant = resetCast({ baseDamage = 0 })
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(0, cast.damageCalls[1][4])
	assert_equal(0, cast.damageCalls[1][5])
end)

test("keeps the cast successful after commit even when Combat reports no damage", function()
	local player, _, variant = resetCast({ combatResult = false })
	assert_equal(true, registration.spell.onCastSpell(player, variant))
	assert_equal(1, #cast.damageCalls)
end)

test("creates one immutable context for the executing player", function()
	local player, _, variant = resetCast()
	registration.spell.onCastSpell(player, variant)
	assert_equal(player, cast.createdFor)
	local creates = 0
	for _, event in ipairs(cast.events) do
		if event == "create" then
			creates = creates + 1
		end
	end
	assert_equal(1, creates)
end)

print(string.format("\n%d passed, %d failed", passed, failed))
if #errors > 0 then
	for _, entry in ipairs(errors) do
		print(string.format("  FAIL: %s\n        %s", entry.name, entry.err))
	end
	os.exit(1)
end
