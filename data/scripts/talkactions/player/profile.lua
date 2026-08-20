local profile = TalkAction("!profile")

function profile.onSay(player, words, param)
	local data = player:getDisciplineProfile()
	local attributes = data.attributes
	local lines = {
		"Atributos",
		("FOR: %d"):format(attributes["for"]),
		("DES: %d"):format(attributes.des),
		("VIT: %d"):format(attributes.vit),
		("INT: %d"):format(attributes.int),
		("VON: %d"):format(attributes.von),
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
