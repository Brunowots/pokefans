local damage = 40

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
	doSendDistanceShoot(creature:getPosition(), creature:getTarget():getPosition(), 93)
	doTargetCombatHealth(creature.uid, creature:getTarget().uid, COMBAT_ELECTRICDAMAGE, damage, damage, 877)
	return true
end