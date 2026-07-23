#include "Bosses/PRBossObjectiveActor.h"

#include "Bosses/PRBossEncounterController.h"
#include "Core/PRRiftGameMode.h"

void APRBossObjectiveActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && BossEncounterController)
	{
		BossEncounterController->OnBossDefeated.AddDynamic(this, &APRBossObjectiveActor::HandleBossDefeated);
	}
}

void APRBossObjectiveActor::HandleObjectiveActivated(AController* ActivatingController)
{
	Super::HandleObjectiveActivated(ActivatingController);
	if (HasAuthority() && BossEncounterController)
	{
		BossEncounterController->StartBossEncounter();
	}
}

void APRBossObjectiveActor::HandleBossDefeated(APRBossCharacter* DefeatedBoss)
{
	if (!HasAuthority() || !DefeatedBoss || !BossEncounterController)
	{
		return;
	}
	if (APRRiftGameMode* RiftGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APRRiftGameMode>() : nullptr)
	{
		RiftGameMode->RegisterBossDefeated(this, BossEncounterController->ConsumePendingRewardId());
	}
}
