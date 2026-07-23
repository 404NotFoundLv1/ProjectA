#include "Abilities/GA_RiftGuardianAbilities.h"

#include "Bosses/PRBossCharacter.h"
#include "Bosses/PRBossSchedulerComponent.h"
#include "Bosses/PRRiftGuardianChargeAction.h"
#include "Bosses/PRRiftGuardianPollutionField.h"
#include "Engine/World.h"

namespace
{
	APRBossCharacter* ResolveBoss(const FGameplayAbilityActorInfo* ActorInfo)
	{
		return ActorInfo ? Cast<APRBossCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	}
}

void UGA_RiftGuardianLockedCharge::ExecuteBossPattern(const FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo, const FGameplayEventData*)
{
	APRBossCharacter* Boss = ResolveBoss(ActorInfo);
	UPRBossSchedulerComponent* Scheduler = Boss ? Boss->GetBossScheduler() : nullptr;
	if (!Boss || !Boss->HasAuthority() || !Scheduler)
	{
		return;
	}
	const FPRBossAbilityPatternDefinition* Pattern = Scheduler->GetActivePatternDefinition();
	if (!Pattern || !Boss->GetWorld())
	{
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Boss;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APRRiftGuardianChargeAction* ChargeAction = Boss->GetWorld()->SpawnActor<APRRiftGuardianChargeAction>(
		APRRiftGuardianChargeAction::StaticClass(), Boss->GetActorLocation(), Boss->GetActorRotation(), SpawnParameters))
	{
		ChargeAction->InitializeCharge(Boss, Scheduler->GetLockedTargetLocation(), *Pattern);
	}
}

void UGA_RiftGuardianPollutionField::ExecuteBossPattern(const FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo, const FGameplayEventData*)
{
	APRBossCharacter* Boss = ResolveBoss(ActorInfo);
	UPRBossSchedulerComponent* Scheduler = Boss ? Boss->GetBossScheduler() : nullptr;
	const FPRBossAbilityPatternDefinition* Pattern = Scheduler ? Scheduler->GetActivePatternDefinition() : nullptr;
	if (!Boss || !Boss->HasAuthority() || !Scheduler || !Pattern || !Boss->GetWorld())
	{
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Boss;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APRRiftGuardianPollutionField* Field = Boss->GetWorld()->SpawnActor<APRRiftGuardianPollutionField>(
		APRRiftGuardianPollutionField::StaticClass(), Scheduler->GetLockedTargetLocation(), FRotator::ZeroRotator, SpawnParameters))
	{
		Field->InitializeField(Boss, FMath::Max(1.0f, Pattern->EffectRadius), FMath::Max(0.1f, Pattern->PersistentDurationSeconds), Pattern->BaseDamage);
	}
}
