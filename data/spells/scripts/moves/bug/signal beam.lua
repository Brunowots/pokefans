local damage = 75/2

function onCastSpell(creature, variant)
	if not creature:getTarget() then return true end
	local target = creature:getTarget()
	if creature:getMaster() then
		if target:getMaster() then
			if isInArray(getPartyMembers(creature:getMaster()), Player(getPlayerByName(target:getMaster():getName()))) then return false end
		elseif target:isPlayer() then
			if isInArray(getPartyMembers(creature:getMaster()), getPlayerByName(target:getName())) then return false end
		end
	end
	for i = 0, 1 do
		addEvent(doSendDistanceShoot, i * 150, creature:getPosition(), target:getPosition(), 5)
		addEvent(doTargetCombatHealth, i * 175, creature.uid, target.uid, COMBAT_BUGDAMAGE, damage, damage, 0)
		local pos = target:getPosition()
		pos:getNextPosition(1)
		pos:getNextPosition(2)
		addEvent(doSendMagicEffect, 250, pos, 899)
	end
	if math.random(1, 10) <= 10 then
		setConditionOn(creature.uid, target.uid, 'confusion')
	end
	return true
end
