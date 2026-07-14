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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Physical, "Damage.Type.Physical", "Physical damage type");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Pollution, "Damage.Type.Pollution", "Pollution damage type");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Debuff_Polluted, "Status.Debuff.Polluted", "Pollution damage over time is active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Debuff_Slowed, "Status.Debuff.Slowed", "Movement slow is active");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Ability_Enemy_Melee, "Event.Ability.Enemy.Melee", "Requests an enemy melee attack");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Player is dead");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Downed, "State.Downed", "Player is downed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "State.Stunned", "Player is stunned");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill_Q, "Cooldown.Skill.Q", "Cooldown for Q skill");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill_E, "Cooldown.Skill.E", "Cooldown for E skill");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill_R, "Cooldown.Skill.R", "Cooldown for R skill");
}
