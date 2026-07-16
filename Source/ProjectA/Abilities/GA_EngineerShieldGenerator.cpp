#include "Abilities/GA_EngineerShieldGenerator.h"

#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Core/PRGameplayTags.h"

FGameplayTag UGA_EngineerShieldGenerator::GetModuleCooldownTag() const { return ProjectRiftGameplayTags::Cooldown_Skill_R; }
TSubclassOf<UGameplayEffect> UGA_EngineerShieldGenerator::GetModuleCooldownEffectClass() const { return UPRAssaultShieldCooldownGameplayEffect::StaticClass(); }
