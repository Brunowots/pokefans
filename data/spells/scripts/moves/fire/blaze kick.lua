local damage = 85 

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
	doTargetCombatHealth(creature.uid, creature:getTarget().uid, COMBAT_FIREDAMAGE, damage, damage, 16, COMBAT_PHYSICALDAMAGE)
	target:getPosition():sendMagicEffect(233)
	if math.random(1, 100) <= 10 then
		setConditionOn(creature.uid, target.uid, 'burn')
	end
	return true
end
