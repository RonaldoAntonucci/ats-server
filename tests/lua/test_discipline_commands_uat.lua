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
	getAttributes = function()
		local attributes = level * rank
		return { pot = attributes, tec = attributes, vig = attributes, sin = 0, esp = 0 }
	end,
	getStats = function()
		local attributes = level * rank
		return {
			physicalAttack = attributes,
			magicalAttack = 0,
			precision = attributes,
			physicalDefense = attributes,
			magicalDefense = 0,
			maximumHealth = attributes * 5,
			maximumMana = 0,
		}
	end,
	getDisciplines = function()
		return rank == 0 and {} or {
			{
				id = 1,
				name = "Armamento",
				rank = rank,
				perLevel = { pot = 1, tec = 1, vig = 1, sin = 0, esp = 0 },
			},
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
assert(hero.popup == "Atributos\nPOT: 5\nTEC: 5\nVIG: 5\nSIN: 0\nESP: 0\n\nStatus\nAtaque Fisico: 5\nAtaque Magico: 0\nPrecisao: 5\nDefesa Fisica: 5\nDefesa Magica: 0\nVida Maxima: 25\nMana Maxima: 0\n\nDisciplinas\n[1] Armamento - Rank 1\n  Por level: +1 POT, +1 TEC, +1 VIG")

assert(actions["!discipline"].onSay(admin, "!discipline", "remove, Hero, 1"))
assert(rank == 0)
assert(actions["!profile"].onSay(hero, "!profile", ""))
assert(hero.popup == "Atributos\nPOT: 0\nTEC: 0\nVIG: 0\nSIN: 0\nESP: 0\n\nStatus\nAtaque Fisico: 0\nAtaque Magico: 0\nPrecisao: 0\nDefesa Fisica: 0\nDefesa Magica: 0\nVida Maxima: 0\nMana Maxima: 0\n\nDisciplinas\nNenhuma disciplina adquirida.")

print("\n1 passed, 0 failed")
