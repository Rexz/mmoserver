/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2010 The SWG:ANH Team
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

#ifndef ANH_ZONESERVER_SKILLMANAGER_ENUMS_H
#define ANH_ZONESERVER_SKILLMANAGER_ENUMS_H


//======================================================================================================================
//
// Database Queries
//
enum SMQueryType
{
    SMQuery_Skills						= 1,
    SMQuery_SkillMods					= 2,
    SMQuery_SkillCommands				= 3,
    SMQuery_SkillSchematicGroups		= 4,
    SMQuery_SkillSpecies				= 5,
    SMQuery_SkillPreclusions			= 6,
    SMQuery_SkillRequiredSkills			= 7,
    SMQuery_SkillSkillCommands			= 8,
    SMQuery_SkillSkillMods				= 9,
    SMQuery_SkillSkillSchematicGroups	= 10,
    SMQuery_XpTypes						= 11,
    SMQuery_SkillSkillXpTypes			= 12,
    SMQuery_SkillDescriptions			= 13
};

enum SMSkillType
{
    SMSkill_NoviceEntertainer		= 11,
    SMSkill_MasterEntertainer		= 12,
    SMSkill_EntertainerMusic1		= 17,
    SMSkill_EntertainerMusic2		= 18,
    SMSkill_EntertainerMusic3		= 19,
    SMSkill_EntertainerMusic4		= 20,
    SMSkill_MasterScout				= 32,
    SMSkill_MasterMedic				= 52,
    SMSkill_MasterArtisan			= 72,
    SMSkill_MasterBrawler			= 92,
    SMSkill_MasterRifleman			= 130,
    SMSkill_MasterPistoleer			= 149,
    SMSkill_MasterCarbineer			= 168,
    SMSkill_MasterTerasKasi			= 187,
    SMSkill_MasterFencer			= 206,
    SMSkill_MasterSwordsman			= 225,
    SMSkill_MasterPolearm			= 244,
    SMSkill_MasterDancer			= 263,
    SMSkill_MusicianNovice			= 281,
    SMSkill_MasterMusician			= 282,
    SMSkill_Musician_Knowledge_1	= 291,
    SMSkill_Musician_Knowledge_2	= 292,
    SMSkill_Musician_Knowledge_3	= 293,
    SMSkill_Musician_Knowledge_4	= 294,
    SMSkill_MasterDoctor			= 301,
    SMSkill_MasterRanger			= 320,
    SMSkill_MasterCreatureHandler	= 339,
    SMSkill_MasterBioEngineer		= 358,
    SMSkill_MasterArmorSmith		= 377,
    SMSkill_MasterWeaponSmith		= 396,
    SMSkill_MasterChef				= 415,
    SMSkill_MasterTailor			= 434,
    SMSkill_MasterArchitect			= 453,
    SMSkill_MasterDroidEngineer		= 472,
    SMSkill_NoviceMerchant			= 490,
    SMSkill_MasterMerchant			= 491,
    SMSkill_Merchant_Advertising_1	= 492,
    SMSkill_Merchant_Advertising_2	= 493,
    SMSkill_Merchant_Advertising_3	= 494,
    SMSkill_Merchant_Advertising_4	= 495,
    SMSkill_Merchant_Sales_1		= 496,
    SMSkill_Merchant_Sales_2		= 497,
    SMSkill_Merchant_Sales_3		= 498,
    SMSkill_Merchant_Sales_4		= 499,
    SMSkill_Merchant_Hiring_1		= 500,
    SMSkill_Merchant_Hiring_2		= 501,
    SMSkill_Merchant_Hiring_3		= 502,
    SMSkill_Merchant_Hiring_4		= 503,
    SMSkill_Merchant_Management_1	= 504,
    SMSkill_Merchant_Management_2	= 505,
    SMSkill_Merchant_Management_3	= 506,
    SMSkill_Merchant_Management_4	= 507,
    SMSkill_Smuggler				= 508,
    SMSkill_NoviceSmuggler			= 509,
    SMSkill_MasterSmuggler			= 510,
    SMSkill_MasterBountyhunter		= 529,
    SMSkill_MasterCommando			= 548,
    SMSkill_MasterCombatMedic		= 567,
    SMSkill_MasterImageDesigner		= 586,
    SMSkill_MasterSquadleader		= 605,
    SMSkill_MasterPolitician		= 624

};

