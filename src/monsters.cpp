/**
 * The Forgotten Server - a free and open-source MMORPG server emulator
 * Copyright (C) 2016  Mark Samman <mark.samman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "otpch.h"

#include "monsters.h"
#include "monster.h"
#include "spells.h"
#include "combat.h"
#include "weapons.h"
#include "configmanager.h"
#include "game.h"
#include "player.h"

#include "pugicast.h"

extern Game g_game;
extern Spells* g_spells;
extern Monsters g_monsters;
extern ConfigManager g_config;

spellBlock_t::~spellBlock_t()
{
	if (combatSpell) {
		delete spell;
	}
}

uint32_t Monsters::getLootRandom()
{
	return uniform_random(0, MAX_LOOTCHANCE) / g_config.getNumber(ConfigManager::RATE_LOOT);
}

//void MonsterType::createLoot(Container* corpse)
void MonsterType::createLoot(Container* corpse, double bonus) //pota
{
	if (g_config.getNumber(ConfigManager::RATE_LOOT) == 0) {
		corpse->startDecaying();
		return;
	}

	Player* owner = g_game.getPlayerByID(corpse->getCorpseOwner());
	if (!owner || owner->getStaminaMinutes() > 840) {
		for (auto it = info.lootItems.rbegin(), end = info.lootItems.rend(); it != end; ++it) {
//			auto itemList = createLootItem(*it);

			auto itemList = createLootItem(*it, bonus); //pota
			if (itemList.empty()) {
				continue;
			}

			for (Item* item : itemList) {
				//check containers
				if (Container* container = item->getContainer()) {
//					if (!createLootContainer(container, *it)) {
					if (!createLootContainer(container, *it, bonus)) { //pota
						delete container;
						continue;
					}
				}

				if (g_game.internalAddItem(corpse, item) != RETURNVALUE_NOERROR) {
					corpse->internalAddThing(item);
				}
			}
		}

		if (owner) {
			std::ostringstream ss;
			ss << "Loot of " << nameDescription << ": " << corpse->getContentDescription();

			if (owner->getParty()) {
				owner->getParty()->broadcastPartyLoot(ss.str());
			} else {
				owner->sendTextMessage(MESSAGE_LOOT, ss.str());
			}
		}
	} else {
		std::ostringstream ss;
		ss << "Loot of " << nameDescription << ": nothing (due to low stamina)";

		if (owner->getParty()) {
			owner->getParty()->broadcastPartyLoot(ss.str());
		} else {
			owner->sendTextMessage(MESSAGE_LOOT, ss.str());
		}
	}

	corpse->startDecaying();
}

//std::vector<Item*> MonsterType::createLootItem(const LootBlock& lootBlock)
std::vector<Item*> MonsterType::createLootItem(const LootBlock& lootBlock, double bonus) //pota
{
	int32_t itemCount = 0;

	uint32_t randvalue = Monsters::getLootRandom();
	double modifier = randvalue * bonus; //pota
	// make sure no underflow happens
	if (static_cast<int64_t>(randvalue) - modifier < 0) { //pota
		randvalue = 0;
	} else {
		randvalue = randvalue - modifier;
	}
	if (randvalue < lootBlock.chance) {
		if (Item::items[lootBlock.id].stackable) {
			itemCount = randvalue % lootBlock.countmax + 1;
		} else {
			itemCount = 1;
		}
	}

	std::vector<Item*> itemList;
	while (itemCount > 0) {
		uint16_t n = static_cast<uint16_t>(std::min<int32_t>(itemCount, 100));
		Item* tmpItem = Item::CreateItem(lootBlock.id, n);
		if (!tmpItem) {
			break;
		}

		itemCount -= n;

		if (lootBlock.subType != -1) {
			tmpItem->setSubType(lootBlock.subType);
		}

		if (lootBlock.actionId != -1) {
			tmpItem->setActionId(lootBlock.actionId);
		}

		if (!lootBlock.text.empty()) {
			tmpItem->setText(lootBlock.text);
		}

		itemList.push_back(tmpItem);
	}
	return itemList;
}

//bool MonsterType::createLootContainer(Container* parent, const LootBlock& lootblock)
bool MonsterType::createLootContainer(Container* parent, const LootBlock& lootblock, double bonus) //pota
{
	auto it = lootblock.childLoot.begin(), end = lootblock.childLoot.end();
	if (it == end) {
		return true;
	}

	for (; it != end && parent->size() < parent->capacity(); ++it) {
//		auto itemList = createLootItem(*it);
		auto itemList = createLootItem(*it, bonus); //pota
		for (Item* tmpItem : itemList) {
			if (Container* container = tmpItem->getContainer()) {
				if (!createLootContainer(container, *it)) {
					delete container;
				} else {
					parent->internalAddThing(container);
				}
			} else {
				parent->internalAddThing(tmpItem);
			}
		}
	}
	return !parent->empty();
}

bool Monsters::loadFromXml(bool reloading /*= false*/)
{
	allMonsters = {}; //pota
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file("data/monster/monsters.xml");
	if (!result) {
		printXMLError("Error - Monsters::loadFromXml", "data/monster/monsters.xml", result);
		return false;
	}

	loaded = true;

	std::list<std::pair<MonsterType*, std::string>> monsterScriptList;
	for (auto monsterNode : doc.child("monsters").children()) {
		std::string name = monsterNode.attribute("name").as_string(); //pota
		loadMonster("data/monster/" + std::string(monsterNode.attribute("file").as_string()), name, monsterScriptList, reloading); //pota
		allMonsters.push_back(name); //pota
	}

	if (!monsterScriptList.empty()) {
		if (!scriptInterface) {
			scriptInterface.reset(new LuaScriptInterface("Monster Interface"));
			scriptInterface->initState();
		}

		for (const auto& scriptEntry : monsterScriptList) {
			MonsterType* mType = scriptEntry.first;
			if (scriptInterface->loadFile("data/monster/scripts/" + scriptEntry.second) == 0) {
				mType->info.scriptInterface = scriptInterface.get();
				mType->info.creatureAppearEvent = scriptInterface->getEvent("onCreatureAppear");
				mType->info.creatureDisappearEvent = scriptInterface->getEvent("onCreatureDisappear");
				mType->info.creatureMoveEvent = scriptInterface->getEvent("onCreatureMove");
				mType->info.creatureSayEvent = scriptInterface->getEvent("onCreatureSay");
				mType->info.thinkEvent = scriptInterface->getEvent("onThink");
			} else {
				std::cout << "[Warning - Monsters::loadMonster] Can not load script: " << scriptEntry.second << std::endl;
				std::cout << scriptInterface->getLastLuaError() << std::endl;
			}
		}
	}
	return true;
}

bool Monsters::reload()
{
	loaded = false;

	scriptInterface.reset();

	return loadFromXml(true);
}

ConditionDamage* Monsters::getDamageCondition(ConditionType_t conditionType,
        int32_t maxDamage, int32_t minDamage, int32_t startDamage, uint32_t tickInterval)
{
	ConditionDamage* condition = static_cast<ConditionDamage*>(Condition::createCondition(CONDITIONID_COMBAT, conditionType, 0, 0));
	condition->setParam(CONDITION_PARAM_TICKINTERVAL, tickInterval);
	condition->setParam(CONDITION_PARAM_MINVALUE, minDamage);
	condition->setParam(CONDITION_PARAM_MAXVALUE, maxDamage);
	condition->setParam(CONDITION_PARAM_STARTVALUE, startDamage);
	condition->setParam(CONDITION_PARAM_DELAYED, 1);
	return condition;
}

