-- Headless tests for data/scripts/talkactions/player/profile.lua.

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

local registration = {}
function TalkAction(words)
	registration.words = words
	local action = {
		groupType = function(_, group)
			registration.group = group
		end,
		register = function()
			registration.registered = true
		end,
	}
	registration.action = action
	return action
end

dofile("data/scripts/talkactions/player/profile.lua")

local function playerWith(profile)
	local state = { popup = nil, profileCalls = 0 }
	local player = {
		getDisciplineProfile = function()
			state.profileCalls = state.profileCalls + 1
			return profile
		end,
		popupFYI = function(_, text)
			state.popup = text
		end,
	}
	return player, state
end

local emptyProfile = {
	attributes = { ["for"] = 0, des = 0, vit = 0, int = 0, von = 0 },
	disciplines = {},
}

test("registers !profile for normal players", function()
	assert_equal("!profile", registration.words)
	assert_equal("normal", registration.group)
	assert_equal(true, registration.registered)
end)

test("shows all five zero attributes and the empty discipline message", function()
	local player, state = playerWith(emptyProfile)
	assert_equal(true, registration.action.onSay(player, "!profile", ""))
	assert_equal("Atributos\nFOR: 0\nDES: 0\nVIT: 0\nINT: 0\nVON: 0\n\nDisciplinas\nNenhuma disciplina adquirida.", state.popup)
end)

test("shows owned disciplines with id name and rank", function()
	local player, state = playerWith({
		attributes = { ["for"] = 12, des = 9, vit = 6, int = 0, von = 3 },
		disciplines = { { id = 1, name = "Armamento", rank = 2 } },
	})
	registration.action.onSay(player, "!profile", "")
	assert_equal("Atributos\nFOR: 12\nDES: 9\nVIT: 6\nINT: 0\nVON: 3\n\nDisciplinas\n[1] Armamento — Rank 2", state.popup)
end)

test("preserves the ascending order supplied by the profile API", function()
	local player, state = playerWith({
		attributes = { ["for"] = 1, des = 2, vit = 3, int = 4, von = 5 },
		disciplines = {
			{ id = 1, name = "Armamento", rank = 2 },
			{ id = 7, name = "Defesa", rank = 1 },
		},
	})
	registration.action.onSay(player, "!profile", "")
	local first = assert(state.popup:find("%[1%] Armamento", 1))
	local second = assert(state.popup:find("%[7%] Defesa", 1))
	if first >= second then
		error("discipline rows are not ordered by id")
	end
end)

test("reads only the executing player and ignores parameters", function()
	Player = function()
		error("!profile must never resolve another character")
	end
	local player, state = playerWith(emptyProfile)
	registration.action.onSay(player, "!profile", "Outro Personagem")
	assert_equal(1, state.profileCalls)
	assert_equal(true, state.popup ~= nil)
end)

print(string.format("\n%d passed, %d failed", passed, failed))
if #errors > 0 then
	for _, entry in ipairs(errors) do
		print(string.format("  FAIL: %s\n        %s", entry.name, entry.err))
	end
	os.exit(1)
end
