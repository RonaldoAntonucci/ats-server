local profile = TalkAction("!profile")

function profile.onSay(player, words, param)
	local data = player:getDisciplineProfile()
	local attributes = data.attributes
	local stats = data.stats
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
	if #data.disciplines == 0 then
		table.insert(lines, "Nenhuma disciplina adquirida.")
	else
		for _, discipline in ipairs(data.disciplines) do
			table.insert(lines, ("[%d] %s — Rank %d"):format(discipline.id, discipline.name, discipline.rank))
		end
	end
	player:popupFYI(table.concat(lines, "\n"))
	return true
end

profile:groupType("normal")
profile:register()
