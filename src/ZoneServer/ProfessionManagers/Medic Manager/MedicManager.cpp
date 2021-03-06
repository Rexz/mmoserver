/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2014 The SWG:ANH Team
---------------------------------------------------------------------------------------
Use of this source code is governed by the GPL v3 license that can be found
in the COPYING file or at http://www.gnu.org/licenses/gpl-3.0.html

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
---------------------------------------------------------------------------------------
*/
#include <cstdint>

#define NOMINMAX
#include <algorithm>

#include <boost/regex.hpp>

#include "Zoneserver/ProfessionManagers/Medic Manager/MedicManager.h"
#include "ZoneServer/ProfessionManagers/Medic Manager/InjuryTreatmentEvent.h"
#include "ZoneServer/ProfessionManagers/Medic Manager/WoundTreatmentEvent.h"
#include "ZoneServer/ProfessionManagers/Medic Manager/QuickHealInjuryTreatmentEvent.h"
#include "Zoneserver/Objects/Inventory.h"
#include "ZoneServer/Objects/Medicine.h"
#include "ZoneServer/ObjectController/ObjectControllerCommandMap.h"
#include "ZoneServer/ObjectController/ObjectControllerOpcodes.h"
#include "ZoneServer/Objects/Player Object/PlayerObject.h"

#include "ZoneServer/GameSystemManagers/UI Manager/UIManager.h"
#include "ZoneServer/GameSystemManagers/Container Manager/ContainerManager.h"
#include "ZoneServer/GameSystemManagers/Forage Manager/ForageManager.h"
#include "ZoneServer/GameSystemManagers/Structure Manager/StructureManager.h"

#include "ZoneServer\Services\equipment\equipment_service.h"
#include "ZoneServer/WorldManager.h"
#include "ZoneServer/WorldConfig.h"

#include "NetworkManager/Message.h"
#include "MessageLib/MessageLib.h"
#include "anh/Utils/rand.h"

#include "ZoneServer\Services\ham\ham_service.h"


using boost::regex;
using boost::smatch;
using boost::regex_search;
using boost::sregex_token_iterator;


bool			MedicManager::mInsFlag = false;
MedicManager*	MedicManager::mSingleton = NULL;

//consts
const char* const woundpack = "woundpack";
const char* const stim = "stim";
const char* const rangedstim = "ranged";
const char* const self = "self";
const char* const action = "action";
const char* const constitution = "constitution";
const char* const health = "health";
const char* const quickness = "quickness";
const char* const stamina = "stamina";
const char* const strength = "strength";



MedicManager::MedicManager(swganh::app::SwganhKernel*	kernel)
{
    kernel_ = kernel;
}


MedicManager::~MedicManager()
{
}


bool MedicManager::Diagnose(PlayerObject* Medic, PlayerObject* Patient)
{
    //TODO: Allow Pet Diagnosis

    if(!Medic->verifyAbility(opOCdiagnose))
    {
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "cannot_heal"), Medic);
        return false;
    }


    float distance = gWorldConfig->getConfiguration("Player_heal_distance",(float)6.0);

	if(glm::distance(Medic->GetCreature()->mPosition, Patient->GetCreature()->mPosition) > distance)
    {
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_b7"), Medic);
        return false;
    }
	StringVector attributesMenu;

	auto ham = kernel_->GetServiceManager()->GetService<swganh::ham::HamService>("HamService");

	int8 Health[32];
	sprintf(Health,"Health -- %i",Patient->GetCreature()->GetStatWound(HamBar_Health));
    attributesMenu.push_back(Health);

    int8 Strength[32];
    sprintf(Strength,"Strength -- %i",Patient->GetCreature()->GetStatWound(HamBar_Strength));
    attributesMenu.push_back(Strength);

    int8 Constitution[32];
    sprintf(Constitution,"Constitution -- %i",Patient->GetCreature()->GetStatWound(HamBar_Constitution));
    attributesMenu.push_back(Constitution);

    int8 Action[32];
    sprintf(Action,"Action -- %i",Patient->GetCreature()->GetStatWound(HamBar_Action));
    attributesMenu.push_back(Action);

    int8 Quickness[32];
    sprintf(Quickness,"Quickness -- %i",Patient->GetCreature()->GetStatWound(HamBar_Quickness));
    attributesMenu.push_back(Quickness);

    int8 Stamina[32];
    sprintf(Stamina,"Stamina -- %i",Patient->GetCreature()->GetStatWound(HamBar_Stamina));
    attributesMenu.push_back(Stamina);

    int8 Mind[32];
    sprintf(Mind,"Mind -- %i",Patient->GetCreature()->GetStatWound(HamBar_Mind));
    attributesMenu.push_back(Mind);

    int8 Focus[32];
    sprintf(Focus,"Focus -- %i",Patient->GetCreature()->GetStatWound(HamBar_Focus));
    attributesMenu.push_back(Focus);

    int8 Willpower[32];
    sprintf(Willpower,"Willpower -- %i",Patient->GetCreature()->GetStatWound(HamBar_Willpower));
    attributesMenu.push_back(Willpower);

    int8 BattleFatigue[32];
	sprintf(BattleFatigue,"Battle Fatigue -- %i",Patient->GetCreature()->GetBattleFatigue());
    attributesMenu.push_back(BattleFatigue);



    gUIManager->createNewDiagnoseListBox(Medic, Medic, Patient, attributesMenu);
    return true;
}

