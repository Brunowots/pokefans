local damage = 60

function onCastSpell(creature, variant)
	if not creature:getTarget() then return true end
	doTargetCombatHealth(creature.uid, creature:getTarget().uid, COMBAT_GHOSTDAMAGE, damage, damage, 465, COMBAT_PHYSICALDAMAGE)
	return true
end