//======================================================================================================================
//
// Creo1 delta actions
//
enum SMCreo1Action
{
    SMSkillRemove		= 0,
    SMSkillAdd			= 1,
    SMSkillClearAll		= 2
};

//======================================================================================================================
//
// Skillmods, reflect the db ids
//
enum SMSkillMod
{
    SMod_language_basic_speak			= 1,
    SMod_healing_dance_wound			= 2,
    SMod_healing_music_wound			= 3,
    SMod_healing_music_ability			= 4,
    SMod_healing_dance_ability			= 5,
    SMod_hair							= 6,
    SMod_markings						= 7,
    SMod_face							= 8,
    SMod_foraging						= 9,
    SMod_trapping						= 10,
    SMod_camp							= 11,
    SMod_creature_knowledge				= 12,
    SMod_creature_harvesting			= 13,
    SMod_slope_move						= 14,
    SMod_burst_run						= 15,
    SMod_mask_scent						= 16,
    SMod_creature_hit_bonus				= 17,
    SMod_healing_injury_treatment		= 18,
    SMod_healing_ability				= 19,
    SMod_medical_foraging				= 20,
    SMod_healing_injury_speed			= 21,
    SMod_medicine_assembly				= 22,
    SMod_medicine_experimentation		= 23,
    SMod_surveying						= 24,
    SMod_general_assembly				= 25,
    SMod_general_experimentation		= 26,
    SMod_clothing_customization			= 27,
    SMod_armor_customization			= 28,
    SMod_manage_vendor					= 29,
    SMod_hiring							= 30,
    SMod_vendor_item_limit				= 31,
    SMod_unarmed_accuracy				= 32,
    SMod_unarmed_damage					= 33,
    SMod_unarmed_speed					= 34,
    SMod_polearm_accuracy				= 35,
    SMod_polearm_speed					= 36,
    SMod_onehandmelee_accuracy			= 37,
    SMod_onehandmelee_speed				= 38,
    SMod_twohandmelee_accuracy			= 39,
    SMod_twohandmelee_speed				= 40,
    SMod_private_onehandmelee_combat_difficulty	= 41,
    SMod_private_twohandmelee_combat_difficulty	= 42,
    SMod_private_unarmed_combat_difficulty		= 43,
    SMod_private_polearm_combat_difficulty		= 44,
    SMod_taunt									= 45,
    SMod_private_center_of_being_efficacy		= 46,
    SMod_polearm_center_of_being_efficacy		= 47,
    SMod_onehandmelee_center_of_being_efficacy	= 48,
    SMod_twohandmelee_center_of_being_efficacy	= 49,
    SMod_center_of_being_duration_unarmed		= 50,
    SMod_center_of_being_duration_onehandmelee	= 51,
    SMod_center_of_being_duration_twohandmelee	= 52,
    SMod_center_of_being_duration_polearm		= 53,
    SMod_warcry									= 54,
    SMod_intimidate								= 55,
    SMod_berserk								= 56,
    SMod_pistol_accuracy						= 57,
    SMod_pistol_speed							= 58,
    SMod_rifle_accuracy							= 59,
    SMod_rifle_speed							= 60,
    SMod_carbine_accuracy						= 61,
    SMod_carbine_speed							= 62,
    SMod_private_rifle_combat_difficulty		= 63,
    SMod_private_carbine_combat_difficulty		= 64,
    SMod_private_pistol_combat_difficulty		= 65,
    SMod_rifle_concealment_chance				= 66,
    SMod_ranged_defense							= 67,
    SMod_melee_defense							= 68,
    SMod_alert									= 69,
    SMod_block									= 70,
    SMod_posture_change_up_defense				= 71,
    SMod_stun_defense							= 72,
    SMod_blind_defense							= 73,
    SMod_dizzy_defense							= 74,
    SMod_dodge									= 75,
    SMod_pistol_hit_while_moving				= 76,
    SMod_pistol_aim								= 77,
    SMod_posture_change_down_defense			= 78,
    SMod_knockdown_defense						= 79,
    SMod_counterattack							= 80,
    SMod_unarmed_toughness						= 81,
    SMod_combat_equillibrium					= 82,
    SMod_private_med_dot						= 83,
    SMod_unarmed_passive_defense				= 84,
    SMod_private_med_wound						= 85,
    SMod_empty1									= 86,
    SMod_healing_dance_mind						= 87,
    SMod_healing_music_shock					= 88,
    SMod_healing_music_mind						= 89,
    SMod_instrument_assembly					= 90,
    SMod_private_place_cantina					= 91,
    SMod_private_place_theater					= 92,
    SMod_empty2									= 93,
    SMod_empty3									= 94,
    SMod_private_place_hospital					= 95,
    SMod_healing_wound_speed					= 96,
    SMod_healing_wound_treatment				= 97,
    SMod_private_areatrack						= 98,
    SMod_camouflage								= 99,
    SMod_rescue									= 100,
    SMod_stored_pets							= 101,
    SMod_keep_creature							= 102,
    SMod_tame_non_aggro							= 103,
    SMod_tame_level								= 104,
    SMod_private_creature_empathy				= 105,
    SMod_private_creature_handling				= 106,
    SMod_private_creature_training				= 107,
    SMod_bio_engineer_assembly					= 108,
    SMod_bio_engineer_experimentation			= 109,
    SMod_dna_harvesting							= 110,
    SMod_armor_assembly							= 111,
    SMod_armor_experimentation					= 112,
    SMod_weapon_assembly						= 113,
    SMod_weapon_experimentation					= 114,
    SMod_food_assembly							= 115,
    SMod_food_experimentation					= 116,
    SMod_clothing_assembly						= 117,
    SMod_clothing_experimentation				= 118,
    SMod_structure_assembly						= 119,
    SMod_structure_experimentation				= 120,
    SMod_droid_assembly							= 121,
    SMod_droid_customization					= 122,
    SMod_shop_sign								= 123,
    SMod_language_all_comprehend				= 124,
    SMod_feign_death							= 125,
    SMod_spice_assembly							= 126,
    SMod_spice_experimentation					= 127,
    SMod_bounty_mission_level					= 128,
    SMod_heavy_rifle_lightning_accuracy			= 129,
    SMod_heavy_rifle_lightning_speed			= 130,
    SMod_private_heavyweapon_combat_difficulty	= 131,
    SMod_droid_tracks							= 132,
    SMod_droid_track_chance						= 133,
    SMod_droid_find_chance						= 134,
    SMod_droid_find_speed						= 135,
    SMod_droid_track_speed						= 136,
    SMod_heavy_flame_thrower_accuracy			= 137,
    SMod_heavy_rifle_acid_accuracy				= 138,
    SMod_thrown_accuracy						= 139,
    SMod_heavy_rocket_launcher_accuracy			= 140,
    SMod_healing_range_speed					= 141,
    SMod_combat_medicine_experimentation		= 142,
    SMod_combat_healing_ability					= 143,
    SMod_combat_medic_effectiveness				= 144,
    SMod_healing_range							= 145,
    SMod_combat_medicine_assembly				= 146,
    SMod_empty8									= 147,
    SMod_group_melee_defense					= 148,
    SMod_ground_ranged_defense					= 149,
    SMod_group_burst_run						= 150,
    SMod_group_slope_move						= 151,
    SMod_steadyaim								= 152,
    SMod_volley									= 153,
    SMod_private_place_cityhall					= 154,
    SMod_private_place_bank						= 155,
    SMod_private_place_shuttleport				= 156,
    SMod_private_place_cloning					= 157,
    SMod_private_place_garage					= 158,
    SMod_private_place_small_garden				= 159,
    SMod_private_place_medium_garden			= 160,
    SMod_private_place_large_garden				= 161,
    SMod_private_place_exotic_garden			= 162,
    SMod_empty4									= 163,
    SMod_empty5									= 164,
    SMod_language_rodian_speak					= 165,
    SMod_language_rodian_comprehend				= 166,
    SMod_language_trandoshan_speak				= 167,
    SMod_language_trandoshan_comprehend			= 168,
    SMod_language_moncalamari_speak				= 169,
    SMod_language_moncalamari_comprehend		= 170,
    SMod_language_wookiee_speak					= 171,
    SMod_language_wookiee_comprehend			= 172,
    SMod_language_bothan_speak					= 173,
    SMod_language_bothan_comprehend				= 174,
    SMod_language_twilek_speak					= 175,
    SMod_language_twilek_comprehend				= 176,
    SMod_language_zabrak_speak					= 177,
    SMod_language_zabrak_comprehend				= 178,
    SMod_language_lekku_speak					= 179,
    SMod_language_lekku_comprehend				= 180,
    SMod_language_ithorian_speak				= 181,
    SMod_language_ithorian_comprehend			= 182,
    SMod_language_sullustan_speak				= 183,
    SMod_language_sullustan_comprehend			= 184,
    SMod_take_cover								= 185,
    SMod_leadership								= 186,
    SMod_empty6									= 187,
    SMod_empty7									= 188,
    SMod_private_innate_regeneration			= 189,
    SMod_engine_assembly						= 190,
    SMod_booster_assembly						= 191,
    SMod_weapon_systems							= 192,
    SMod_chassis_assembly						= 193,
    SMod_power_systems							= 194,
    SMod_shields_assembly						= 195,
    SMod_advanced_assembly						= 196,
    SMod_chassis_experimentation				= 197,
    SMod_weapons_systems_experimentation		= 198,
    SMod_engine_experimentation					= 199,
    SMod_booster_experimentation				= 200,
    SMod_power_systems_experimentation			= 201,
    SMod_shields_experimentation				= 202,
    SMod_advanced_ship_experimentation			= 203,
    SMod_defense_reverse						= 204,
    SMod_propulsion_reverse						= 205,
    SMod_engineering_reverse					= 206,
    SMod_systems_reverse						= 207,
    SMod_pilot_special_tactics					= 208,
    SMod_missile_launching						= 209,
    SMod_unarmed_center_of_being_efficacy		= 210,
    SMod_polearm_toughness						= 211,
    SMod_onehandmelee_toughness					= 212,
    SMod_twohandmelee_toughness					= 213,
    SMod_aim									= 214,
    SMod_rifle_hit_while_moving					= 215,
    SMod_rifle_aim								= 216,
    SMod_pistol_accuracy_while_standing			= 217,
    SMod_carbine_hit_while_moving				= 218,
    SMod_carbine_aim							= 219,
    SMod_intimidate_defense						= 220,
    SMod_meditate								= 221,
    SMod_healing_dance_shock					= 222,
    SMod_private_creature_management			= 223,
    SMod_tame_aggro								= 224,
    SMod_droid_experimentation					= 225,
    SMod_private_place_merchant_tent			= 226,
    SMod_thrown_speed							= 227,
    SMod_heavy_rocket_launcher_speed			= 228,
    SMod_heavy_particle_beam_accuracy			= 229,
    SMod_heavy_particle_beam_speed				= 230,
    SMod_heavy_acid_beam_speed					= 231,
    SMod_heavy_acid_beam_accuracy				= 232,
    SMod_heavy_lightning_beam_speed				= 233,
    SMod_heavy_lightning_beam_accuracy			= 234,
    SMod_heavy_flame_thrower_speed				= 235,
    SMod_heavy_rifle_acid_speed					= 236,
    SMod_body									= 237,
    SMod_group_ranged_defense					= 238,
    SMod_anti_shock								= 239,
    SMod_private_innate_equilibrium				= 240,
    SMod_private_innate_vitalize				= 241,
    SMod_tame_bonus								= 242,
    SMod_jedi_force_power_max					= 243,
    SMod_jedi_force_power_regen					= 244,
    SMod_jedi_saber_assembly					= 245,
    SMod_onehandlightsaber_accuracy				= 246,
    SMod_onehandlightsaber_speed				= 247,
    SMod_saber_block							= 248,
    SMod_onehandlightsaber_toughness			= 249,
    SMod_private_jedi_difficulty				= 250,
    SMod_forcethrow_accuracy					= 251,
    SMod_forceknockdown_accuracy				= 252,
    SMod_mindblast_accuracy						= 253,
    SMod_twohandlightsaber_accuracy				= 254,
    SMod_twohandlightsaber_speed				= 255,
    SMod_twohandlightsaber_toughness			= 256,
    SMod_private_force_lightning_single_power	= 257,
    SMod_forcelightning_accuracy				= 258,
    SMod_polearmlightsaber_speed				= 259,
    SMod_polearmlightsaber_accuracy				= 260,
    SMod_forceweaken_accuracy					= 261,
    SMod_polearmlightsaber_toughness			= 262,
    SMod_private_force_lightning_cone_power		= 263,
    SMod_language_basic_comprehend				= 264,
    SMod_ranged_accuracy						= 265,
    SMod_ranged_speed							= 266,
    SMod_melee_accuracy							= 267,
    SMod_melee_speed							= 268,
    SMod_force_vehicle_control					= 269,
    SMod_force_vehicle_speed					= 270,
    SMod_force_experimentation					= 271,
    SMod_force_assembly							= 272,
    SMod_force_repair_bonus						= 273,
    SMod_force_failure_reduction				= 274,
    SMod_force_persuade							= 275,
    SMod_force_luck								= 276,
    SMod_forceintimidate_accuracy				= 277,
    SMod_jedi_state_defense						= 278,
    SMod_jedi_toughness							= 279,
    SMod_force_defense							= 280,
    SMod_force_control_light					= 281,
    SMod_force_power_light						= 282,
    SMod_force_manipulation_light				= 283,
    SMod_force_power_dark						= 284,
    SMod_force_control_dark						= 285,
    SMod_force_manipulation_dark				= 286,
    SMod_weapon_systems_experimentation			= 287,
    SMod_force_choke							= 288,
    SMod_lightsaber_toughness					= 289,
    SMod_jedi_saber_experimentation				= 290,
    SMod_private_onehandlightsaber_combat_difficulty	= 291,
    SMod_private_twohandlightsaber_combat_difficulty	= 292,
    SMod_private_polearmlightsaber_combat_difficulty	= 293,
    SMod_private_carbine_difficulty						= 294,
    SMod_private_pistol_difficulty						= 295,
    SMod_private_innate_roar							= 296
};

