-- Headless command UAT using the production TalkAction scripts together.

local actions = {}
function TalkAction(words)
	local action = {
		groupType = function(self, value)
			self.group = value
		end,
		separator = function(self, value)
			self.separatorValue = value
		end,
		register = function(self)
			self.registered = true
		end,
	}
	actions[words] = action
	return action
end

MESSAGE_FAILURE = 21
MESSAGE_ADMINISTRATOR = 18
logger = { info = function() end }
function logCommand() end

local rank = 0
local level = 5
local heroMessages = {}
local hero = {
	getName = function()
		return "Hero"
	end,
	addDisciplineRank = function()
		local before = rank
		rank = rank + 1
		return true, before, rank, "success"
	end,
	removeDisciplineRank = function()
		local before = rank
		rank = rank - 1
		return true, before, rank, "success"
	end,
	getDisciplineProfile = function()
		local attributes = level * rank
		return {
			attributes = { ["for"] = attributes, des = attributes, vit = attributes, int = 0, von = 0 },
			disciplines = rank == 0 and {} or { { id = 1, name = "Armamento", rank = rank } },
		}
	end,
	sendTextMessage = function(_, kind, text)
		table.insert(heroMessages, { kind, text })
	end,
	popupFYI = function(self, text)
		self.popup = text
	end,
}

function Player(name)
	if name == "Hero" then
		return hero
	end
	return nil
end

local admin = {
	getName = function()
		return "Admin"
	end,
	sendTextMessage = function() end,
}

dofile("data/scripts/talkactions/player/profile.lua")
dofile("data/scripts/talkactions/gm/discipline.lua")

assert(actions["!profile"].registered and actions["!profile"].group == "normal")
assert(actions["!discipline"].registered and actions["!discipline"].group == "gamemaster")

assert(actions["!discipline"].onSay(admin, "!discipline", "add, Hero, 1"))
assert(rank == 1)
assert(actions["!profile"].onSay(hero, "!profile", ""))
assert(hero.popup == "Atributos\nFOR: 5\nDES: 5\nVIT: 5\nINT: 0\nVON: 0\n\nDisciplinas\n[1] Armamento — Rank 1")

assert(actions["!discipline"].onSay(admin, "!discipline", "remove, Hero, 1"))
assert(rank == 0)
assert(actions["!profile"].onSay(hero, "!profile", ""))
assert(hero.popup == "Atributos\nFOR: 0\nDES: 0\nVIT: 0\nINT: 0\nVON: 0\n\nDisciplinas\nNenhuma disciplina adquirida.")

print("\n1 passed, 0 failed")
