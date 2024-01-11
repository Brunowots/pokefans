local area = createCombatArea(AREA_CIRCLE3X3)

local combatEvent = Combat()
combatEvent:setArea(createCombatArea(AREA_CIRCLE3X3))

function onTargetCreature(creature, target)
	if target:isNpc() then return false end
	if creature:getMaster() then
		if target:getMaster() then
			if isInArray(getPartyMembers(creature:getMaster()), Player(getPlayerByName(target:getMaster():getName()))) then return false end
		elseif target:isPlayer() then
			if isInArray(getPartyMembers(creature:getMaster()), getPlayerByName(target:getName())) then return false end
		end
	end
	setConditionOn(creature.uid, target.uid, 'attack minus')
end

combatEvent:setCallback(CALLBACK_PARAM_TARGETCREATURE, "onTargetCreature")

function onCastSpell(creature, variant)
	
	combatEvent:execute(creature, variant)
	doAreaCombatHealth(creature.uid, COMBAT_ELECTRICDAMAGE, creature:getPosition(), area, 0, 0, 0)
	local pos = creature:getPosition()
	pos:getNextPosition(1)
	pos:getNextPosition(2)
	pos:sendMagicEffect(842)
	return true
end
