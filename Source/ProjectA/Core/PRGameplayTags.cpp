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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Physical, "Damage.Type.Physical", "Physical damage type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Pollution, "Damage.Type.Pollution", "Pollution damage type");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Debuff_Polluted, "Status.Debuff.Polluted", "Pollution damage over time is active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Debuff_Slowed, "Status.Debuff.Slowed", "Movement slow is active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Debuff_Revealed, "Status.Debuff.Revealed", "Recon scan is revealing an enemy");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Ability_Enemy_Melee, "Event.Ability.Enemy.Melee", "Requests an enemy melee attack");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Player is dead");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Downed, "State.Downed", "Player is downed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "State.Stunned", "Player is stunned");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_HitStaggered, "State.HitStaggered", "Combatant is reacting to a hit");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Aiming, "State.Aiming", "Player is aiming a weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Reloading, "State.Reloading", "Player is reloading a weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_PlacingDeployable, "State.PlacingDeployable", "Player is confirming a deployable placement");

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
}
