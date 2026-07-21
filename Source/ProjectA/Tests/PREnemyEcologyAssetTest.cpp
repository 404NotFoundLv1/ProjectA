#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRAssetManager.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "Enemies/PREnemyDefinitionDataAsset.h"
#include "Enemies/PREnemyRosterDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPREnemyEcologyAssetTest,
	"ProjectRift.Rift.EnemyEcology.Assets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPREnemyEcologyAssetTest::RunTest(const FString& Parameters)
{
	UPRAssetManager* Manager = UPRAssetManager::Get();
	TestNotNull(TEXT("ProjectRift asset manager exists"), Manager);
	if (!Manager)
	{
		return false;
	}

	TArray<UPREnemyDefinitionDataAsset*> Definitions;
	TArray<UPREnemyRosterDataAsset*> Rosters;
	TestTrue(TEXT("Enemy catalog loads"), Manager->LoadEnemyCatalog(Definitions, Rosters));
	TestEqual(TEXT("Exactly twelve playable enemy definitions ship"), Definitions.Num(), 12);
	TestEqual(TEXT("Exactly one Rift test roster ships"), Rosters.Num(), 1);

	TSet<FName> ExpectedIds = {
		TEXT("Enemy.Regular.Crawler"), TEXT("Enemy.Regular.Spitter"), TEXT("Enemy.Regular.Burster"),
		TEXT("Enemy.Regular.Burrower"), TEXT("Enemy.Regular.Swarmer"), TEXT("Enemy.Regular.Parasite"),
		TEXT("Enemy.Regular.GuardDrone"), TEXT("Enemy.Regular.RangedSentry"), TEXT("Enemy.Elite.NestGuardian"),
		TEXT("Enemy.Elite.RiftHunter"), TEXT("Enemy.Elite.Summoner"), TEXT("Enemy.Elite.AbilityDisruptor") };
	int32 EliteCount = 0;
	UPREnemyDefinitionDataAsset* Guardian = nullptr;
	UPREnemyDefinitionDataAsset* Summoner = nullptr;
	TMap<FName, UPREnemyDefinitionDataAsset*> DefinitionsById;
	for (UPREnemyDefinitionDataAsset* Definition : Definitions)
	{
		FString Diagnostic;
		TestTrue(FString::Printf(TEXT("Definition %s validates"), *GetNameSafe(Definition)), Definition && Definition->ValidateDefinition(Diagnostic));
		if (Definition)
		{
			DefinitionsById.Add(Definition->EnemyId, Definition);
			ExpectedIds.Remove(Definition->EnemyId);
			EliteCount += Definition->Tier == EPREnemyTier::Elite ? 1 : 0;
			if (Definition->EnemyId == TEXT("Enemy.Elite.NestGuardian")) Guardian = Definition;
			if (Definition->EnemyId == TEXT("Enemy.Elite.Summoner")) Summoner = Definition;
		}
	}
	TestEqual(TEXT("All expected definition IDs are present"), ExpectedIds.Num(), 0);
	TestEqual(TEXT("Roster contains four genuine elite definitions"), EliteCount, 4);
	TestTrue(TEXT("Nest Guardian ignores light hit reactions"), Guardian && Guardian->bHeavyHitReactionsOnly);
	TestTrue(TEXT("Summoner has a three-Swarmer action"), Summoner && Summoner->Actions.Num() == 1
		&& Summoner->Actions[0].Kind == EPREnemyActionKind::Summon
		&& Summoner->Actions[0].SummonDefinitionId == TEXT("Enemy.Regular.Swarmer")
		&& Summoner->Actions[0].SummonCount == 3);

	const TMap<FName, EPREnemyActionKind> ExpectedActionKinds = {
		{TEXT("Enemy.Regular.Crawler"), EPREnemyActionKind::Melee},
		{TEXT("Enemy.Regular.Spitter"), EPREnemyActionKind::Projectile},
		{TEXT("Enemy.Regular.Burster"), EPREnemyActionKind::Exploder},
		{TEXT("Enemy.Regular.Burrower"), EPREnemyActionKind::Burrow},
		{TEXT("Enemy.Regular.Swarmer"), EPREnemyActionKind::Melee},
		{TEXT("Enemy.Regular.Parasite"), EPREnemyActionKind::Pounce},
		{TEXT("Enemy.Regular.GuardDrone"), EPREnemyActionKind::ShieldPulse},
		{TEXT("Enemy.Regular.RangedSentry"), EPREnemyActionKind::Burst},
		{TEXT("Enemy.Elite.NestGuardian"), EPREnemyActionKind::ShieldPulse},
		{TEXT("Enemy.Elite.RiftHunter"), EPREnemyActionKind::Pounce},
		{TEXT("Enemy.Elite.Summoner"), EPREnemyActionKind::Summon},
		{TEXT("Enemy.Elite.AbilityDisruptor"), EPREnemyActionKind::Disrupt}};
	for (const TPair<FName, EPREnemyActionKind>& Expected : ExpectedActionKinds)
	{
		const UPREnemyDefinitionDataAsset* Definition = DefinitionsById.FindRef(Expected.Key);
		TestTrue(FString::Printf(TEXT("%s has one configured combat action"), *Expected.Key.ToString()), Definition && Definition->Actions.Num() == 1);
		if (Definition && Definition->Actions.Num() == 1)
		{
			TestEqual(FString::Printf(TEXT("%s uses its unique action kind"), *Expected.Key.ToString()), Definition->Actions[0].Kind, Expected.Value);
		}
	}

	const UPREnemyDefinitionDataAsset* Crawler = DefinitionsById.FindRef(TEXT("Enemy.Regular.Crawler"));
	const UPREnemyDefinitionDataAsset* Spitter = DefinitionsById.FindRef(TEXT("Enemy.Regular.Spitter"));
	const UPREnemyDefinitionDataAsset* Parasite = DefinitionsById.FindRef(TEXT("Enemy.Regular.Parasite"));
	const UPREnemyDefinitionDataAsset* Disruptor = DefinitionsById.FindRef(TEXT("Enemy.Elite.AbilityDisruptor"));
	TestTrue(TEXT("Crawler carries its pollution response"), Crawler && Crawler->Attributes.MaxHealth == 45.0f && Crawler->Attributes.MoveSpeed == 440.0f
		&& Crawler->Actions[0].StatusEffect.EffectClass == UPRPollutionStatusGameplayEffect::StaticClass());
	TestTrue(TEXT("Spitter has an obstructable 1400-speed projectile profile"), Spitter && Spitter->Actions[0].MinimumRange == 650.0f
		&& Spitter->Actions[0].MaximumRange == 1600.0f && Spitter->Actions[0].ProjectileSpeed == 1400.0f && Spitter->Actions[0].TelegraphSeconds == 0.45f);
	TestTrue(TEXT("Parasite carries its energy drain and slow"), Parasite && Parasite->Actions[0].StatusEffect.EffectClass == UPRParasitizedStatusGameplayEffect::StaticClass()
		&& Parasite->Actions[0].StatusEffect.DurationSeconds == 3.0f);
	TestTrue(TEXT("Disruptor has Q/E/R disruption and an eight-second grace period"), Disruptor && Disruptor->Actions[0].StatusEffect.EffectClass == UPRAbilityDisruptedStatusGameplayEffect::StaticClass()
		&& Disruptor->Actions[0].CooldownSeconds == 8.0f);

	UPREnemyRosterDataAsset* Roster = Manager->LoadEnemyRosterSync(TEXT("Roster.RiftTest"));
	TestNotNull(TEXT("Rift test roster loads by primary ID"), Roster);
	if (Roster)
	{
		FString Diagnostic;
		TestTrue(TEXT("Rift test roster validates"), Roster->ValidateRoster(Diagnostic));
		TestEqual(TEXT("Roster binds every definition to a concrete enemy Blueprint"), Roster->Entries.Num(), 12);
		int32 CappedEntries = 0;
		int32 RiskGatedEntries = 0;
		for (const FPREnemyRosterEntry& Entry : Roster->Entries)
		{
			CappedEntries += Entry.MaxAlive > 0 ? 1 : 0;
			RiskGatedEntries += Entry.MinimumRiskTier != EPRRiftRiskTier::Stable ? 1 : 0;
		}
		TestTrue(TEXT("Roster constrains duplicate high-pressure enemies"), CappedEntries >= 1);
		TestTrue(TEXT("Roster gates elite appearance by Rift risk"), RiskGatedEntries >= 4);
	}
	return true;
}

#endif
