local damage = 90
local area = createCombatArea(AREA_CIRCLE6X6_CALLBACK)

local combatEvent = Combat()
combatEvent:setArea(createCombatArea(AREA_CIRCLE6X6_CALLBACK))

function onTargetCreature(creature, target)
	if target:isNpc() then return false end
	if isInArray({"squirtle turret", "charmander turret", "bulbasaur turret", "caterpie turret", "pikachu turret", "koffing turret", "bellsprout turrent", "porygon turret"}, target:getName():lower()) then return false end
	if creature:getMaster() then
		if target:getMaster() then
			if isInArray(getPartyMembers(creature:getMaster()), Player(getPlayerByName(target:getMaster():getName()))) then return false end
		elseif target:isPlayer() then
			if isInArray(getPartyMembers(creature:getMaster()), getPlayerByName(target:getName())) then return false end
		end
	end
	addEvent(doSendMagicEffect, 150, creature:getPosition(), 527)
	addEvent(doTargetCombatHealth, 200, creature.uid, target.uid, COMBAT_NORMALDAMAGE, damage, damage, 230, COMBAT_PHYSICALDAMAGE)
end

combatEvent:setCallback(CALLBACK_PARAM_TARGETCREATURE, "onTargetCreature")

function onCastSpell(creature, variant)
	combatEvent:execute(creature, variant)
	doAreaCombatHealth(creature.uid, COMBAT_NORMALDAMAGE, creature:getPosition(), area, 0, 0, 0)
	return true
end