#include "Abilities/GA_EngineerSentry.h"

#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Core/PRGameplayTags.h"

FGameplayTag UGA_EngineerSentry::GetModuleCooldownTag() const { return ProjectRiftGameplayTags::Cooldown_Skill_Q; }
TSubclassOf<UGameplayEffect> UGA_EngineerSentry::GetModuleCooldownEffectClass() const { return UPRAssaultChargeCooldownGameplayEffect::StaticClass(); }
