function onThink(interval)
	local messages = {"Ajude a bater a meta semanal para ativar os eventos especiais! Mais sobre em discord.gg/G8uHTp9kHE", "Se gostou do servidor, consider doar para ajudar!", "Gostou do servidor? Convide seus amigos e faca ele crescer ainda mais!", "Chame seus amigos pro PokeFans!", "Acesse nosso trello para saber mais sobre o jogo trello.com/b/ELrkzeTi/pokefans"}
	broadcastMessage(messages[math.random(1, #messages)], MESSAGE_STATUS_WARNING)
	return true
end
