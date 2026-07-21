#include "Enemies/PREnemyDefinitionDataAsset.h"

#include "Abilities/GameplayAbility.h"

const FPrimaryAssetType UPREnemyDefinitionDataAsset::EnemyPrimaryAssetType(TEXT("ProjectRiftEnemy"));

FPrimaryAssetId UPREnemyDefinitionDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(EnemyPrimaryAssetType, EnemyId.IsNone() ? GetFName() : EnemyId);
}

bool UPREnemyDefinitionDataAsset::ValidateDefinition(FString& OutDiagnostic) const
{
	if (EnemyId.IsNone() || !FMath::IsFinite(ThreatCost) || ThreatCost <= 0.0f
		|| !FMath::IsFinite(Attributes.MaxHealth) || Attributes.MaxHealth <= 0.0f
		|| !FMath::IsFinite(Attributes.MaxShield) || Attributes.MaxShield < 0.0f
		|| !FMath::IsFinite(Attributes.InitialShield) || Attributes.InitialShield < 0.0f || Attributes.InitialShield > Attributes.MaxShield
		|| !FMath::IsFinite(Attributes.AttackPower) || Attributes.AttackPower < 0.0f
		|| !FMath::IsFinite(Attributes.MoveSpeed) || Attributes.MoveSpeed <= 0.0f)
	{
		OutDiagnostic = TEXT("Enemy definition has an invalid identity, threat cost, or attribute value.");
		return false;
	}

	for (const FPREnemyActionDefinition& Action : Actions)
	{
		if (!Action.ActionTag.IsValid()
			|| !FMath::IsFinite(Action.MinimumRange) || !FMath::IsFinite(Action.MaximumRange)
			|| !FMath::IsFinite(Action.CooldownSeconds) || !FMath::IsFinite(Action.TelegraphSeconds)
			|| !FMath::IsFinite(Action.BaseDamage) || !FMath::IsFinite(Action.Radius) || !FMath::IsFinite(Action.ProjectileSpeed)
			|| Action.MinimumRange < 0.0f || Action.MaximumRange < Action.MinimumRange
			|| Action.CooldownSeconds < 0.0f || Action.TelegraphSeconds < 0.0f
			|| Action.BaseDamage < 0.0f || Action.Radius < 0.0f || Action.ProjectileSpeed < 0.0f
			|| (Action.Kind == EPREnemyActionKind::Projectile && Action.ProjectileSpeed <= 0.0f)
			|| (Action.Kind == EPREnemyActionKind::Exploder && Action.Radius <= 0.0f)
			|| (Action.Kind == EPREnemyActionKind::Summon && (Action.SummonDefinitionId.IsNone() || Action.SummonCount <= 0)))
		{
			OutDiagnostic = TEXT("Enemy definition contains an invalid action.");
			return false;
		}
	}

	OutDiagnostic.Reset();
	return true;
}
