-- Headless registration-resource tests for data/scripts/spells/attack/armamento_assault.lua.

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

local function loadWithout(missingResource)
	local state = { registrations = 0, logs = {} }
	local environment = {
		ipairs = ipairs,
		pairs = pairs,
		table = table,
		type = type,
		COMBAT_PHYSICALDAMAGE = 1,
		ORIGIN_SPELL = 2,
		CONST_ME_DRAWBLOOD = 10,
		CONST_ME_HITAREA = 11,
		CONST_ME_BLOCKHIT = 12,
		CONST_ANI_ARROW = 20,
		CONST_ANI_BOLT = 21,
		logger = {
			error = function(message, resource)
				table.insert(state.logs, message .. "|" .. tostring(resource))
			end,
		},
	}
	environment[missingResource] = nil
	environment.Spell = function()
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
		spell.register = function()
			state.registrations = state.registrations + 1
			return true
		end
		return spell
	end

	local chunk = assert(loadfile("data/scripts/spells/attack/armamento_assault.lua"))
	setfenv(chunk, environment)
	state.result = chunk()
	return state
end

test("missing impact resource rejects registration and identifies its constant", function()
	local state = loadWithout("CONST_ME_DRAWBLOOD")
	assert_equal(false, state.result)
	assert_equal(0, state.registrations)
	assert_equal("[ArmamentoAssault] Missing visual resource: {}|CONST_ME_DRAWBLOOD", state.logs[1])
end)

test("missing projectile resource rejects registration and identifies its constant", function()
	local state = loadWithout("CONST_ANI_BOLT")
	assert_equal(false, state.result)
	assert_equal(0, state.registrations)
	assert_equal("[ArmamentoAssault] Missing visual resource: {}|CONST_ANI_BOLT", state.logs[1])
end)

test("missing shield impact resource rejects registration and identifies its constant", function()
	local state = loadWithout("CONST_ME_BLOCKHIT")
	assert_equal(false, state.result)
	assert_equal(0, state.registrations)
	assert_equal("[ArmamentoAssault] Missing visual resource: {}|CONST_ME_BLOCKHIT", state.logs[1])
end)

print(string.format("\n%d passed, %d failed", passed, failed))
if #errors > 0 then
	for _, entry in ipairs(errors) do
		print(string.format("  FAIL: %s\n        %s", entry.name, entry.err))
	end
	os.exit(1)
end