bool Monsters::deserializeSpell(const pugi::xml_node& node, spellBlock_t& sb, const std::string& description)
{
	std::string name;
	std::string scriptName;
	bool isScripted;

	pugi::xml_attribute attr;
	if ((attr = node.attribute("script"))) {
		scriptName = attr.as_string();
		isScripted = true;
	} else if ((attr = node.attribute("name"))) {
		name = attr.as_string();
		isScripted = false;
	} else {
		return false;
	}

	if ((attr = node.attribute("speed")) || (attr = node.attribute("interval"))) {
		sb.speed = std::max<int32_t>(1, pugi::cast<int32_t>(attr.value()));
	}

	if ((attr = node.attribute("chance"))) {
		uint32_t chance = pugi::cast<uint32_t>(attr.value());
		if (chance > 100) {
			chance = 100;
		}
		sb.chance = chance;
	}

	if ((attr = node.attribute("range"))) {
		uint32_t range = pugi::cast<uint32_t>(attr.value());
		if (range > (Map::maxViewportX * 2)) {
			range = Map::maxViewportX * 2;
		}
		sb.range = range;
	}

	if ((attr = node.attribute("min"))) {
		sb.minCombatValue = pugi::cast<int32_t>(attr.value());
	}

	if ((attr = node.attribute("name"))) {//pota
		sb.name = attr.as_string();
	}

	if ((attr = node.attribute("level"))) {//pota
		sb.level = pugi::cast<uint16_t>(attr.value());
	}

	if ((attr = node.attribute("isTarget"))) {//pota
		sb.isTarget = attr.as_bool();
	}

	if ((attr = node.attribute("max"))) {
		sb.maxCombatValue = pugi::cast<int32_t>(attr.value());

		//normalize values
		if (std::abs(sb.minCombatValue) > std::abs(sb.maxCombatValue)) {
			int32_t value = sb.maxCombatValue;
			sb.maxCombatValue = sb.minCombatValue;
			sb.minCombatValue = value;
		}
	}

	if (auto spell = g_spells->getSpellByName(name)) {
		sb.spell = spell;
		return true;
	}

	CombatSpell* combatSpell = nullptr;
	bool needTarget = false;
	bool needDirection = false;

	if (isScripted) {
		if ((attr = node.attribute("direction"))) {
			needDirection = attr.as_bool();
		}

		if ((attr = node.attribute("target"))) {
			needTarget = attr.as_bool();
		}

		std::unique_ptr<CombatSpell> combatSpellPtr(new CombatSpell(nullptr, needTarget, needDirection));
		if (!combatSpellPtr->loadScript("data/" + g_spells->getScriptBaseName() + "/scripts/" + scriptName)) {
			return false;
		}

		if (!combatSpellPtr->loadScriptCombat()) {
			return false;
		}

		combatSpell = combatSpellPtr.release();
		combatSpell->getCombat()->setPlayerCombatValues(COMBAT_FORMULA_DAMAGE, sb.minCombatValue, 0, sb.maxCombatValue, 0);
	} else {
		Combat* combat = new Combat;
		if ((attr = node.attribute("length"))) {
			int32_t length = pugi::cast<int32_t>(attr.value());
			if (length > 0) {
				int32_t spread = 3;

				//need direction spell
				if ((attr = node.attribute("spread"))) {
					spread = std::max<int32_t>(0, pugi::cast<int32_t>(attr.value()));
				}

				AreaCombat* area = new AreaCombat();
				area->setupArea(length, spread);
				combat->setArea(area);

				needDirection = true;
			}
		}

		if ((attr = node.attribute("radius"))) {
			int32_t radius = pugi::cast<int32_t>(attr.value());

			//target spell
			if ((attr = node.attribute("target"))) {
				needTarget = attr.as_bool();
			}

			AreaCombat* area = new AreaCombat();
			area->setupArea(radius);
			combat->setArea(area);
		}

		std::string tmpName = asLowerCaseString(name);

		if (tmpName == "melee") {
			sb.isMelee = true;

			pugi::xml_attribute attackAttribute, skillAttribute;
			if ((attackAttribute = node.attribute("attack")) && (skillAttribute = node.attribute("skill"))) {
				sb.minCombatValue = 0;
				sb.maxCombatValue = -Weapons::getMaxMeleeDamage(pugi::cast<int32_t>(skillAttribute.value()), pugi::cast<int32_t>(attackAttribute.value()));
			}

			ConditionType_t conditionType = CONDITION_NONE;
			int32_t minDamage = 0;
			int32_t maxDamage = 0;
			uint32_t tickInterval = 2000;

			if ((attr = node.attribute("fire"))) {
				conditionType = CONDITION_FIRE;

				minDamage = pugi::cast<int32_t>(attr.value());
				maxDamage = minDamage;
				tickInterval = 9000;
			} else if ((attr = node.attribute("poison"))) {
				conditionType = CONDITION_POISON;

				minDamage = pugi::cast<int32_t>(attr.value());
				maxDamage = minDamage;
				tickInterval = 5000;
			} else if ((attr = node.attribute("energy"))) {
				conditionType = CONDITION_ENERGY;

				minDamage = pugi::cast<int32_t>(attr.value());
				maxDamage = minDamage;
				tickInterval = 10000;
			} else if ((attr = node.attribute("drown"))) {
				conditionType = CONDITION_DROWN;

				minDamage = pugi::cast<int32_t>(attr.value());
				maxDamage = minDamage;
				tickInterval = 5000;
			} else if ((attr = node.attribute("freeze"))) {
				conditionType = CONDITION_FREEZING;

				minDamage = pugi::cast<int32_t>(attr.value());
				maxDamage = minDamage;
				tickInterval = 8000;
			} else if ((attr = node.attribute("dazzle"))) {
				conditionType = CONDITION_DAZZLED;

				minDamage = pugi::cast<int32_t>(attr.value());
				maxDamage = minDamage;
				tickInterval = 10000;
			} else if ((attr = node.attribute("curse"))) {
				conditionType = CONDITION_CURSED;

				minDamage = pugi::cast<int32_t>(attr.value());
				maxDamage = minDamage;
				tickInterval = 4000;
			} else if ((attr = node.attribute("bleed")) || (attr = node.attribute("physical"))) {
				conditionType = CONDITION_BLEEDING;
				tickInterval = 5000;
			}

			if ((attr = node.attribute("tick"))) {
				int32_t value = pugi::cast<int32_t>(attr.value());
				if (value > 0) {
					tickInterval = value;
				}
			}

			if (conditionType != CONDITION_NONE) {
				Condition* condition = getDamageCondition(conditionType, maxDamage, minDamage, 0, tickInterval);
				combat->setCondition(condition);
			}

			sb.range = 1;
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_PHYSICALDAMAGE);
			combat->setParam(COMBAT_PARAM_BLOCKARMOR, 1);
			combat->setParam(COMBAT_PARAM_BLOCKSHIELD, 1);
			combat->setOrigin(ORIGIN_MELEE);
		} else if (tmpName == "physical") {
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_PHYSICALDAMAGE);
			combat->setParam(COMBAT_PARAM_BLOCKARMOR, 1);
			combat->setOrigin(ORIGIN_RANGED);
		} else if (tmpName == "bleed") {
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_PHYSICALDAMAGE);
		} else if (tmpName == "poison") {
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_POISONDAMAGE);
		} else if (tmpName == "fire") {
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_FIREDAMAGE);
		} else if (tmpName == "energy") {
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_ENERGYDAMAGE);
		} else if (tmpName == "drown") {
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_DROWNDAMAGE);
		} else if (tmpName == "ice") {
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_ICEDAMAGE);
		} else if (tmpName == "holy") {
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_HOLYDAMAGE);
		} else if (tmpName == "death") {
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_DEATHDAMAGE);
		} else if (tmpName == "psychic") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_PSYCHICDAMAGE);
		} else if (tmpName == "grass") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_GRASSDAMAGE);
		} else if (tmpName == "normal") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_NORMALDAMAGE);
		} else if (tmpName == "water") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_WATERDAMAGE);
		} else if (tmpName == "flying") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_FLYINGDAMAGE);
		} else if (tmpName == "poison") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_POISONDAMAGE);
		} else if (tmpName == "electric") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_ELECTRICDAMAGE);
		} else if (tmpName == "ground") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_GROUNDDAMAGE);
		} else if (tmpName == "rock") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_ROCKDAMAGE);
		} else if (tmpName == "bug") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_BUGDAMAGE);
		} else if (tmpName == "dragon") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_DRAGONDAMAGE);
		} else if (tmpName == "ghost") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_GHOSTDAMAGE);
		} else if (tmpName == "dark") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_DARKDAMAGE);
		} else if (tmpName == "steel") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_STEELDAMAGE);
		} else if (tmpName == "fairy") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_FAIRYDAMAGE);
		} else if (tmpName == "fighting") { //pota
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_FIGHTINGDAMAGE);
		} else if (tmpName == "lifedrain") {
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_LIFEDRAIN);
		} else if (tmpName == "manadrain") {
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_MANADRAIN);
		} else if (tmpName == "healing") {
			combat->setParam(COMBAT_PARAM_TYPE, COMBAT_HEALING);
			combat->setParam(COMBAT_PARAM_AGGRESSIVE, 0);
		} else if (tmpName == "speed") {
			int32_t speedChange = 0;
			int32_t duration = 10000;

			if ((attr = node.attribute("duration"))) {
				duration = pugi::cast<int32_t>(attr.value());
			}

			if ((attr = node.attribute("speedchange"))) {
				speedChange = pugi::cast<int32_t>(attr.value());
				if (speedChange < -1000) {
					//cant be slower than 100%
					speedChange = -1000;
				}
			}

			ConditionType_t conditionType;
			if (speedChange > 0) {
				conditionType = CONDITION_HASTE;
				combat->setParam(COMBAT_PARAM_AGGRESSIVE, 0);
			} else if (speedChange == -100) { //pota
				conditionType = CONDITION_SLEEP;
			} else if (speedChange == 0) { //Helena
				conditionType = CONDITION_SILENCE;
			} else {
				conditionType = CONDITION_PARALYZE;
			}

			ConditionSpeed* condition = static_cast<ConditionSpeed*>(Condition::createCondition(CONDITIONID_COMBAT, conditionType, duration, 0));
			condition->setFormulaVars(speedChange / 1000.0, 0, speedChange / 1000.0, 0);
			combat->setCondition(condition);
		} else if (tmpName == "outfit") {
			int32_t duration = 10000;

			if ((attr = node.attribute("duration"))) {
				duration = pugi::cast<int32_t>(attr.value());
			}

			if ((attr = node.attribute("monster"))) {
				MonsterType* mType = g_monsters.getMonsterType(attr.as_string());
				if (mType) {
					ConditionOutfit* condition = static_cast<ConditionOutfit*>(Condition::createCondition(CONDITIONID_COMBAT, CONDITION_OUTFIT, duration, 0));
					condition->setOutfit(mType->info.outfit);
					combat->setParam(COMBAT_PARAM_AGGRESSIVE, 0);
					combat->setCondition(condition);
				}
			} else if ((attr = node.attribute("item"))) {
				Outfit_t outfit;
				outfit.lookTypeEx = pugi::cast<uint16_t>(attr.value());

				ConditionOutfit* condition = static_cast<ConditionOutfit*>(Condition::createCondition(CONDITIONID_COMBAT, CONDITION_OUTFIT, duration, 0));
				condition->setOutfit(outfit);
				combat->setParam(COMBAT_PARAM_AGGRESSIVE, 0);
				combat->setCondition(condition);
			}
		} else if (tmpName == "invisible") {
			int32_t duration = 10000;

			if ((attr = node.attribute("duration"))) {
				duration = pugi::cast<int32_t>(attr.value());
			}

			Condition* condition = Condition::createCondition(CONDITIONID_COMBAT, CONDITION_INVISIBLE, duration, 0);
			combat->setParam(COMBAT_PARAM_AGGRESSIVE, 0);
			combat->setCondition(condition);
		} else if (tmpName == "drunk") {
			int32_t duration = 10000;

			if ((attr = node.attribute("duration"))) {
				duration = pugi::cast<int32_t>(attr.value());
			}

			Condition* condition = Condition::createCondition(CONDITIONID_COMBAT, CONDITION_DRUNK, duration, 0);
			combat->setCondition(condition);
		} else if (tmpName == "firefield") {
			combat->setParam(COMBAT_PARAM_CREATEITEM, ITEM_FIREFIELD_PVP_FULL);
		} else if (tmpName == "poisonfield") {
			combat->setParam(COMBAT_PARAM_CREATEITEM, ITEM_POISONFIELD_PVP);
		} else if (tmpName == "energyfield") {
			combat->setParam(COMBAT_PARAM_CREATEITEM, ITEM_ENERGYFIELD_PVP);
		} else if (tmpName == "firecondition" || tmpName == "energycondition" ||
		           tmpName == "earthcondition" || tmpName == "poisoncondition" ||
		           tmpName == "icecondition" || tmpName == "freezecondition" ||
		           tmpName == "deathcondition" || tmpName == "cursecondition" ||
		           tmpName == "holycondition" || tmpName == "dazzlecondition" ||
		           tmpName == "drowncondition" || tmpName == "bleedcondition" ||
		           tmpName == "physicalcondition") {
			ConditionType_t conditionType = CONDITION_NONE;
			uint32_t tickInterval = 2000;

			if (tmpName == "firecondition") {
				conditionType = CONDITION_FIRE;
				tickInterval = 10000;
			} else if (tmpName == "poisoncondition") {
				conditionType = CONDITION_POISON;
				tickInterval = 5000;
			} else if (tmpName == "energycondition") {
				conditionType = CONDITION_ENERGY;
				tickInterval = 10000;
			} else if (tmpName == "drowncondition") {
				conditionType = CONDITION_DROWN;
				tickInterval = 5000;
			} else if (tmpName == "freezecondition" || tmpName == "icecondition") {
				conditionType = CONDITION_FREEZING;
				tickInterval = 10000;
			} else if (tmpName == "cursecondition" || tmpName == "deathcondition") {
				conditionType = CONDITION_CURSED;
				tickInterval = 4000;
			} else if (tmpName == "dazzlecondition" || tmpName == "holycondition") {
				conditionType = CONDITION_DAZZLED;
				tickInterval = 10000;
			} else if (tmpName == "physicalcondition" || tmpName == "bleedcondition") {
				conditionType = CONDITION_BLEEDING;
				tickInterval = 5000;
			}

			if ((attr = node.attribute("tick"))) {
				int32_t value = pugi::cast<int32_t>(attr.value());
				if (value > 0) {
					tickInterval = value;
				}
			}

			int32_t minDamage = std::abs(sb.minCombatValue);
			int32_t maxDamage = std::abs(sb.maxCombatValue);
			int32_t startDamage = 0;

			if ((attr = node.attribute("start"))) {
				int32_t value = std::abs(pugi::cast<int32_t>(attr.value()));
				if (value <= minDamage) {
					startDamage = value;
				}
			}

			Condition* condition = getDamageCondition(conditionType, maxDamage, minDamage, startDamage, tickInterval);
			combat->setCondition(condition);
		} else if (tmpName == "strength") {
			//
		} else if (tmpName == "effect") {
			//
		} else {
			std::cout << "[Error - Monsters::deserializeSpell] - " << description << " - Unknown spell name: " << name << std::endl;
			delete combat;
			return false;
		}

		combat->setPlayerCombatValues(COMBAT_FORMULA_DAMAGE, sb.minCombatValue, 0, sb.maxCombatValue, 0);
		combatSpell = new CombatSpell(combat, needTarget, needDirection);

		for (auto attributeNode : node.children()) {
			if ((attr = attributeNode.attribute("key"))) {
				const char* value = attr.value();
				if (strcasecmp(value, "shooteffect") == 0) {
					if ((attr = attributeNode.attribute("value"))) {
						ShootType_t shoot = getShootType(attr.as_string());
						if (shoot != CONST_ANI_NONE) {
							combat->setParam(COMBAT_PARAM_DISTANCEEFFECT, shoot);
						} else {
							std::cout << "[Warning - Monsters::deserializeSpell] " << description << " - Unknown shootEffect: " << attr.as_string() << std::endl;
						}
					}
				} else if (strcasecmp(value, "areaeffect") == 0) {
					if ((attr = attributeNode.attribute("value"))) {
						MagicEffectClasses effect = getMagicEffect(attr.as_string());
						if (effect != CONST_ME_NONE) {
							combat->setParam(COMBAT_PARAM_EFFECT, effect);
						} else {
							std::cout << "[Warning - Monsters::deserializeSpell] " << description << " - Unknown areaEffect: " << attr.as_string() << std::endl;
						}
					}
				} else {
					std::cout << "[Warning - Monsters::deserializeSpells] Effect type \"" << attr.as_string() << "\" does not exist." << std::endl;
				}
			}
		}
	}

	sb.spell = combatSpell;
	if (combatSpell) {
		sb.combatSpell = true;
	}
	return true;
}

