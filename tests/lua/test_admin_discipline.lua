-- Headless tests for data/scripts/talkactions/gm/discipline.lua.

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
		separator = function(_, value) registration.separator = value end,
		groupType = function(_, value) registration.group = value end,
		register = function() registration.registered = true end,
	}
	registration.action = action
	return action
end

MESSAGE_FAILURE = 21
MESSAGE_ADMINISTRATOR = 18

local state
local online
local function reset()
	state = { commandLogs = {}, audits = {} }
	online = {}
end

function logCommand(player, words, param)
	table.insert(state.commandLogs, { player:getName(), words, param })
end

logger = {
	info = function(format, actor, target, id, before, after, result)
		table.insert(state.audits, { format, actor, target, id, before, after, result })
	end,
}

function Player(name)
	return online[name]
end

dofile("data/scripts/talkactions/gm/discipline.lua")

local function actor(name)
	local messages = {}
	return {
		getName = function() return name or "Admin" end,
		sendTextMessage = function(_, kind, text) table.insert(messages, { kind, text }) end,
		messages = messages,
	}
end

local function target(name, initialRank, forcedResult)
	local messages = {}
	local rank = initialRank or 0
	local value = {
		getName = function() return name end,
		sendTextMessage = function(_, kind, text) table.insert(messages, { kind, text }) end,
		addDisciplineRank = function(_, id)
			if forcedResult then
				return forcedResult(id, rank)
			end
			local before = rank
			rank = rank + 1
			return true, before, rank, "success"
		end,
		removeDisciplineRank = function(_, id)
			if forcedResult then
				return forcedResult(id, rank)
			end
			local before = rank
			rank = rank - 1
			return true, before, rank, "success"
		end,
		messages = messages,
		rank = function() return rank end,
		profileAttribute = function(level) return rank * level end,
	}
	online[name] = value
	return value
end

local function invoke(param)
	local admin = actor()
	registration.action.onSay(admin, "!discipline", param)
	return admin
end

local function failure(result)
	return function(_, rank)
		return false, rank, rank, result
	end
end

test("registers only the gamemaster TalkAction boundary", function()
	assert_equal("!discipline", registration.words)
	assert_equal(" ", registration.separator)
	assert_equal("gamemaster", registration.group)
	assert_equal(true, registration.registered)
end)

test("add changes exactly one rank and the next profile calculation", function()
	reset()
	local affected = target("Hero", 2)
	local admin = invoke("add, Hero, 1")
	assert_equal(3, affected.rank())
	assert_equal(15, affected.profileAttribute(5))
	assert_equal(MESSAGE_ADMINISTRATOR, admin.messages[1][1])
end)

test("remove changes exactly one rank", function()
	reset()
	local affected = target("Hero", 2)
	invoke("remove, Hero, 1")
	assert_equal(1, affected.rank())
end)

test("success notifies both author and affected character", function()
	reset()
	local affected = target("Hero", 0)
	local admin = invoke("add, Hero, 1")
	assert_equal(1, #admin.messages)
	assert_equal(1, #affected.messages)
	assert_equal("Disciplina 1 atualizada: rank 1.", admin.messages[1][2])
	assert_equal(admin.messages[1][2], affected.messages[1][2])
end)

test("success notifies only once when author and target are the same character", function()
	reset()
	local god = target("GOD", 0)
	registration.action.onSay(god, "!discipline", "add, GOD, 1")
	assert_equal(1, god.rank())
	assert_equal(1, #god.messages)
	assert_equal(MESSAGE_ADMINISTRATOR, god.messages[1][1])
	assert_equal("Disciplina 1 atualizada: rank 1.", god.messages[1][2])
end)

test("rejects an unsupported operation without changing state", function()
	reset()
	local affected = target("Hero", 2)
	local admin = invoke("set, Hero, 1")
	assert_equal(2, affected.rank())
	assert_equal(MESSAGE_FAILURE, admin.messages[1][1])
	assert_equal("invalid_syntax", state.audits[1][7])
end)

test("rejects missing comma-separated segments", function()
	reset()
	local admin = invoke("add Hero 1")
	assert_equal(MESSAGE_FAILURE, admin.messages[1][1])
	assert_equal("-", state.audits[1][3])
end)

test("rejects extra comma-separated segments", function()
	reset()
	local admin = invoke("add, Hero, 1, extra")
	assert_equal(MESSAGE_FAILURE, admin.messages[1][1])
	assert_equal("invalid_syntax", state.audits[1][7])
end)

test("rejects a non-numeric id before resolving a player", function()
	reset()
	target("Hero", 2)
	local admin = invoke("add, Hero, nope")
	assert_equal(MESSAGE_FAILURE, admin.messages[1][1])
	assert_equal(2, online.Hero.rank())
	assert_equal("-", state.audits[1][4])
end)

test("requires the target to be online", function()
	reset()
	local admin = invoke("add, Offline Hero, 1")
	assert_equal("Personagem deve estar online.", admin.messages[1][2])
	assert_equal("target_offline", state.audits[1][7])
	assert_equal("Offline Hero", state.audits[1][3])
end)

test("unknown discipline preserves state and returns a clear error", function()
	reset()
	local affected = target("Hero", 2, failure("unknown_discipline"))
	local admin = invoke("add, Hero, 99")
	assert_equal(2, affected.rank())
	assert_equal("Disciplina inexistente.", admin.messages[1][2])
end)

test("absent removal preserves state and returns a clear error", function()
	reset()
	local affected = target("Hero", 0, failure("not_owned"))
	local admin = invoke("remove, Hero, 1")
	assert_equal(0, affected.rank())
	assert_equal("O personagem não possui essa disciplina.", admin.messages[1][2])
end)

test("rank limit preserves state and returns a clear error", function()
	reset()
	local affected = target("Hero", 2147483647, failure("rank_limit"))
	local admin = invoke("add, Hero, 1")
	assert_equal(2147483647, affected.rank())
	assert_equal("A disciplina atingiu a capacidade técnica de rank.", admin.messages[1][2])
end)

test("invalid numeric range from the binding is reported clearly", function()
	reset()
	target("Hero", 0, failure("invalid_id"))
	local admin = invoke("add, Hero, 65536")
	assert_equal("ID de disciplina inválido.", admin.messages[1][2])
end)

test("every attempt keeps the standard command log", function()
	reset()
	invoke("bad")
	assert_equal(1, #state.commandLogs)
	assert_equal("!discipline", state.commandLogs[1][2])
end)

test("successful audit records all required transition fields", function()
	reset()
	target("Hero", 4)
	invoke("add, Hero, 7")
	local audit = state.audits[1]
	assert_equal("Admin", audit[2])
	assert_equal("Hero", audit[3])
	assert_equal("7", audit[4])
	assert_equal(4, audit[5])
	assert_equal(5, audit[6])
	assert_equal("success", audit[7])
end)

test("parser trims a character name without changing internal spaces", function()
	reset()
	local affected = target("Hero Name", 0)
	invoke("add,   Hero Name  , 1")
	assert_equal(1, affected.rank())
	assert_equal("Hero Name", state.audits[1][3])
end)

print(string.format("\n%d passed, %d failed", passed, failed))
if #errors > 0 then
	for _, entry in ipairs(errors) do
		print(string.format("  FAIL: %s\n        %s", entry.name, entry.err))
	end
	os.exit(1)
end