bool MedicManager::CheckMedicine(PlayerObject* Medic, PlayerObject* Target, ObjectControllerCmdProperties* cmdProperties, std::string medpackType)
{
    //This determines what type of medicine the player is using
    bool wound = false;
    uint32 opcode = 0;
    if (medpackType == action || medpackType == constitution || medpackType == health || medpackType == quickness ||
            medpackType == stamina || medpackType == strength || medpackType == woundpack)
        opcode = opOChealwound;
    else if (medpackType == rangedstim)
    {
        // CM
        //uint32 opcode == opOChealdamagerange;
    }
    else
        opcode = opOChealdamage;


    Medicine* medicine = NULL;

    uint64 MedicinePackObjectID = 0;

    //If we don't have an OC Controller Cmd Property (ie we have been called by using an item) - go get one
    if(cmdProperties == 0)
    {
        CmdPropertyMap::iterator it = gObjControllerCmdPropertyMap.find(opcode);

        if(it == gObjControllerCmdPropertyMap.end())
        {
            //Cannot find properties
            return false;
        } else {
            cmdProperties = ((*it).second);
        }
    }

    //If we weren't triggered by a stim ie. from a command
    if(MedicinePackObjectID == 0)
    {

        //TODO have an automated function that looks for the first item of a certain type in ALL containers

        //Look through inventory to find the correct MedicinePack
        auto inventory = gWorldManager->getKernel()->GetServiceManager()->GetService<swganh::equipment::EquipmentService>("EquipmentService")->GetEquippedObject(Medic, "inventory");
		inventory->ViewObjects(Medic, 0, true, [&] (Object* object) {
			Item* item = dynamic_cast<Item*>(object);
			if(!item || medicine)	{
				return;
			}
			
			//ItemType
			uint32 mItemType = item->getItemType();
			//check the opCode to see which medicine we need ??
			if (medpackType == stim)
			{
				switch(mItemType)
				{
				case ItemType_Stimpack_A:
				case ItemType_Stimpack_B:
				case ItemType_Stimpack_C:
				case ItemType_Stimpack_D:
				case ItemType_Stimpack_E:
					MedicinePackObjectID = item->getId();
					medicine = dynamic_cast<Medicine*>(item);
				default:
					break;
				}
			}
			else if (medpackType == rangedstim)
			{
				switch(mItemType)
				{
				case ItemType_Ranged_Stimpack_A:
				case ItemType_Ranged_Stimpack_B:
				case ItemType_Ranged_Stimpack_C:
				case ItemType_Ranged_Stimpack_D:
				case ItemType_Ranged_Stimpack_E:
					MedicinePackObjectID = item->getId();
					medicine = dynamic_cast<Medicine*>(item);
				default:
					break;
				}
			}
			else if (medpackType == action )
			{
				switch(mItemType)
				{
					//action
				case ItemType_Wound_Action_A:
				case ItemType_Wound_Action_B:
				case ItemType_Wound_Action_C:
				case ItemType_Wound_Action_D:
				case ItemType_Wound_Action_E:
					MedicinePackObjectID = item->getId();
					medicine = dynamic_cast<Medicine*>(item);
					wound = true;
				default:
					break;
				}
			}
			else if	(medpackType == constitution)
			{
				switch(mItemType)
				{
					//constitution
				case ItemType_Wound_Constitution_A:
				case ItemType_Wound_Constitution_B:
				case ItemType_Wound_Constitution_C:
				case ItemType_Wound_Constitution_D:
				case ItemType_Wound_Constitution_E:
					MedicinePackObjectID = item->getId();
					medicine = dynamic_cast<Medicine*>(item);
					wound = true;
				default:
					break;
				}
			}
			else if (medpackType == health)
			{
				switch(mItemType)
				{
					// health
				case ItemType_Wound_Health_A:
				case ItemType_Wound_Health_B:
				case ItemType_Wound_Health_C:
				case ItemType_Wound_Health_D:
				case ItemType_Wound_Health_E:
					MedicinePackObjectID = item->getId();
					medicine = dynamic_cast<Medicine*>(item);
					wound = true;
				default:
					break;
				}
			}
			else if (medpackType == quickness)
			{
				switch(mItemType)
				{
					// quickness
				case ItemType_Wound_Quickness_A:
				case ItemType_Wound_Quickness_B:
				case ItemType_Wound_Quickness_C:
				case ItemType_Wound_Quickness_D:
				case ItemType_Wound_Quickness_E:
					MedicinePackObjectID = item->getId();
					medicine = dynamic_cast<Medicine*>(item);
					wound = true;
				default:
					break;
				}
			}
			else if (medpackType == stamina)
			{
				switch(mItemType)
				{
					// stamina
				case ItemType_Wound_Stamina_A:
				case ItemType_Wound_Stamina_B:
				case ItemType_Wound_Stamina_C:
				case ItemType_Wound_Stamina_D:
				case ItemType_Wound_Stamina_E:
					MedicinePackObjectID = item->getId();
					medicine = dynamic_cast<Medicine*>(item);
					wound = true;
				default:
					break;
				}
			}
			else if (medpackType == strength)
			{
				switch(mItemType)
				{
					// strength
				case ItemType_Wound_Strength_A:
				case ItemType_Wound_Strength_B:
				case ItemType_Wound_Strength_C:
				case ItemType_Wound_Strength_D:
				case ItemType_Wound_Strength_E:
					MedicinePackObjectID = item->getId();
					medicine = dynamic_cast<Medicine*>(item);
					wound = true;
				default:
					break;
				}
			}
			else
			{
				DLOG(info) << "Invalid Medicine Type" ;
			}

		
			
    });
    //Check if a Stim was found
    if(medicine == 0)
    {
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_60"), Medic);
        return false;
    }
    } else
    {
        medicine = dynamic_cast<Medicine*>(gWorldManager->getObjectById(MedicinePackObjectID));
    }

    //Is the medicine suitable for skill level
    uint64 medicSkill;
    uint64 req = medicine->getSkillRequired("healing_ability");
    if (wound)
    {
        medicSkill = Medic->GetCreature()->getSkillModValue(SMod_healing_wound_treatment);
    }
    else
    {
        medicSkill = Medic->GetCreature()->getSkillModValue(SMod_healing_ability);
    };

    if(medicSkill < req)
    {
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "insufficient_skill_heal", L"", L"", L"healingskill", static_cast<int32_t>(req)), Medic);
        return false;
    }

    if (medpackType == rangedstim)
        return HealDamageRanged(Medic, Target, MedicinePackObjectID, cmdProperties);


    if (medpackType == stim)
        return HealDamage(Medic, Target, MedicinePackObjectID, cmdProperties, stim);


    if (wound)
        return HealWound(Medic, Target, MedicinePackObjectID, cmdProperties, medpackType);

    return false;
}