bool Monsters::loadMonster(const std::string& file, const std::string& monsterName, std::list<std::pair<MonsterType*, std::string>>& monsterScriptList, bool reloading /*= false*/)
{
	MonsterType* mType = nullptr;
	bool new_mType = true;

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(file.c_str());
	if (!result) {
		printXMLError("Error - Monsters::loadMonster", file, result);
		return false;
	}

	pugi::xml_node monsterNode = doc.child("monster");
	if (!monsterNode) {
		std::cout << "[Error - Monsters::loadMonster] Missing monster node in: " << file << std::endl;
		return false;
	}

	pugi::xml_attribute attr;
	if (!(attr = monsterNode.attribute("name"))) {
		std::cout << "[Error - Monsters::loadMonster] Missing name in: " << file << std::endl;
		return false;
	}

	if (reloading) {
		mType = getMonsterType(monsterName);
		if (mType != nullptr) {
			new_mType = false;
			mType->info = {};
		}
	}

	if (new_mType) {
		mType = &monsters[asLowerCaseString(monsterName)];
	}

	mType->name = attr.as_string();

	if ((attr = monsterNode.attribute("nameDescription"))) {
		mType->nameDescription = attr.as_string();
	} else {
		mType->nameDescription = "a " + asLowerCaseString(mType->name);
	}

	if ((attr = monsterNode.attribute("race"))) { //pota
		std::string tmpStrValue = asLowerCaseString(attr.as_string());
		uint16_t tmpInt = pugi::cast<uint16_t>(attr.value());
		if (tmpStrValue == "venom" || tmpInt == 1) {
			mType->info.race = RACE_VENOM;
		} else if (tmpStrValue == "blood" || tmpInt == 2) {
			mType->info.race = RACE_BLOOD;
		} else if (tmpStrValue == "undead" || tmpInt == 3) {
			mType->info.race = RACE_UNDEAD;
		} else if (tmpStrValue == "fire" || tmpInt == 4) {
			mType->info.race = RACE_FIRE;
		} else if (tmpStrValue == "energy" || tmpInt == 5) {
			mType->info.race = RACE_ENERGY;
		} else if (tmpStrValue == "grass" || tmpInt == 6) {
			mType->info.race = RACE_GRASS;
		} else if (tmpStrValue == "normal" || tmpInt == 7) {
			mType->info.race = RACE_NORMAL;
		} else if (tmpStrValue == "water" || tmpInt == 8) {
			mType->info.race = RACE_WATER;
		} else if (tmpStrValue == "flying" || tmpInt == 9) {
			mType->info.race = RACE_FLYING;
		} else if (tmpStrValue == "poison" || tmpInt == 10) {
			mType->info.race = RACE_POISON;
		} else if (tmpStrValue == "electric" || tmpInt == 11) {
			mType->info.race = RACE_ELECTRIC;
		} else if (tmpStrValue == "ground" || tmpInt == 12) {
			mType->info.race = RACE_GROUND;
		} else if (tmpStrValue == "psychic" || tmpInt == 13) {
			mType->info.race = RACE_PSYCHIC;
		} else if (tmpStrValue == "rock" || tmpInt == 14) {
			mType->info.race = RACE_ROCK;
		} else if (tmpStrValue == "ice" || tmpInt == 15) {
			mType->info.race = RACE_ICE;
		} else if (tmpStrValue == "bug" || tmpInt == 16) {
			mType->info.race = RACE_BUG;
		} else if (tmpStrValue == "dragon" || tmpInt == 17) {
			mType->info.race = RACE_DRAGON;
		} else if (tmpStrValue == "ghost" || tmpInt == 18) {
			mType->info.race = RACE_GHOST;
		} else if (tmpStrValue == "dark" || tmpInt == 19) {
			mType->info.race = RACE_DARK;
		} else if (tmpStrValue == "steel" || tmpInt == 20) {
			mType->info.race = RACE_STEEL;
		} else if (tmpStrValue == "fairy" || tmpInt == 21) {
			mType->info.race = RACE_FAIRY;
		} else if (tmpStrValue == "fighting" || tmpInt == 22) {
			mType->info.race = RACE_FIGHTING;
		} else if (tmpStrValue == "none" || tmpInt == 23) {
			mType->info.race = RACE_NONE;
		} else {
			std::cout << "[Warning - Monsters::loadMonster] Unknown race type " << attr.as_string() << ". " << file << std::endl;
		}
	}

	if ((attr = monsterNode.attribute("race2"))) { //pota
		std::string tmpStrValue = asLowerCaseString(attr.as_string());
		uint16_t tmpInt = pugi::cast<uint16_t>(attr.value());
		if (tmpStrValue == "venom" || tmpInt == 1) {
			mType->info.race2 = RACE_VENOM;
		} else if (tmpStrValue == "blood" || tmpInt == 2) {
			mType->info.race2 = RACE_BLOOD;
		} else if (tmpStrValue == "undead" || tmpInt == 3) {
			mType->info.race2 = RACE_UNDEAD;
		} else if (tmpStrValue == "fire" || tmpInt == 4) {
			mType->info.race2 = RACE_FIRE;
		} else if (tmpStrValue == "energy" || tmpInt == 5) {
			mType->info.race2 = RACE_ENERGY;
		} else if (tmpStrValue == "grass" || tmpInt == 6) {
			mType->info.race2 = RACE_GRASS;
		} else if (tmpStrValue == "normal" || tmpInt == 7) {
			mType->info.race2 = RACE_NORMAL;
		} else if (tmpStrValue == "water" || tmpInt == 8) {
			mType->info.race2 = RACE_WATER;
		} else if (tmpStrValue == "flying" || tmpInt == 9) {
			mType->info.race2 = RACE_FLYING;
		} else if (tmpStrValue == "poison" || tmpInt == 10) {
			mType->info.race2 = RACE_POISON;
		} else if (tmpStrValue == "electric" || tmpInt == 11) {
			mType->info.race2 = RACE_ELECTRIC;
		} else if (tmpStrValue == "ground" || tmpInt == 12) {
			mType->info.race2 = RACE_GROUND;
		} else if (tmpStrValue == "psychic" || tmpInt == 13) {
			mType->info.race2 = RACE_PSYCHIC;
		} else if (tmpStrValue == "rock" || tmpInt == 14) {
			mType->info.race2 = RACE_ROCK;
		} else if (tmpStrValue == "ice" || tmpInt == 15) {
			mType->info.race2 = RACE_ICE;
		} else if (tmpStrValue == "bug" || tmpInt == 16) {
			mType->info.race2 = RACE_BUG;
		} else if (tmpStrValue == "dragon" || tmpInt == 17) {
			mType->info.race2 = RACE_DRAGON;
		} else if (tmpStrValue == "ghost" || tmpInt == 18) {
			mType->info.race2 = RACE_GHOST;
		} else if (tmpStrValue == "dark" || tmpInt == 19) {
			mType->info.race2 = RACE_DARK;
		} else if (tmpStrValue == "steel" || tmpInt == 20) {
			mType->info.race2 = RACE_STEEL;
		} else if (tmpStrValue == "fairy" || tmpInt == 21) {
			mType->info.race2 = RACE_FAIRY;
		} else if (tmpStrValue == "fighting" || tmpInt == 22) {
			mType->info.race2 = RACE_FIGHTING;
		} else if (tmpStrValue == "none" || tmpInt == 23) {
			mType->info.race2 = RACE_NONE;
		} else {
			std::cout << "[Warning - Monsters::loadMonster] Unknown race2 type " << attr.as_string() << ". " << file << std::endl;
		}
	}

	if ((attr = monsterNode.attribute("experience"))) {
		mType->info.experience = pugi::cast<uint64_t>(attr.value());
	}

	if ((attr = monsterNode.attribute("speed"))) {
		mType->info.baseSpeed = pugi::cast<int32_t>(attr.value());
	}

	if ((attr = monsterNode.attribute("manacost"))) {
		mType->info.manaCost = pugi::cast<uint32_t>(attr.value());
	}

	if ((attr = monsterNode.attribute("skull"))) {
		mType->info.skull = getSkullType(attr.as_string());
	}

	if ((attr = monsterNode.attribute("script"))) {
		monsterScriptList.emplace_back(mType, attr.as_string());
	}

	pugi::xml_node node;
	if ((node = monsterNode.child("health"))) {
		if ((attr = node.attribute("now"))) {
			mType->info.health = pugi::cast<int32_t>(attr.value());
		} else {
			std::cout << "[Error - Monsters::loadMonster] Missing health now. " << file << std::endl;
		}

		if ((attr = node.attribute("max"))) {
			mType->info.healthMax = pugi::cast<int32_t>(attr.value());
		} else {
			std::cout << "[Error - Monsters::loadMonster] Missing health max. " << file << std::endl;
		}
	}

	if ((node = monsterNode.child("level"))) { //pota
		if ((attr = node.attribute("min"))) {
			mType->info.minLevel = pugi::cast<uint16_t>(attr.value());

		} else {
			std::cout << "[Error - Monsters::loadMonster] Missing level min. " << file << std::endl;

		}

		if ((attr = node.attribute("max"))) {
			mType->info.maxLevel = pugi::cast<uint16_t>(attr.value());

		} else {
			std::cout << "[Error - Monsters::loadMonster] Missing level max. " << file << std::endl;

		}
	}

	if ((node = monsterNode.child("flags"))) {
		for (auto flagNode : node.children()) {
			attr = flagNode.first_attribute();
			const char* attrName = attr.name();
			if (strcasecmp(attrName, "summonable") == 0) {
				mType->info.isSummonable = attr.as_bool();
			} else if (strcasecmp(attrName, "attackable") == 0) {
				mType->info.isAttackable = attr.as_bool();
			} else if (strcasecmp(attrName, "hostile") == 0) {
				mType->info.isHostile = attr.as_bool();
			} else if (strcasecmp(attrName, "passive") == 0) {
				mType->info.isPassive = attr.as_bool();

			} else if (strcasecmp(attrName, "flyable") == 0) {
				mType->info.isFlyable = pugi::cast<int32_t>(attr.value());
			} else if (strcasecmp(attrName, "rideable") == 0) {
				mType->info.isRideable = pugi::cast<int32_t>(attr.value());
			} else if (strcasecmp(attrName, "surfable") == 0) {
				mType->info.isSurfable = pugi::cast<int32_t>(attr.value());
			} else if (strcasecmp(attrName, "canteleport") == 0) {
				mType->info.canTeleport = attr.as_bool();
			} else if (strcasecmp(attrName, "catchchance") == 0) {
				mType->info.catchChance = pugi::cast<int32_t>(attr.value());
			} else if (strcasecmp(attrName, "moveMagicAttackBase") == 0) {
				mType->info.moveMagicAttackBase = pugi::cast<int32_t>(attr.value());
			} else if (strcasecmp(attrName, "moveMagicDefenseBase") == 0) {
				mType->info.moveMagicDefenseBase = pugi::cast<int32_t>(attr.value());

			} else if (strcasecmp(attrName, "hasshiny") == 0) {
				mType->info.hasShiny = pugi::cast<int32_t>(attr.value());
			} else if (strcasecmp(attrName, "hasmega") == 0) {
				mType->info.hasMega = pugi::cast<int32_t>(attr.value());
			} else if (strcasecmp(attrName, "dexentry") == 0) {
				mType->info.dexEntry = pugi::cast<int32_t>(attr.value());
			} else if (strcasecmp(attrName, "portraitid") == 0) {
				mType->info.portraitId = pugi::cast<int32_t>(attr.value());


			} else if (strcasecmp(attrName, "illusionable") == 0) {
				mType->info.isIllusionable = attr.as_bool();
			} else if (strcasecmp(attrName, "convinceable") == 0) {
				mType->info.isConvinceable = attr.as_bool();
			} else if (strcasecmp(attrName, "pushable") == 0) {
				mType->info.pushable = attr.as_bool();
			} else if (strcasecmp(attrName, "canpushitems") == 0) {
				mType->info.canPushItems = attr.as_bool();
			} else if (strcasecmp(attrName, "canpushcreatures") == 0) {
				mType->info.canPushCreatures = attr.as_bool();
			} else if (strcasecmp(attrName, "staticattack") == 0) {
				uint32_t staticAttack = pugi::cast<uint32_t>(attr.value());
				if (staticAttack > 100) {
					std::cout << "[Warning - Monsters::loadMonster] staticattack greater than 100. " << file << std::endl;
					staticAttack = 100;
				}

				mType->info.staticAttackChance = staticAttack;
			} else if (strcasecmp(attrName, "lightlevel") == 0) {
				mType->info.light.level = pugi::cast<uint16_t>(attr.value());
			} else if (strcasecmp(attrName, "lightcolor") == 0) {
				mType->info.light.color = pugi::cast<uint16_t>(attr.value());
			} else if (strcasecmp(attrName, "targetdistance") == 0) {
				mType->info.targetDistance = std::max<int32_t>(1, pugi::cast<int32_t>(attr.value()));
			} else if (strcasecmp(attrName, "runonhealth") == 0) {
				mType->info.runAwayHealth = pugi::cast<int32_t>(attr.value());
			} else if (strcasecmp(attrName, "hidehealth") == 0) {
				mType->info.hiddenHealth = attr.as_bool();
			} else {
				std::cout << "[Warning - Monsters::loadMonster] Unknown flag attribute: " << attrName << ". " << file << std::endl;
			}
		}

		//if a monster can push creatures,
		// it should not be pushable
		if (mType->info.canPushCreatures) {
			mType->info.pushable = false;
		}
	}

	if ((node = monsterNode.child("targetchange"))) {
		if ((attr = node.attribute("speed")) || (attr = node.attribute("interval"))) {
			mType->info.changeTargetSpeed = pugi::cast<uint32_t>(attr.value());
		} else {
			std::cout << "[Warning - Monsters::loadMonster] Missing targetchange speed. " << file << std::endl;
		}

		if ((attr = node.attribute("chance"))) {
			mType->info.changeTargetChance = pugi::cast<int32_t>(attr.value());
		} else {
			std::cout << "[Warning - Monsters::loadMonster] Missing targetchange chance. " << file << std::endl;
		}
	}

	if ((node = monsterNode.child("look"))) {
		if ((attr = node.attribute("type"))) {
			mType->info.outfit.lookType = pugi::cast<uint16_t>(attr.value());

			if ((attr = node.attribute("head"))) {
				mType->info.outfit.lookHead = pugi::cast<uint16_t>(attr.value());
			}

			if ((attr = node.attribute("body"))) {
				mType->info.outfit.lookBody = pugi::cast<uint16_t>(attr.value());
			}

			if ((attr = node.attribute("legs"))) {
				mType->info.outfit.lookLegs = pugi::cast<uint16_t>(attr.value());
			}

			if ((attr = node.attribute("feet"))) {
				mType->info.outfit.lookFeet = pugi::cast<uint16_t>(attr.value());
			}

			if ((attr = node.attribute("addons"))) {
				mType->info.outfit.lookAddons = pugi::cast<uint16_t>(attr.value());
			}
		} else if ((attr = node.attribute("typeex"))) {
			mType->info.outfit.lookTypeEx = pugi::cast<uint16_t>(attr.value());
		} else {
			std::cout << "[Warning - Monsters::loadMonster] Missing look type/typeex. " << file << std::endl;
		}

		if ((attr = node.attribute("mount"))) {
			mType->info.outfit.lookMount = pugi::cast<uint16_t>(attr.value());
		}

		if ((attr = node.attribute("corpse"))) {
			mType->info.lookcorpse = pugi::cast<uint16_t>(attr.value());
		}
	}

	if ((node = monsterNode.child("attacks"))) {
		for (auto attackNode : node.children()) {
			spellBlock_t sb;
			if (deserializeSpell(attackNode, sb, monsterName)) {
				mType->info.attackSpells.emplace_back(std::move(sb));
			} else {
				std::cout << "[Warning - Monsters::loadMonster] Cant load spell. " << file << std::endl;
			}
		}
	}


	if ((node = monsterNode.child("moves"))) { //pota
		for (auto attackNode : node.children()) {
			spellBlock_t sb;
			if (deserializeSpell(attackNode, sb, monsterName)) {
				mType->info.moves.emplace_back(std::move(sb));
			} else {
				std::cout << "[Warning - Monsters::loadMonster] Cant load move spell. " << file << std::endl;
			}
		}
	}




	if ((node = monsterNode.child("evolutions"))) { //pota
		for (auto evolutionNode : node.children()) {
			int32_t level = 1000;
			int32_t love = 0;
			int32_t count = 100;
			std::string itemName = "none";

			if ((attr = evolutionNode.attribute("level"))) {
				level = std::max<int32_t>(1, pugi::cast<int32_t>(attr.value()));
			}

			if ((attr = evolutionNode.attribute("love"))) {
				love = pugi::cast<int32_t>(attr.value());
			}

			if ((attr = evolutionNode.attribute("count"))) {
				count = pugi::cast<uint32_t>(attr.value());
			}

			if ((attr = evolutionNode.attribute("itemName"))) {
				itemName = attr.as_string();
			}

			if ((attr = evolutionNode.attribute("name"))) {
				evolutionBlock_t eb;
				eb.name = attr.as_string();
				eb.itemName = itemName;
				eb.level = level;
				eb.love = love;
				eb.count = count;
				mType->info.evolutions.emplace_back(eb);
			} else {
				std::cout << "[Warning - Monsters::loadMonster] Missing evolution name. " << file << std::endl;
			}
		}
	}





	if ((node = monsterNode.child("defenses"))) {
		if ((attr = node.attribute("defense"))) {
			mType->info.defense = pugi::cast<int32_t>(attr.value());
		}

		if ((attr = node.attribute("armor"))) {
			mType->info.armor = pugi::cast<int32_t>(attr.value());
		}

		for (auto defenseNode : node.children()) {
			spellBlock_t sb;
			if (deserializeSpell(defenseNode, sb, monsterName)) {
				mType->info.defenseSpells.emplace_back(std::move(sb));
			} else {
				std::cout << "[Warning - Monsters::loadMonster] Cant load spell. " << file << std::endl;
			}
		}
	}

	if ((node = monsterNode.child("immunities"))) {
		for (auto immunityNode : node.children()) {
			if ((attr = immunityNode.attribute("name"))) {
				std::string tmpStrValue = asLowerCaseString(attr.as_string());
				if (tmpStrValue == "physical") {
					mType->info.damageImmunities |= COMBAT_PHYSICALDAMAGE;
					mType->info.conditionImmunities |= CONDITION_BLEEDING;
				} else if (tmpStrValue == "energy") {
					mType->info.damageImmunities |= COMBAT_ENERGYDAMAGE;
					mType->info.conditionImmunities |= CONDITION_ENERGY;
				} else if (tmpStrValue == "fire") {
					mType->info.damageImmunities |= COMBAT_FIREDAMAGE;
					mType->info.conditionImmunities |= CONDITION_FIRE;
				} else if (tmpStrValue == "poison") {
					mType->info.damageImmunities |= COMBAT_POISONDAMAGE;
					mType->info.conditionImmunities |= CONDITION_POISON;
				} else if (tmpStrValue == "drown") {
					mType->info.damageImmunities |= COMBAT_DROWNDAMAGE;
					mType->info.conditionImmunities |= CONDITION_DROWN;
				} else if (tmpStrValue == "ice") {
					mType->info.damageImmunities |= COMBAT_ICEDAMAGE;
					mType->info.conditionImmunities |= CONDITION_FREEZING;
				} else if (tmpStrValue == "holy") {
					mType->info.damageImmunities |= COMBAT_HOLYDAMAGE;
					mType->info.conditionImmunities |= CONDITION_DAZZLED;
				} else if (tmpStrValue == "death") {
					mType->info.damageImmunities |= COMBAT_DEATHDAMAGE;
					mType->info.conditionImmunities |= CONDITION_CURSED;
				} else if (tmpStrValue == "psychic") { //pota
					mType->info.damageImmunities |= COMBAT_PSYCHICDAMAGE;
				} else if (tmpStrValue == "grass") { //pota
					mType->info.damageImmunities |= COMBAT_GRASSDAMAGE;
				} else if (tmpStrValue == "normal") { //pota
					mType->info.damageImmunities |= COMBAT_NORMALDAMAGE;
				} else if (tmpStrValue == "water") { //pota
					mType->info.damageImmunities |= COMBAT_WATERDAMAGE;
				} else if (tmpStrValue == "flying") { //pota
					mType->info.damageImmunities |= COMBAT_FLYINGDAMAGE;
				} else if (tmpStrValue == "poison") { //pota
					mType->info.damageImmunities |= COMBAT_POISONDAMAGE;
				} else if (tmpStrValue == "electric") { //pota
					mType->info.damageImmunities |= COMBAT_ELECTRICDAMAGE;
				} else if (tmpStrValue == "ground") { //pota
					mType->info.damageImmunities |= COMBAT_GROUNDDAMAGE;
				} else if (tmpStrValue == "rock") { //pota
					mType->info.damageImmunities |= COMBAT_ROCKDAMAGE;
				} else if (tmpStrValue == "bug") { //pota
					mType->info.damageImmunities |= COMBAT_BUGDAMAGE;
				} else if (tmpStrValue == "dragon") { //pota
					mType->info.damageImmunities |= COMBAT_DRAGONDAMAGE;
				} else if (tmpStrValue == "ghost") { //pota
					mType->info.damageImmunities |= COMBAT_GHOSTDAMAGE;
				} else if (tmpStrValue == "dark") { //pota
					mType->info.damageImmunities |= COMBAT_DARKDAMAGE;
				} else if (tmpStrValue == "steel") { //pota
					mType->info.damageImmunities |= COMBAT_STEELDAMAGE;
				} else if (tmpStrValue == "fairy") { //pota
					mType->info.damageImmunities |= COMBAT_FAIRYDAMAGE;
				} else if (tmpStrValue == "fighting") { //pota
					mType->info.damageImmunities |= COMBAT_FIGHTINGDAMAGE;
				} else if (tmpStrValue == "lifedrain") {
					mType->info.damageImmunities |= COMBAT_LIFEDRAIN;
				} else if (tmpStrValue == "manadrain") {
					mType->info.damageImmunities |= COMBAT_MANADRAIN;
				} else if (tmpStrValue == "paralyze") {
					mType->info.conditionImmunities |= CONDITION_PARALYZE;
				} else if (tmpStrValue == "sleep") { //pota
					mType->info.conditionImmunities |= CONDITION_SLEEP;
				} else if (tmpStrValue == "sleep") { //helena
					mType->info.conditionImmunities |= CONDITION_SILENCE;
				} else if (tmpStrValue == "outfit") {
					mType->info.conditionImmunities |= CONDITION_OUTFIT;
				} else if (tmpStrValue == "drunk") {
					mType->info.conditionImmunities |= CONDITION_DRUNK;
				} else if (tmpStrValue == "invisible" || tmpStrValue == "invisibility") {
					mType->info.conditionImmunities |= CONDITION_INVISIBLE;
				} else if (tmpStrValue == "bleed") {
					mType->info.conditionImmunities |= CONDITION_BLEEDING;
				} else {
					std::cout << "[Warning - Monsters::loadMonster] Unknown immunity name " << attr.as_string() << ". " << file << std::endl;
				}
			} else if ((attr = immunityNode.attribute("physical"))) {
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_PHYSICALDAMAGE;
					mType->info.conditionImmunities |= CONDITION_BLEEDING;
				}
			} else if ((attr = immunityNode.attribute("energy"))) {
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_ENERGYDAMAGE;
					mType->info.conditionImmunities |= CONDITION_ENERGY;
				}
			} else if ((attr = immunityNode.attribute("fire"))) {
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_FIREDAMAGE;
					mType->info.conditionImmunities |= CONDITION_FIRE;
				}
			} else if ((attr = immunityNode.attribute("poison"))) {
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_POISONDAMAGE;
					mType->info.conditionImmunities |= CONDITION_POISON;
				}
			} else if ((attr = immunityNode.attribute("drown"))) {
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_DROWNDAMAGE;
					mType->info.conditionImmunities |= CONDITION_DROWN;
				}
			} else if ((attr = immunityNode.attribute("ice"))) {
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_ICEDAMAGE;
					mType->info.conditionImmunities |= CONDITION_FREEZING;
				}
			} else if ((attr = immunityNode.attribute("holy"))) {
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_HOLYDAMAGE;
					mType->info.conditionImmunities |= CONDITION_DAZZLED;
				}
			} else if ((attr = immunityNode.attribute("death"))) {
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_DEATHDAMAGE;
					mType->info.conditionImmunities |= CONDITION_CURSED;
				}
			} else if ((attr = immunityNode.attribute("psychic"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_PSYCHICDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("grass"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_GRASSDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("normal"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_NORMALDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("water"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_WATERDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("flying"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_FLYINGDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("poison"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_POISONDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("electric"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_ELECTRICDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("ground"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_GROUNDDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("rock"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_ROCKDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("bug"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_BUGDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("dragon"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_DRAGONDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("ghost"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_GHOSTDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("dark"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_DARKDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("steel"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_STEELDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("fairy"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_FAIRYDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("fighting"))) { //pota
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_FIGHTINGDAMAGE;
				}
			} else if ((attr = immunityNode.attribute("lifedrain"))) {
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_LIFEDRAIN;
				}
			} else if ((attr = immunityNode.attribute("manadrain"))) {
				if (attr.as_bool()) {
					mType->info.damageImmunities |= COMBAT_MANADRAIN;
				}
			} else if ((attr = immunityNode.attribute("paralyze"))) {
				if (attr.as_bool()) {
					mType->info.conditionImmunities |= CONDITION_PARALYZE;
				}
			} else if ((attr = immunityNode.attribute("sleep"))) { //pota
				if (attr.as_bool()) {
					mType->info.conditionImmunities |= CONDITION_SLEEP;
				}
			} else if ((attr = immunityNode.attribute("silence"))) { //helena
				if (attr.as_bool()) {
					mType->info.conditionImmunities |= CONDITION_SILENCE;
				}
				
			} else if ((attr = immunityNode.attribute("outfit"))) {
				if (attr.as_bool()) {
					mType->info.conditionImmunities |= CONDITION_OUTFIT;
				}
			} else if ((attr = immunityNode.attribute("bleed"))) {
				if (attr.as_bool()) {
					mType->info.conditionImmunities |= CONDITION_BLEEDING;
				}
			} else if ((attr = immunityNode.attribute("drunk"))) {
				if (attr.as_bool()) {
					mType->info.conditionImmunities |= CONDITION_DRUNK;
				}
			} else if ((attr = immunityNode.attribute("invisible")) || (attr = immunityNode.attribute("invisibility"))) {
				if (attr.as_bool()) {
					mType->info.conditionImmunities |= CONDITION_INVISIBLE;
				}
			} else {
				std::cout << "[Warning - Monsters::loadMonster] Unknown immunity. " << file << std::endl;
			}
		}
	}

	if ((node = monsterNode.child("voices"))) {
		if ((attr = node.attribute("speed")) || (attr = node.attribute("interval"))) {
			mType->info.yellSpeedTicks = pugi::cast<uint32_t>(attr.value());
		} else {
			std::cout << "[Warning - Monsters::loadMonster] Missing voices speed. " << file << std::endl;
		}

		if ((attr = node.attribute("chance"))) {
			mType->info.yellChance = pugi::cast<uint32_t>(attr.value());
		} else {
			std::cout << "[Warning - Monsters::loadMonster] Missing voices chance. " << file << std::endl;
		}

		for (auto voiceNode : node.children()) {
			voiceBlock_t vb;
			if ((attr = voiceNode.attribute("sentence"))) {
				vb.text = attr.as_string();
			} else {
				std::cout << "[Warning - Monsters::loadMonster] Missing voice sentence. " << file << std::endl;
			}

			if ((attr = voiceNode.attribute("yell"))) {
				vb.yellText = attr.as_bool();
			} else {
				vb.yellText = false;
			}
			mType->info.voiceVector.emplace_back(vb);
		}
	}

	if ((node = monsterNode.child("loot"))) {
		for (auto lootNode : node.children()) {
			LootBlock lootBlock;
			if (loadLootItem(lootNode, lootBlock)) {
				mType->info.lootItems.emplace_back(std::move(lootBlock));
			} else {
				std::cout << "[Warning - Monsters::loadMonster] Cant load loot. " << file << std::endl;
			}
		}
	}

	if ((node = monsterNode.child("elements"))) {
		for (auto elementNode : node.children()) {
			if ((attr = elementNode.attribute("physicalPercent"))) {
				mType->info.elementMap[COMBAT_PHYSICALDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("icePercent"))) {
				mType->info.elementMap[COMBAT_ICEDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("poisonPercent")) || (attr = elementNode.attribute("poisonPercent"))) {
				mType->info.elementMap[COMBAT_POISONDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("firePercent"))) {
				mType->info.elementMap[COMBAT_FIREDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("energyPercent"))) {
				mType->info.elementMap[COMBAT_ENERGYDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("holyPercent"))) {
				mType->info.elementMap[COMBAT_HOLYDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("deathPercent"))) {
				mType->info.elementMap[COMBAT_DEATHDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("psychicPercent"))) { //pota
				mType->info.elementMap[COMBAT_PSYCHICDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("grassPercent"))) { //pota
				mType->info.elementMap[COMBAT_GRASSDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("normalPercent"))) { //pota
				mType->info.elementMap[COMBAT_NORMALDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("waterPercent"))) { //pota
				mType->info.elementMap[COMBAT_WATERDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("flyingPercent"))) { //pota
				mType->info.elementMap[COMBAT_FLYINGDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("poisonPercent"))) { //pota
				mType->info.elementMap[COMBAT_POISONDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("electricPercent"))) { //pota
				mType->info.elementMap[COMBAT_ELECTRICDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("groundPercent"))) { //pota
				mType->info.elementMap[COMBAT_GROUNDDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("rockPercent"))) { //pota
				mType->info.elementMap[COMBAT_ROCKDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("bugPercent"))) { //pota
				mType->info.elementMap[COMBAT_BUGDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("dragonPercent"))) { //pota
				mType->info.elementMap[COMBAT_DRAGONDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("ghostPercent"))) { //pota
				mType->info.elementMap[COMBAT_GHOSTDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("darkPercent"))) { //pota
				mType->info.elementMap[COMBAT_DARKDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("steelPercent"))) { //pota
				mType->info.elementMap[COMBAT_STEELDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("fairyPercent"))) { //pota
				mType->info.elementMap[COMBAT_FAIRYDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("fightingPercent"))) { //pota
				mType->info.elementMap[COMBAT_FIGHTINGDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("drownPercent"))) {
				mType->info.elementMap[COMBAT_DROWNDAMAGE] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("lifedrainPercent"))) {
				mType->info.elementMap[COMBAT_LIFEDRAIN] = pugi::cast<int32_t>(attr.value());
			} else if ((attr = elementNode.attribute("manadrainPercent"))) {
				mType->info.elementMap[COMBAT_MANADRAIN] = pugi::cast<int32_t>(attr.value());
			} else {
				std::cout << "[Warning - Monsters::loadMonster] Unknown element percent. " << file << std::endl;
			}
		}
	}

	if ((node = monsterNode.child("summons"))) {
		if ((attr = node.attribute("maxSummons"))) {
			mType->info.maxSummons = std::min<uint32_t>(pugi::cast<uint32_t>(attr.value()), 100);
		} else {
			std::cout << "[Warning - Monsters::loadMonster] Missing summons maxSummons. " << file << std::endl;
		}

		for (auto summonNode : node.children()) {
			int32_t chance = 100;
			int32_t speed = 1000;
			int32_t max = mType->info.maxSummons;
			bool force = false;

			if ((attr = summonNode.attribute("speed")) || (attr = summonNode.attribute("interval"))) {
				speed = std::max<int32_t>(1, pugi::cast<int32_t>(attr.value()));
			}

			if ((attr = summonNode.attribute("chance"))) {
				chance = pugi::cast<int32_t>(attr.value());
			}

			if ((attr = summonNode.attribute("max"))) {
				max = pugi::cast<uint32_t>(attr.value());
			}

			if ((attr = summonNode.attribute("force"))) {
				force = attr.as_bool();
			}

			if ((attr = summonNode.attribute("name"))) {
				summonBlock_t sb;
				sb.name = attr.as_string();
				sb.speed = speed;
				sb.chance = chance;
				sb.max = max;
				sb.force = force;
				mType->info.summons.emplace_back(sb);
			} else {
				std::cout << "[Warning - Monsters::loadMonster] Missing summon name. " << file << std::endl;
			}
		}
	}

	if ((node = monsterNode.child("script"))) {
		for (auto eventNode : node.children()) {
			if ((attr = eventNode.attribute("name"))) {
				mType->info.scripts.emplace_back(attr.as_string());
			} else {
				std::cout << "[Warning - Monsters::loadMonster] Missing name for script event. " << file << std::endl;
			}
		}
	}

	mType->info.summons.shrink_to_fit();
	mType->info.lootItems.shrink_to_fit();
	mType->info.attackSpells.shrink_to_fit();
	mType->info.defenseSpells.shrink_to_fit();
	mType->info.voiceVector.shrink_to_fit();
	mType->info.scripts.shrink_to_fit();

	mType->info.moves.shrink_to_fit(); //pota
	mType->info.evolutions.shrink_to_fit(); //pota

	return true;
}

bool Monsters::loadLootItem(const pugi::xml_node& node, LootBlock& lootBlock)
{
	pugi::xml_attribute attr;
	if ((attr = node.attribute("id"))) {
		lootBlock.id = pugi::cast<int32_t>(attr.value());
	} else if ((attr = node.attribute("name"))) {
		auto name = attr.as_string();
		auto ids = Item::items.nameToItems.equal_range(asLowerCaseString(name));

		if (ids.first == Item::items.nameToItems.cend()) {
			std::cout << "[Warning - Monsters::loadMonster] Unknown loot item \"" << name << "\". " << std::endl;
			return false;
		}

		uint32_t id = ids.first->second;

		if (std::next(ids.first) != ids.second) {
			std::cout << "[Warning - Monsters::loadMonster] Non-unique loot item \"" << name << "\". " << std::endl;
			return false;
		}

		lootBlock.id = id;
	}

	if (lootBlock.id == 0) {
		return false;
	}

	if ((attr = node.attribute("countmax"))) {
		lootBlock.countmax = std::max<int32_t>(1, pugi::cast<int32_t>(attr.value()));
	} else {
		lootBlock.countmax = 1;
	}

	if ((attr = node.attribute("chance")) || (attr = node.attribute("chance1"))) {
		lootBlock.chance = std::min<int32_t>(MAX_LOOTCHANCE, pugi::cast<int32_t>(attr.value()));
	} else {
		lootBlock.chance = MAX_LOOTCHANCE;
	}

	if (Item::items[lootBlock.id].isContainer()) {
		loadLootContainer(node, lootBlock);
	}

	//optional
	if ((attr = node.attribute("subtype"))) {
		lootBlock.subType = pugi::cast<int32_t>(attr.value());
	} else {
		uint32_t charges = Item::items[lootBlock.id].charges;
		if (charges != 0) {
			lootBlock.subType = charges;
		}
	}

	if ((attr = node.attribute("actionId"))) {
		lootBlock.actionId = pugi::cast<int32_t>(attr.value());
	}

	if ((attr = node.attribute("text"))) {
		lootBlock.text = attr.as_string();
	}
	return true;
}

void Monsters::loadLootContainer(const pugi::xml_node& node, LootBlock& lBlock)
{
	for (auto subNode : node.children()) {
		LootBlock lootBlock;
		if (loadLootItem(subNode, lootBlock)) {
			lBlock.childLoot.emplace_back(std::move(lootBlock));
		}
	}
}

MonsterType* Monsters::getMonsterType(const std::string& name)
{
	auto it = monsters.find(asLowerCaseString(name));

	if (it == monsters.end()) {
		return nullptr;
	}
	return &it->second;
}
