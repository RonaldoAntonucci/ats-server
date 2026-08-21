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
	local state = { popup = nil, attributesCalls = 0, statsCalls = 0, disciplinesCalls = 0 }
	local player = {
		getAttributes = function()
			state.attributesCalls = state.attributesCalls + 1
			return profile.attributes
		end,
		getStats = function()
			state.statsCalls = state.statsCalls + 1
			return profile.stats
		end,
		getDisciplines = function()
			state.disciplinesCalls = state.disciplinesCalls + 1
			return profile.disciplines
		end,
		popupFYI = function(_, text)
			state.popup = text
		end,
	}
	return player, state
end

local emptyProfile = {
	attributes = { pot = 0, tec = 0, vig = 0, sin = 0, esp = 0 },
	stats = {
		physicalAttack = 0,
		magicalAttack = 0,
		precision = 0,
		physicalDefense = 0,
		magicalDefense = 0,
		maximumHealth = 0,
		maximumMana = 0,
	},
	disciplines = {},
}

local emptyText = "Atributos\nPOT: 0\nTEC: 0\nVIG: 0\nSIN: 0\nESP: 0\n\nStatus\nAtaque Físico: 0\nAtaque Mágico: 0\nPrecisão: 0\nDefesa Física: 0\nDefesa Mágica: 0\nVida Máxima: 0\nMana Máxima: 0\n\nDisciplinas\nNenhuma disciplina adquirida."

local normativeProfile = {
	attributes = { pot = 10, tec = 10, vig = 10, sin = 0, esp = 0 },
	stats = {
		physicalAttack = 10,
		magicalAttack = 0,
		precision = 10,
		physicalDefense = 10,
		magicalDefense = 0,
		maximumHealth = 50,
		maximumMana = 0,
	},
	disciplines = {},
}

test("registers !profile for normal players", function()
	assert_equal("!profile", registration.words)
	assert_equal("normal", registration.group)
	assert_equal(true, registration.registered)
end)

test("shows every zero attribute and status plus the empty discipline message", function()
	local player, state = playerWith(emptyProfile)
	assert_equal(true, registration.action.onSay(player, "!profile", ""))
	assert_equal(emptyText, state.popup)
end)

test("renders the normative attribute and derived-stat example", function()
	local player, state = playerWith(normativeProfile)
	registration.action.onSay(player, "!profile", "")
	assert_equal("Atributos\nPOT: 10\nTEC: 10\nVIG: 10\nSIN: 0\nESP: 0\n\nStatus\nAtaque Físico: 10\nAtaque Mágico: 0\nPrecisão: 10\nDefesa Física: 10\nDefesa Mágica: 0\nVida Máxima: 50\nMana Máxima: 0\n\nDisciplinas\nNenhuma disciplina adquirida.", state.popup)
end)

test("shows owned disciplines with id name and rank", function()
	local player, state = playerWith({
		attributes = { pot = 12, tec = 9, vig = 6, sin = 0, esp = 3 },
		stats = {
			physicalAttack = 12,
			magicalAttack = 0,
			precision = 9,
			physicalDefense = 8,
			magicalDefense = 3,
			maximumHealth = 30,
			maximumMana = 15,
		},
		disciplines = { { id = 1, name = "Armamento", rank = 2 } },
	})
	registration.action.onSay(player, "!profile", "")
	assert_equal("Atributos\nPOT: 12\nTEC: 9\nVIG: 6\nSIN: 0\nESP: 3\n\nStatus\nAtaque Físico: 12\nAtaque Mágico: 0\nPrecisão: 9\nDefesa Física: 8\nDefesa Mágica: 3\nVida Máxima: 30\nMana Máxima: 15\n\nDisciplinas\n[1] Armamento — Rank 2", state.popup)
end)

test("preserves the ascending order supplied by the profile API", function()
	local player, state = playerWith({
		attributes = emptyProfile.attributes,
		stats = emptyProfile.stats,
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

test("keeps the three sections in their public order", function()
	local player, state = playerWith(normativeProfile)
	registration.action.onSay(player, "!profile", "")
	local attributes = assert(state.popup:find("Atributos", 1, true))
	local stats = assert(state.popup:find("Status", 1, true))
	local disciplines = assert(state.popup:find("Disciplinas", 1, true))
	if not (attributes < stats and stats < disciplines) then
		error("profile sections are out of order")
	end
end)

test("keeps every attribute and status in its fixed presentation order", function()
	local player, state = playerWith(normativeProfile)
	registration.action.onSay(player, "!profile", "")
	local labels = {
		"POT:",
		"TEC:",
		"VIG:",
		"SIN:",
		"ESP:",
		"Ataque Físico:",
		"Ataque Mágico:",
		"Precisão:",
		"Defesa Física:",
		"Defesa Mágica:",
		"Vida Máxima:",
		"Mana Máxima:",
	}
	local previous = 0
	for _, label in ipairs(labels) do
		local position = assert(state.popup:find(label, previous + 1, true), label)
		if position <= previous then
			error(label .. " is out of order")
		end
		previous = position
	end
end)

test("reads only the executing player and ignores parameters", function()
	Player = function()
		error("!profile must never resolve another character")
	end
	local player, state = playerWith(emptyProfile)
	registration.action.onSay(player, "!profile", "Outro Personagem")
	assert_equal(1, state.attributesCalls)
	assert_equal(1, state.statsCalls)
	assert_equal(1, state.disciplinesCalls)
	assert_equal(emptyText, state.popup)
end)

print(string.format("\n%d passed, %d failed", passed, failed))
if #errors > 0 then
	for _, entry in ipairs(errors) do
		print(string.format("  FAIL: %s\n        %s", entry.name, entry.err))
	end
	os.exit(1)
end
