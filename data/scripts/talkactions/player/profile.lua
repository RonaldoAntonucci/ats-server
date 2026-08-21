local profile = TalkAction("!profile")

local attributePresentation = {
	{ key = "pot", label = "POT" },
	{ key = "tec", label = "TEC" },
	{ key = "vig", label = "VIG" },
	{ key = "sin", label = "SIN" },
	{ key = "esp", label = "ESP" },
}

function profile.onSay(player, words, param)
	local attributes = player:getAttributes()
	local stats = player:getStats()
	local disciplines = player:getDisciplines()
	local lines = {
		"Atributos",
		("POT: %d"):format(attributes.pot),
		("TEC: %d"):format(attributes.tec),
		("VIG: %d"):format(attributes.vig),
		("SIN: %d"):format(attributes.sin),
		("ESP: %d"):format(attributes.esp),
		"",
		"Status",
		("Ataque Físico: %d"):format(stats.physicalAttack),
		("Ataque Mágico: %d"):format(stats.magicalAttack),
		("Precisão: %d"):format(stats.precision),
		("Defesa Física: %d"):format(stats.physicalDefense),
		("Defesa Mágica: %d"):format(stats.magicalDefense),
		("Vida Máxima: %d"):format(stats.maximumHealth),
		("Mana Máxima: %d"):format(stats.maximumMana),
		"",
		"Disciplinas",
	}
	if #disciplines == 0 then
		table.insert(lines, "Nenhuma disciplina adquirida.")
	else
		for _, discipline in ipairs(disciplines) do
			table.insert(lines, ("[%d] %s — Rank %d"):format(discipline.id, discipline.name, discipline.rank))
			local contributions = {}
			for _, attribute in ipairs(attributePresentation) do
				local value = discipline.perLevel[attribute.key]
				if value > 0 then
					table.insert(contributions, ("+%d %s"):format(value, attribute.label))
				end
			end
			if #contributions > 0 then
				table.insert(lines, "  Por level: " .. table.concat(contributions, ", "))
			end
		end
	end
	player:popupFYI(table.concat(lines, "\n"))
	return true
end

profile:groupType("normal")
profile:register()
