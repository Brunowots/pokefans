local pokemons = 
{
	[1] = 
		{
			name = "Dragonite",
			level = 100
		},
	[2] = 
		{
			name = "Salamence",
			level = 100
		},

	[3] = 
		{
			name = "Haxorus",
			level = 100
		},

	[4] = 
		{
			name = "Goodra",
			level = 100
		},

	[5] = 
		{
			name = "Hisuian Goodra",
			level = 100
		},

	[6] = 
		{
			name = "Mega Garchomp",
			level = 100
		}
}

local keywordHandler = KeywordHandler:new()
local npcHandler = NpcHandler:new(keywordHandler)
NpcSystem.parseParameters(npcHandler)

function onCreatureAppear(cid)              npcHandler:onCreatureAppear(cid)            end
function onCreatureDisappear(cid)           npcHandler:onCreatureDisappear(cid)         end
function onCreatureSay(cid, type, msg)      npcHandler:onCreatureSay(cid, type, msg)    end
function onThink()                          npcHandler:onThink()                        end

local function creatureGreetCallback(cid, message)
	if message == nil then
		return true
	end
	if npcHandler:hasFocus() then
		selfSay("Espere sua vez, " .. Player(cid):getName() .. "!")
		return false
	end
	return true
end

local function creatureSayCallback(cid, type, msg)
	if not npcHandler:isFocused(cid) then
		return false
	end	
	if msgcontains(msg, 'bye') or msgcontains(msg, 'no') or msgcontains(msg, 'nao') then
		selfSay('Volte quando estiver pronto!', cid)
		npcHandler:releaseFocus(cid)
	elseif (msgcontains(msg, 'yes') or msgcontains(msg, 'sim')) and not Player(cid):getParty() then
		local player = Player(cid)
		if player then
			selfSay('Te derrotarei com todos meus pokemons!', cid)
			npcHandler.topic[cid] = 1
			npcHandler:setMaxIdleTime(600)
			player:setDuelWithNpc()
		end
	elseif Player(cid):getParty() then
		selfSay('Saia do grupo para me enfrentar!', cid)
		npcHandler:releaseFocus(cid)
	end
	return true
end

local function creatureOnReleaseFocusCallback(cid)
	local npc = Npc()
	if hasSummons(npc) then
		local monster = npc:getSummons()[1]
		monster:getPosition():sendMagicEffect(balls.pokeball.effectRelease)
		monster:remove()
	end
	local player = Player(cid)
	if player then
		player:unsetDuelWithNpc()
	end
	return true
end

local function creatureOnDisapearCallback(cid)
	local player = Player(cid)
	if not player then
		npcHandler:updateFocus()
		return true
	end
	if npcHandler:isFocused(cid) then
		if getDistanceTo(cid) >= 0 and getDistanceTo(cid) <= 8 then
			return false
		end
		selfSay("Mais sorte na proxima tentativa, " .. player:getName() .. "!", cid)
		Player(cid):teleportTo(Position(283, 869, 7))		
		npcHandler:releaseFocus(cid)
	end
	return true
end

local function creatureOnThinkCallback()
	if npcHandler:hasFocus() then
		local npc = Npc()
		local npcPosition = npc:getPosition()
		local spectators = Game.getSpectators(npcPosition, false, true)
		for i = 1, #spectators do
			local player = spectators[i]
			local cid = player:getId()
			if npcHandler:isFocused(cid) and npcHandler.topic[cid] == 1 then
				local duelStatus = player:getDuelWithNpcStatus()
				local monster = npc:getSummons()[1]
				if not monster then
					if pokemons[duelStatus] then
						selfSay(pokemons[duelStatus].name .. ", eu escolho voce!")
						npcPosition:getNextPosition(npc:getDirection())
						monster = Game.createMonster(pokemons[duelStatus].name, npcPosition, false, true, pokemons[duelStatus].level, 0)
						npcPosition:sendMagicEffect(balls.pokeball.effectRelease)
						monster:setMaster(npc)
						local health = monster:getTotalHealth() * 10
						monster:setMaxHealth(health)
						monster:setHealth(health)
						monster:changeSpeed(-monster:getSpeed() + monster:getTotalSpeed())
						player:increaseDuelWithNpcStatus()
					else
						selfSay('Incrivel, ' .. player:getName() .. "! Talvez voce realmente tenha chances de vencer a liga.", cid)
						Player(cid):teleportTo(Position(282, 872, 1))
						npcHandler:releaseFocus(cid)
					end
				end
				if hasSummons(player) and hasSummons(npc) then
					monster:selectTarget(player:getSummons()[1])
				end
				local pokeballs = player:getPokeballs()
				local pokemonsAlive = 0
				for i=1, #pokeballs do
					local ball = pokeballs[i]					
					if ball:getSpecialAttribute("pokeHealth") > 0 then
						pokemonsAlive = pokemonsAlive + 1
					end
				end
				if pokemonsAlive == 0 then
					selfSay("Mais sorte na proxima tentativa, " .. player:getName() .. "!", cid)
					Player(cid):teleportTo(Position(283, 869, 7))
					npcHandler:releaseFocus(cid)				
				end
			end
		end

	end
	return true
end

npcHandler:setCallback(CALLBACK_MESSAGE_DEFAULT, creatureSayCallback)
npcHandler:setCallback(CALLBACK_ONTHINK, creatureOnThinkCallback)
npcHandler:setCallback(CALLBACK_ONRELEASEFOCUS, creatureOnReleaseFocusCallback)
npcHandler:setCallback(CALLBACK_CREATURE_DISAPPEAR, creatureOnDisapearCallback)
npcHandler:setCallback(CALLBACK_GREET, creatureGreetCallback)
npcHandler:addModule(FocusModule:new())