bool MedicManager::HealDamage(PlayerObject* Medic, PlayerObject* Target, uint64 StimPackObjectID, ObjectControllerCmdProperties* cmdProperties, std::string healType)
{
    Medicine* Stim = dynamic_cast<Medicine*>(gWorldManager->getObjectById(StimPackObjectID));
    bool isSelf = (Medic->getId() == Target->getId());
    bool tendDamage = (healType.find("tendDamage") != std::string::npos);
    bool quickHeal = (healType.find("quickHeal") != std::string::npos);

    //Get Medic Skill Mods
    uint32 healingskill = Medic->GetCreature()->getSkillModValue(SMod_healing_injury_treatment);

    //If Currently in Delay Period
    if(!quickHeal && Medic->checkPlayerCustomFlag(PlayerCustomFlag_InjuryTreatment))
    {
        //Say you can't heal yet.
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_must_wait"), Medic);
        return false;
    }
    //quickHeal is on a seperate cooldown
    else if (quickHeal && Medic->checkPlayerCustomFlag(PlayerCustomFlag_QuickHealInjuryTreatment))
    {
        //Say you can't heal yet.
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_must_wait"), Medic);
        return false;
    }

    if (!MedicManager::CheckMedicRange(Medic, Target, (float)6.0))
    {
        return false;
    }

    //If we don't have an OC Controller Cmd Property (ie we have been called by using an item) - go get one
    if(cmdProperties == 0)
    {
        CmdPropertyMap::iterator it = gObjControllerCmdPropertyMap.find(opOChealdamage);

        if(it == gObjControllerCmdPropertyMap.end())
        {
            //Cannot find properties
            return false;
        } else {
            cmdProperties = ((*it).second);
        }
    }

    if (!CheckMedicRange(Medic, Target, (float)6.0))
        return false;

    //Does Medic have ability
    if(!Medic->verifyAbility(cmdProperties->mAbilityCrc))
    {
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "cannot_heal"), Medic);
        return false;
    }

    //Does Target Need Healing take into account wounds
	auto ham = kernel_->GetServiceManager()->GetService<swganh::ham::HamService>("HamService");

	int TargetHealth = Target->GetCreature()->GetStatCurrent(HamBar_Health);
    int TargetAction = Target->GetCreature()->GetStatCurrent(HamBar_Action);
	int TargetMaxHealth = ham->getModifiedHitPoints(Target->GetCreature(), HamBar_Health); 
    int TargetMaxAction = ham->getModifiedHitPoints(Target->GetCreature(), HamBar_Action); 

    if(!(TargetHealth < TargetMaxHealth))
    {
        if(!(TargetAction < TargetMaxAction))
        {
            if (isSelf) {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "no_damage_to_heal_self"), Medic);
                return false;
            }
            if (!isSelf) {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "no_damage_to_heal_target", 0, Target->getId(), 0), Medic);
                return false;
            }
        }
    }


    //Get Heal Strength
    //TODO - BEClothing, and Med Center/city bonuses.
    int healthpower = 0;
    int actionpower = 0;
    if (tendDamage)
    {
        //random range between 75 and 100
        healthpower =	gRandom->getRand()%(100-75+1)+75;
        actionpower =	gRandom->getRand()%(100-75+1)+75;
    }
    else if (quickHeal)
    {
        healthpower = gRandom->getRand()%(1000-400+1)+400;
        actionpower = gRandom->getRand()%(1000-400+1)+400;
    }
    //heal damage
    else
    {
        healthpower = Stim->getHealthHeal();
        actionpower = Stim->getActionHeal();
    }

    //uint BEClothes = NULL;
    //uint MedCityBonus = NULL;
    uint maxhealhealth = healthpower * ((100 + healingskill) / 100);
    uint maxhealaction = actionpower * ((100 + healingskill) / 100);


    //Adjust for Target BF
    maxhealhealth = MedicManager::CalculateBF(Medic, Target, maxhealhealth);
    maxhealaction = MedicManager::CalculateBF(Medic, Target, maxhealaction);

    int StrengthHealth = std::min((int)maxhealhealth, TargetMaxHealth-TargetHealth);
    int StrengthAction = std::min((int)maxhealaction, TargetMaxAction-TargetAction);

    //Cost.
    int cost = 140;
    if (tendDamage)
        cost = 500;
    else if (quickHeal)
        cost = 1000;
	
	if (!ham->checkMainPool(Medic->GetCreature(), HamBar_Mind, cost)) {
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "not_enough_mind"), Medic);
        return false;
    }

	ham->UpdateCurrentHitpoints(Medic->GetCreature(), HamBar_Mind, -cost);
    
	ham->UpdateCurrentHitpoints(Target->GetCreature(), HamBar_Health, StrengthHealth);
	ham->UpdateCurrentHitpoints(Target->GetCreature(), HamBar_Action, StrengthAction);
    

    //Add XP as Total Heal / 4 if not targetting self
    if(!isSelf && !tendDamage && !quickHeal)
        gSkillManager->addExperience(XpType_medical, (int)((StrengthHealth + StrengthAction)/4), Medic);

    if(isSelf) //If targetting self
    {
        if(StrengthHealth > 0)
        {
            if(StrengthAction > 0)
            {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_01", 0, 0, 0, StrengthHealth), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_04", 0, 0, 0, StrengthAction), Medic);
            } else {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_10", 0, 0, 0, StrengthHealth), Medic);
            }
        } else {
            if(StrengthAction > 0)
            {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_11", 0, 0, 0, StrengthAction), Medic);
            }
        }

        //Anim
        gMessageLib->sendCreatureAnimation(Medic->GetCreature(), BString("heal_self"));

        //CE
        gMessageLib->sendPlayClientEffectLocMessage("clienteffect/healing_healdamage.cef",Medic->mPosition,Medic);

    }
    else
    {   //if targetting something else
        if(StrengthHealth > 0)
        {
            if(StrengthAction > 0)
            {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_06", 0, Target->getId(), 0, StrengthHealth), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_04", 0, 0, 0, StrengthAction), Medic);


                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_08", 0, 0, Medic->getId(), StrengthHealth), Target);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_04", 0, 0, 0, StrengthAction), Target);
            } else {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_13", 0, Target->getId(), 0, StrengthHealth), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_16", 0, 0, Medic->getId(), StrengthHealth), Target);
            }
        }
        else
        {
            if(StrengthAction > 0)
            {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_14", 0, Target->getId(), 0, StrengthAction), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_17", 0, 0, Medic->getId(), StrengthAction), Target);
            }
        }
        //Anim
        gMessageLib->sendCreatureAnimation(Medic->GetCreature(), BString("heal_other"));

        //CE
        gMessageLib->sendPlayClientEffectLocMessage("clienteffect/healing_healdamage.cef",Target->mPosition,Target);
    }

    if((!tendDamage && !quickHeal) && Stim->ConsumeUse(Medic))
    {
        TangibleObject* container = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById(Stim->getParentId()));
		gContainerManager->deleteObject(Stim, container);
    }

	gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_62"), Medic);
    return true;
}

