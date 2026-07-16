#include "Abilities/GA_EngineerRepairDrone.h"

#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Core/PRGameplayTags.h"

FGameplayTag UGA_EngineerRepairDrone::GetModuleCooldownTag() const { return ProjectRiftGameplayTags::Cooldown_Skill_E; }
TSubclassOf<UGameplayEffect> UGA_EngineerRepairDrone::GetModuleCooldownEffectClass() const { return UPRAssaultBlastCooldownGameplayEffect::StaticClass(); }
