#include "Core/PRGameplayTags.h"

namespace ProjectRiftGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Ability_Primary, "Input.Ability.Primary", "Primary ability input");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Ability_Dodge, "Input.Ability.Dodge", "Dodge ability input");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Ability_Skill_Q, "Input.Ability.Skill.Q", "Q skill ability input");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Ability_Skill_E, "Input.Ability.Skill.E", "E skill ability input");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Ability_Skill_R, "Input.Ability.Skill.R", "R skill ability input");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Role_Assault, "Ability.Role.Assault", "Assault role module");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Role_Engineer, "Ability.Role.Engineer", "Engineer role module");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Role_Medic, "Ability.Role.Medic", "Medic role module");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller damage amount");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Status_Magnitude, "Data.Status.Magnitude", "SetByCaller status magnitude");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_CombatFeedback_Request, "Data.CombatFeedback.Request", "Marks structured combat damage requests");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_CombatFeedback_Policy, "Data.CombatFeedback.Policy", "Structured combat feedback policy");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_HitReaction_Strength, "Data.HitReaction.Strength", "Structured hit reaction strength");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_HitReaction_Duration, "Data.HitReaction.Duration", "Structured hit reaction duration");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Ability_EnergyDelta, "Data.Ability.EnergyDelta", "SetByCaller energy delta for a role module");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Ability_CooldownDuration, "Data.Ability.CooldownDuration", "SetByCaller role module cooldown duration");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Ability_EnergyRegen, "Data.Ability.EnergyRegen", "SetByCaller role energy regeneration per period");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_ShieldRepair, "Data.ShieldRepair", "SetByCaller friendly shield restoration amount");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Healing, "Data.Healing", "SetByCaller friendly health restoration amount");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Affix_Magnitude, "Data.Affix.Magnitude", "SetByCaller equipment affix magnitude");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Affix_Exclusive_Vital, "Affix.Exclusive.Vital", "Prevents multiple health affix variants on one item");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Affix_Exclusive_Shield, "Affix.Exclusive.Shield", "Prevents multiple shield affix variants on one item");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Affix_Exclusive_Energy, "Affix.Exclusive.Energy", "Prevents multiple energy affix variants on one item");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Affix_Exclusive_Attack, "Affix.Exclusive.Attack", "Prevents multiple attack affix variants on one item");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Affix_Exclusive_Mobility, "Affix.Exclusive.Mobility", "Prevents multiple movement affix variants on one item");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Physical, "Damage.Type.Physical", "Physical damage type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Pollution, "Damage.Type.Pollution", "Pollution damage type");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Debuff_Polluted, "Status.Debuff.Polluted", "Pollution damage over time is active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Debuff_Slowed, "Status.Debuff.Slowed", "Movement slow is active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Debuff_Revealed, "Status.Debuff.Revealed", "Recon scan is revealing an enemy");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Debuff_Parasitized, "Status.Debuff.Parasitized", "Parasite is draining energy and slowing the target");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Grace_Disruption, "Status.Grace.Disruption", "Prevents repeated ability disruption");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Ability_Enemy_Melee, "Event.Ability.Enemy.Melee", "Requests an enemy melee attack");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Player is dead");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Downed, "State.Downed", "Player is downed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "State.Stunned", "Player is stunned");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_HitStaggered, "State.HitStaggered", "Combatant is reacting to a hit");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Aiming, "State.Aiming", "Player is aiming a weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Reloading, "State.Reloading", "Player is reloading a weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_PlacingDeployable, "State.PlacingDeployable", "Player is confirming a deployable placement");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Reviving, "State.Reviving", "Player is reviving a downed ally");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_BeingRevived, "State.BeingRevived", "Player is receiving a revive");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_UsingItem, "State.UsingItem", "Player is channeling a quickbar item");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Enemy_Elite, "State.Enemy.Elite", "Enemy spawned as a mission elite reinforcement");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_AbilityDisrupted, "State.AbilityDisrupted", "Role module inputs are temporarily blocked");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mission_Modifier_LowGravity, "Mission.Modifier.LowGravity", "Rift low-gravity mission rule is active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mission_Modifier_ShieldInterference, "Mission.Modifier.ShieldInterference", "Rift shield interference mission rule is active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mission_Modifier_PollutionAmplified, "Mission.Modifier.PollutionAmplified", "Rift pollution amplification mission rule is active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mission_Modifier_EliteReinforcements, "Mission.Modifier.EliteReinforcements", "Rift elite reinforcement mission rule is active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mission_Modifier_ResourceRich, "Mission.Modifier.ResourceRich", "Rift resource-rich mission rule is active");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Impact_Shield, "GameplayCue.Combat.Impact.Shield", "Shield damage feedback");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Impact_ShieldBreak, "GameplayCue.Combat.Impact.ShieldBreak", "Shield break feedback");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Impact_Health, "GameplayCue.Combat.Impact.Health", "Health damage feedback");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Impact_Lethal, "GameplayCue.Combat.Impact.Lethal", "Lethal damage feedback");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Status_Polluted, "GameplayCue.Combat.Status.Polluted", "Pollution status feedback");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Status_Slowed, "GameplayCue.Combat.Status.Slowed", "Slow status feedback");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Status_Stunned, "GameplayCue.Combat.Status.Stunned", "Stun status feedback");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Support_Healed, "GameplayCue.Combat.Support.Healed", "Health restoration feedback");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Support_Purified, "GameplayCue.Combat.Support.Purified", "Purification feedback");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Status_Revealed, "GameplayCue.Combat.Status.Revealed", "Recon reveal feedback");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill_Q, "Cooldown.Skill.Q", "Cooldown for Q skill");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill_E, "Cooldown.Skill.E", "Cooldown for E skill");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill_R, "Cooldown.Skill.R", "Cooldown for R skill");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Item_HealthInjector, "Cooldown.Item.HealthInjector", "Cooldown for health injector");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Item_ShieldPack, "Cooldown.Item.ShieldPack", "Cooldown for shield pack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Item_EnergyCell, "Cooldown.Item.EnergyCell", "Cooldown for energy cell");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Item_Purifier, "Cooldown.Item.Purifier", "Cooldown for purifier");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Item_AmmoPack, "Cooldown.Item.AmmoPack", "Cooldown for ammunition pack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Using, "GameplayCue.Item.Using", "Quickbar use is in progress");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Completed, "GameplayCue.Item.Completed", "Quickbar use completed");
}