bool MedicManager::HealDamageRanged(PlayerObject* Medic, PlayerObject* Target, uint64 StimPackObjectID, ObjectControllerCmdProperties* cmdProperties)
{
    bool isSelf = false;
    Medicine* Stim = dynamic_cast<Medicine*>(gWorldManager->getObjectById(StimPackObjectID));

    isSelf = (Medic->getId() == Target->getId());

    //Get Medic Skill Mods
    uint32 healingskill = Medic->GetCreature()->getSkillModValue(SMod_healing_range);

    //If Currently in Delay Period

    if(Medic->checkPlayerCustomFlag(PlayerCustomFlag_InjuryTreatment))
    {
        //Say you can't heal yet.
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_must_wait"), Medic);
        return false;
    }
    //check range
    if (!CheckMedicRange(Medic, Target, (float)32.0))
        return false;
    //Does Medic have ability
    if(!Medic->verifyAbility(cmdProperties->mAbilityCrc))
    {

        gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "cannot_heal"), Medic);
        return false;
    }

    //Does Target Need Healing
	int TargetHealth = Target->GetCreature()->GetStatCurrent(HamBar_Health);
	int TargetAction = Target->GetCreature()->GetStatCurrent(HamBar_Action);

	//TODO minus modifiers (wounds bf)
	int TargetMaxHealth = Target->GetCreature()->GetStatMax(HamBar_Health);
    int TargetMaxAction = Target->GetCreature()->GetStatMax(HamBar_Action);

    if(!(TargetHealth < TargetMaxHealth))
    {
        if(!(TargetAction < TargetMaxAction))
        {
            gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_63"), Medic);
            return false;
        }
    }


    //Get Heal Strength
    //TODO - BEClothing, and Med Center/city bonuses.
    int healthpower = Stim->getHealthHeal();
    int actionpower = Stim->getActionHeal();
    uint BEClothes = 0;
    uint MedCityBonus = 0;
    uint maxhealhealth = healthpower * ((100 + healingskill + BEClothes) / 100) * MedCityBonus;
    uint maxhealaction = actionpower * ((100 + healingskill + BEClothes) / 100) * MedCityBonus;


    //Adjust for Target BF
    maxhealhealth = MedicManager::CalculateBF(Medic, Target, maxhealhealth);
    maxhealaction = MedicManager::CalculateBF(Medic, Target, maxhealaction);

    int StrengthHealth = std::min((int)maxhealhealth, TargetMaxHealth-TargetHealth);
    int StrengthAction = std::min((int)maxhealaction, TargetMaxAction-TargetAction);

	auto ham = kernel_->GetServiceManager()->GetService<swganh::ham::HamService>("HamService");

	ham->UpdateCurrentHitpoints(Target->GetCreature(), HamBar_Health, StrengthHealth);
	ham->UpdateCurrentHitpoints(Target->GetCreature(), HamBar_Action, StrengthAction);
    
    
    //Cost.
    int cost = 50;
	
	int MedicMind = Medic->GetCreature()->GetStatCurrent(HamBar_Mind);

    if (MedicMind < cost) {
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "not_enough_mind"), Medic);
        return false;
    }

	ham->UpdateCurrentHitpoints(Medic->GetCreature(), HamBar_Mind, -cost);
    //Medic->getHam()->updatePropertyValue(HamBar_Mind, HamProperty_CurrentHitpoints, -cost);


    if(Stim->ConsumeUse(Medic))
    {
		TangibleObject* container = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById(Stim->getParentId()));
		gContainerManager->deleteObject(Stim, container);
    }

    //Add XP as Total Heal / 4 if not targetting self
    if(!isSelf)
        gSkillManager->addExperience(XpType_medical, (int)((StrengthHealth + StrengthAction)/4), Medic);

    if(isSelf) //If targetting self
    {
        if(StrengthHealth > 0)
        {
            if(StrengthAction > 0)
            {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_01", 0, 0, 0, StrengthHealth), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_04", 0, 0, 0, StrengthAction), Medic);
            } else {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_10", 0, 0, 0, StrengthHealth), Medic);
            }
        } else {
            if(StrengthAction > 0)
            {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_11", 0, 0, 0, StrengthAction), Medic);
            }
        }
        gMessageLib->sendCombatAction(Medic->GetCreature(), Target, BString::CRC("throw_grenade_near_healing"), 1, 1, 1);

    } else { //if targetting something else
        if(StrengthHealth > 0)
        {
            if(StrengthAction > 0)
            {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_06", 0, Target->getId(), 0, StrengthAction), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_04", 0, 0, 0, StrengthAction), Medic);

                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_08", 0, 0, Medic->getId(), StrengthHealth), Target);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_04", 0, 0, 0, StrengthAction), Target);
            } else {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_13", 0, Target->getId(), 0, StrengthHealth), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_16", 0, 0, Medic->getId(), StrengthHealth), Target);
            }
        } else {
            if(StrengthAction > 0)
            {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_14", 0, Target->getId(), 0, StrengthAction), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_17", 0, 0, Medic->getId(), StrengthAction), Target);
            }
        }
        gMessageLib->sendCombatAction(Medic->GetCreature(), Target, BString::CRC("throw_grenade_medium_healing"), 0, 0, 1);
    }

	gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_62"), Medic);
    return true;
}
//HealWounds
bool MedicManager::HealWound(PlayerObject* Medic, PlayerObject* Target, uint64 WoundPackobjectID, ObjectControllerCmdProperties* cmdProperties, std::string healType)
{
    bool isSelf = false;
    bool tendwound = false;
    Medicine* WoundPack = dynamic_cast<Medicine*>(gWorldManager->getObjectById(WoundPackobjectID));
    isSelf = (Medic->getId() == Target->getId());

    if(Medic->checkPlayerCustomFlag(PlayerCustomFlag_WoundTreatment))
    {
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "enhancement_must_wait"), Medic);
        return false;
    }
    if (!CheckMedicRange(Medic, Target, (float)6.0))
        return false;
    //Does Medic have ability
    if(!Medic->verifyAbility(cmdProperties->mAbilityCrc))
    {
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "cannot_enhance"), Medic);
        return false;
    }

    int32 WoundHealPower = 0;
    if (WoundPack)
    {
        WoundHealPower = WoundPack->getHealWound(healType);
    }
    // random between 1-20
    else
    {
        tendwound = true;
        WoundHealPower = (gRandom->getRand() % 20)+1;
    }

    //check the wounds type
    //remove tendwound
    int found = healType.find("tendwound");
    if (found > -1)
        healType = healType.replace(found, 9,"");

    int32 maxwoundheal = MedicManager::CalculateHealWound(Medic, Target, WoundHealPower, healType);

    if(maxwoundheal <= 0)
    {
        if (isSelf) {
            gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_67"), Medic);
            return false;
        }
        if (!isSelf) {
            gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_64"), Medic);
            return false;
        }
    }
    //Cost.
    int cost = 140;
    if (tendwound)
        cost = 500;

	auto ham = kernel_->GetServiceManager()->GetService<swganh::ham::HamService>("HamService");

	if (!ham->checkMainPool(Medic->GetCreature(), HamBar_Mind, cost) ) {
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "not_enough_mind"), Medic);
        return false;
    }

	ham->UpdateCurrentHitpoints(Medic->GetCreature(), HamBar_Mind, -cost);

    if (!isSelf)
    {
        //Anim
        gMessageLib->sendCreatureAnimation(Medic->GetCreature(), BString("heal_other"));

        //CE
        gMessageLib->sendPlayClientEffectLocMessage("clienteffect/healing_healwound.cef",Target->mPosition,Target);
        //XP
        gSkillManager->addExperience(XpType_medical, (int)((maxwoundheal)*2.5), Medic);
    }
    else
    {
        //Anim
        gMessageLib->sendCreatureAnimation(Medic->GetCreature(), BString("heal_self"));

        //CE
        gMessageLib->sendPlayClientEffectLocMessage("clienteffect/healing_healwound.cef",Target->mPosition,Target);
    }

    if(!tendwound && WoundPack->ConsumeUse(Medic))
    {
		TangibleObject* container = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById(WoundPack->getParentId()));
		gContainerManager->deleteObject(WoundPack, container);
    }

	gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_62"), Medic);
    return true;
}

