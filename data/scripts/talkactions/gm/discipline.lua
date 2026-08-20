local discipline = TalkAction("!discipline")

local function trim(value)
	return value and value:match("^%s*(.-)%s*$") or nil
end

local function audit(player, target, id, before, after, result)
	logger.info("[DisciplineAdmin] actor={} target={} id={} before={} after={} result={}", player:getName(), target or "-", id or "-", before or "-", after or "-", result)
end

local function message(result)
	local messages = {
		unknown_discipline = "Disciplina inexistente.",
		not_owned = "O personagem não possui essa disciplina.",
		rank_limit = "A disciplina atingiu a capacidade técnica de rank.",
		invalid_id = "ID de disciplina inválido.",
	}
	return messages[result] or "Parâmetros inválidos. Use add, NOME, ID ou remove, NOME, ID."
end

function discipline.onSay(player, words, param)
	logCommand(player, words, param)
	local operation, name, idText = param:match("^%s*([^,]*)%s*,%s*([^,]*)%s*,%s*([^,]*)%s*$")
	operation = trim(operation)
	name = trim(name)
	idText = trim(idText)
	local validId = idText and idText:match("^%d+$")
	if (operation ~= "add" and operation ~= "remove") or not name or name == "" or not validId then
		audit(player, name ~= "" and name or nil, validId and idText or nil, nil, nil, "invalid_syntax")
		player:sendTextMessage(MESSAGE_FAILURE, "Use !discipline add, NOME, ID ou !discipline remove, NOME, ID.")
		return true
	end
	local id = tonumber(idText)
	local target = Player(name)
	if not target then
		audit(player, name, idText, nil, nil, "target_offline")
		player:sendTextMessage(MESSAGE_FAILURE, "Personagem deve estar online.")
		return true
	end
	local success, before, after, resultCode
	if operation == "add" then
		success, before, after, resultCode = target:addDisciplineRank(id)
	else
		success, before, after, resultCode = target:removeDisciplineRank(id)
	end
	audit(player, target:getName(), idText, before, after, resultCode)
	if not success then
		player:sendTextMessage(MESSAGE_FAILURE, message(resultCode))
		return true
	end
	local text = ("Disciplina %d atualizada: rank %d."):format(id, after)
	player:sendTextMessage(MESSAGE_ADMINISTRATOR, text)
	if target ~= player then
		target:sendTextMessage(MESSAGE_ADMINISTRATOR, text)
	end
	return true
end

discipline:separator(" ")
discipline:groupType("gamemaster")
discipline:register()