//======================================================================================================================

enum SMXpType
{
    XpType_apprenticeship								= 1,
    XpType_crafting_bio_engineer_creature				= 2,
    XpType_bio_engineer_dna_harvesting					= 3,
    XpType_bountyhunter									= 4,
    XpType_camp											= 5,
    XpType_combat_general								= 6,
    XpType_combat_meleespecialize_onehandlightsaber		= 7,
    XpType_combat_meleespecialize_twohandlightsaber		= 8,
    XpType_combat_meleespecialize_polearmlightsaber		= 9,
    XpType_jedi_general									= 10,
    XpType_combat_meleespecialize_onehand				= 11,
    XpType_combat_meleespecialize_polearm				= 12,
    XpType_combat_meleespecialize_twohand				= 13,
    XpType_combat_meleespecialize_unarmed				= 14,
    XpType_combat_rangedspecialize_carbine				= 15,
    XpType_combat_rangedspecialize_pistol				= 16,
    XpType_combat_rangedspecialize_rifle				= 17,
    XpType_crafting_clothing_armor						= 18,
    XpType_crafting_clothing_general					= 19,
    XpType_crafting_droid_general						= 20,
    XpType_crafting_food_general						= 21,
    XpType_crafting_general								= 22,
    XpType_crafting_medicine_general					= 23,
    XpType_crafting_spice								= 24,
    XpType_crafting_structure_general					= 25,
    XpType_crafting_weapons_general						= 26,
    XpType_creaturehandler								= 27,
    XpType_dance										= 28,
    XpType_entertainer_healing							= 29,
    XpType_imagedesigner								= 30,
    XpType_jedi											= 31,
    XpType_medical										= 32,
    XpType_merchant										= 33,
    XpType_music										= 34,
    XpType_resource_harvesting_inorganic				= 35,
    XpType_scout										= 36,
    XpType_slicing										= 37,
    XpType_squadleader									= 38,
    XpType_trapping										= 39,
    XpType_combat_rangedspecialize_heavy				= 40,
    XpType_political									= 41,
    XpType_force_rank_xp								= 42,
    XpType_fs_combat									= 43,
    XpType_fs_crafting									= 44,
    XpType_fs_senses									= 45,
    XpType_fs_reflex									= 46,
    XpType_shipwright									= 47,
    XpType_space_combat_general							= 48,
    XpType_none											= 49
};

#endif