void MedicManager::startInjuryTreatmentEvent(PlayerObject* Medic)
{
    uint healingspeed = Medic->GetCreature()->getSkillModValue(SMod_healing_injury_speed);
    int delay = (int)((( 100 - (float)healingspeed ) / 100 ) * 10000);
    uint64 cooldown = std::max(4000, delay);
    uint64 now = gWorldManager->GetCurrentGlobalTick();

    Medic->getController()->addEvent(new InjuryTreatmentEvent(now + cooldown), cooldown);
    Medic->togglePlayerCustomFlagOn(PlayerCustomFlag_InjuryTreatment);
}
void MedicManager::startQuickHealInjuryTreatmentEvent(PlayerObject* Medic)
{
    uint healingspeed = Medic->GetCreature()->getSkillModValue(SMod_healing_injury_speed);
    int delay = (int)((( 100 - (float)healingspeed ) / 100 ) * 10000);
    uint64 cooldown = std::max(4000, delay);
    uint64 now = gWorldManager->GetCurrentGlobalTick();

    Medic->getController()->addEvent(new QuickHealInjuryTreatmentEvent(now + cooldown), cooldown);
    Medic->togglePlayerCustomFlagOn(PlayerCustomFlag_QuickHealInjuryTreatment);
}
void MedicManager::startWoundTreatmentEvent(PlayerObject* Medic)
{
    uint healingspeed = Medic->GetCreature()->getSkillModValue(SMod_healing_wound_speed);
    int delay = (int)((( 100 - (float)healingspeed ) / 100 ) * 10000);
    uint64 cooldown = std::max(1000, delay);
    uint64 now = gWorldManager->GetCurrentGlobalTick();

    Medic->getController()->addEvent(new WoundTreatmentEvent(now + cooldown), cooldown);
    Medic->togglePlayerCustomFlagOn(PlayerCustomFlag_WoundTreatment);
}
//Foraging
void MedicManager::successForage(PlayerObject* player)
{
    //Chance of success = sqrt(skill)/20 + 0.15
    //Chance in down = chance/2

    //First lets calc our chance to 'win'
    //This is the magic formula!
    double chance = std::sqrt((double)player->GetCreature()->getSkillModValue(20))/20 + 0.15;

    if(!gStructureManager->checkCityRadius(player))
        chance = chance*50;
    else
        chance = chance*100;

    if((gRandom->getRand() % 100) <= chance)
    {
        // YOU WIN!
        gMessageLib->SendSystemMessage(::common::OutOfBand("skl_use", "sys_forage_success"), player);
    }
    else
    {
        //YOU LOSE! GOOD DAY SIR!
        gMessageLib->SendSystemMessage(::common::OutOfBand("skl_use", "sys_forage_fail"), player);
    }

    player->setForaging(false);
}
//HELPERS
int32 MedicManager::CalculateBF(PlayerObject* Medic, PlayerObject* Target, int32 maxhealamount)
{
	int32 BF = Target->GetCreature()->GetBattleFatigue();
    if(BF > 250)
    {
        maxhealamount -= (maxhealamount * (BF-250) / 1000);
        if (Target && Medic->getId() != Target->getId())
        {
            if(BF > 500) {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "shock_effect_medium_target"), Target);
            } else if(BF > 750) {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "shock_effec_high_target"), Target);
            } else {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "shock_effect_low_target"), Target);
            }
        }
        else
        {
            if(BF > 500) {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "shock_effect_medium"), Medic);
            } else if(BF > 750) {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "shoc_effect_high"), Medic);
            } else {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "shock_effect_low"), Medic);
            }
        }
    }
    return maxhealamount;
}
std::string MedicManager::handleMessage(Message* message, std::string regexPattern)
{
    // Read the message out of the packet.
    BString tmp;
    message->getStringUnicode16(tmp);

    // If the string has no length the message is ill-formatted, send the
    // proper format to the client.
    if (!tmp.getLength())
        return "";

    // Convert the string to an ansi string for ease with the regex.
    tmp.convert(BSTRType_ANSI);
    std::string input_string(tmp.getAnsi());

    static const regex pattern(regexPattern);
    smatch result;

    regex_search(input_string, result, pattern);

    // Gather the results of the pattern for validation and use.
    std::string messageType(result[1]);
    if (messageType.length() > 0)
    {
        return messageType;
    }
    return "";
}
int32 MedicManager::CalculateHealWound(PlayerObject* Medic, PlayerObject* Target, int32 WoundHealPower, std::string healType)
{
	
	auto ham = kernel_->GetServiceManager()->GetService<swganh::ham::HamService>("HamService");

    bool isSelf = false;
    isSelf = (Medic->getId() == Target->getId());
    int TargetWounds = 0;
    uint32 healingskill = Medic->GetCreature()->getSkillModValue(SMod_healing_wound_treatment);
    int32 maxwoundheal = 0;
    

    if (healType == action)
    {
		TargetWounds = Target->GetCreature()->GetStatWound(HamBar_Action);
        maxwoundheal = MedicManager::CalculateBF(Medic, Target, WoundHealPower);
        maxwoundheal = maxwoundheal * ((100 + healingskill) / 100);
        maxwoundheal = std::min(maxwoundheal, TargetWounds);
		maxwoundheal = ham->UpdateWound(Target->GetCreature(), HamBar_Action, -maxwoundheal);
        if (maxwoundheal != 0)
        {
            //success message
            if (isSelf)
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_50", 0, 0, 0, maxwoundheal), Medic);
            else {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_53", 0, Target->getId(), 0, maxwoundheal), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_56", 0, 0, Medic->getId(), maxwoundheal), Target);
            }
        }
    }
    else if	(healType == constitution)
    {
        TargetWounds = Target->GetCreature()->GetStatWound(HamBar_Constitution);
        maxwoundheal = MedicManager::CalculateBF(Medic, Target, WoundHealPower);
        maxwoundheal = maxwoundheal * ((100 + healingskill) / 100);
        maxwoundheal = std::min(maxwoundheal, TargetWounds);
        maxwoundheal = ham->UpdateWound(Target->GetCreature(), HamBar_Constitution, -maxwoundheal);
        if (maxwoundheal > 0)
        {
            //success message
            if (isSelf)
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_20", 0, 0, 0, maxwoundheal), Medic);
            else {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_32", 0, Target->getId(), 0, maxwoundheal), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_41", 0, 0, Medic->getId(), maxwoundheal), Target);
            }
        }
    }
    else if (healType == health)
    {
        TargetWounds = Target->GetCreature()->GetStatWound(HamBar_Health);
        maxwoundheal = MedicManager::CalculateBF(Medic, Target, WoundHealPower);
        maxwoundheal = maxwoundheal * ((100 + healingskill) / 100);
        maxwoundheal = std::min(maxwoundheal, TargetWounds);
        maxwoundheal = ham->UpdateWound(Target->GetCreature(), HamBar_Health, -maxwoundheal);
        if (maxwoundheal > 0)
        {
            //success message
            if (isSelf)
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_49", 0, 0, 0, maxwoundheal), Medic);
            else {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_52", 0, Target->getId(), 0, maxwoundheal), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_55", 0, 0, Medic->getId(), maxwoundheal), Target);
            }
        }
    }
    else if (healType == quickness)
    {
        TargetWounds = Target->GetCreature()->GetStatWound(HamBar_Quickness);
        maxwoundheal = MedicManager::CalculateBF(Medic, Target, WoundHealPower);
        maxwoundheal = maxwoundheal * ((100 + healingskill) / 100);
        maxwoundheal = std::min(maxwoundheal, TargetWounds);
        maxwoundheal = ham->UpdateWound(Target->GetCreature(), HamBar_Quickness, -maxwoundheal);
        if (maxwoundheal > 0)
        {
            //success message
            if (isSelf)
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_21", 0, 0, 0, maxwoundheal), Medic);
            else {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_33", 0, Target->getId(), 0, maxwoundheal), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_42", 0, 0, Medic->getId(), maxwoundheal), Target);
            }
        }
    }
    else if (healType == stamina)
    {
        TargetWounds = Target->GetCreature()->GetStatWound(HamBar_Stamina);
        maxwoundheal = MedicManager::CalculateBF(Medic, Target, WoundHealPower);
        maxwoundheal = maxwoundheal * ((100 + healingskill) / 100);
        maxwoundheal = std::min(maxwoundheal, TargetWounds);
        maxwoundheal = ham->UpdateWound(Target->GetCreature(), HamBar_Stamina, -maxwoundheal);
        if (maxwoundheal > 0)
        {
            //success message
            if (isSelf)
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_22", 0, 0, 0, maxwoundheal), Medic);
            else {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_34", 0, Target->getId(), 0, maxwoundheal), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_43", 0, 0, Medic->getId(), maxwoundheal), Target);
            }
        }
    }
    else if (healType == strength)
    {
        TargetWounds = Target->GetCreature()->GetStatWound(HamBar_Strength);
        maxwoundheal = MedicManager::CalculateBF(Medic, Target, WoundHealPower);
        maxwoundheal = maxwoundheal * ((100 + healingskill) / 100);
        maxwoundheal = std::min(maxwoundheal, TargetWounds);
        maxwoundheal = ham->UpdateWound(Target->GetCreature(), HamBar_Strength, -maxwoundheal);
        if (maxwoundheal > 0)
        {
            //success message
            if (isSelf)
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_19", 0, 0, 0, maxwoundheal), Medic);
            else {
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_31", 0, Target->getId(), 0, maxwoundheal), Medic);
                gMessageLib->SendSystemMessage(::common::OutOfBand("healing_response", "healing_response_46", 0, 0, Medic->getId(), maxwoundheal), Target);
            }
        }
    }
    return maxwoundheal;
}
//Check Heal Range
bool MedicManager::CheckMedicRange(PlayerObject* Medic, PlayerObject* Target, float healRange)
{
    float distance = gWorldConfig->getConfiguration<float>("Player_heal_distance", healRange);

	if(glm::distance(Medic->GetCreature()->mPosition, Target->GetCreature()->mPosition) > distance)
    {
        gMessageLib->SendSystemMessage(::common::OutOfBand("healing", "no_line_of_sight"), Medic);
        return false;
    }
    return true;
}
