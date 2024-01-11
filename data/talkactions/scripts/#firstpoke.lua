function onSay(player, words, param)
	local firstItems = {8922, 1988}
	
	if player:getStorageValue(63001) ~= 1 then
		for i = 1, #firstItems do
			player:addItem(firstItems[i], 1)
		end
		player:addSlotItems()
		local monsterType = MonsterType(param)
		local baseHealth = monsterType:getMaxHealth()
		local maxHealth = math.floor(20 + (baseHealth * 0.10) + baseHealth)
		local backpack = player:getSlotItem(CONST_SLOT_BACKPACK)
		local addPokeball = backpack:addItem(26666, 1)
		if addPokeball then
			addPokeball:setSpecialAttribute("pokeName", param)
			addPokeball:setSpecialAttribute("pokeLevel", 5)
			addPokeball:setSpecialAttribute("pokeExperience", 0)
			addPokeball:setSpecialAttribute("pokeBoost", 0)
			addPokeball:setSpecialAttribute("pokeMaxHealth", maxHealth)
			addPokeball:setSpecialAttribute("pokeHealth", maxHealth)
			addPokeball:setSpecialAttribute("pokeLove", 0)
			addPokeball:setSpecialAttribute("evhp", 0)
			addPokeball:setSpecialAttribute("evatk", 0)
			addPokeball:setSpecialAttribute("evdef", 0)
			addPokeball:setSpecialAttribute("evpoints", 300)
			addPokeball:setSpecialAttribute("evkills", 0)
			addPokeball:setSpecialAttribute("unique", 1)
			player:refreshPokemonBar({}, {})
		else
			print("WARNING! Player " .. player:getName() .. " without initial pokeball.")
		end
		player:addItem(27642, 100)
		player:addItem(26662, 100)
		player:addItem(27645, 10)
		player:setStorageValue(63001, 1)
		player:teleportTo(Position(42, 129, 15))
	end
	return false
end