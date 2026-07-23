#include "Bosses/PRBossDefinitionDataAsset.h"

const FPrimaryAssetType UPRBossDefinitionDataAsset::BossPrimaryAssetType(TEXT("ProjectRiftBoss"));

FPrimaryAssetId UPRBossDefinitionDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(BossPrimaryAssetType, BossId.IsNone() ? GetFName() : BossId);
}

bool UPRBossDefinitionDataAsset::ValidateDefinition(FString& OutDiagnostic) const
{
	if (BossId.IsNone() || Phases.IsEmpty() || !FMath::IsFinite(Attributes.MaxHealth) || Attributes.MaxHealth <= 0.0f)
	{
		OutDiagnostic = TEXT("Boss definition requires an ID, positive health, and at least one phase.");
		return false;
	}

	float PreviousThreshold = 1.01f;
	TSet<FName> PhaseIds;
	for (const FPRBossPhaseDefinition& Phase : Phases)
	{
		if (Phase.PhaseId.IsNone() || PhaseIds.Contains(Phase.PhaseId) || !FMath::IsFinite(Phase.StartHealthPercent) || Phase.StartHealthPercent < 0.0f || Phase.StartHealthPercent > 1.0f || Phase.StartHealthPercent >= PreviousThreshold
			|| !FMath::IsFinite(Phase.AttackPowerMultiplier) || Phase.AttackPowerMultiplier < 0.0f
			|| !FMath::IsFinite(Phase.MoveSpeedMultiplier) || Phase.MoveSpeedMultiplier < 0.0f
			|| !FMath::IsFinite(Phase.CooldownMultiplier) || Phase.CooldownMultiplier <= 0.0f)
		{
			OutDiagnostic = TEXT("Boss phases must have unique IDs and strictly descending health thresholds.");
			return false;
		}
		PhaseIds.Add(Phase.PhaseId);
		PreviousThreshold = Phase.StartHealthPercent;
	}
	if (WeakPoints.Num() > 4)
	{
		OutDiagnostic = TEXT("The native reusable Boss has a maximum of four configured weak points.");
		return false;
	}
	for (const FPRBossWeakPointDefinition& WeakPoint : WeakPoints)
	{
		if (WeakPoint.WeakPointId.IsNone() || !FMath::IsFinite(WeakPoint.DamageMultiplier) || WeakPoint.DamageMultiplier < 1.0f || !FMath::IsFinite(WeakPoint.InterruptDamageThreshold) || WeakPoint.InterruptDamageThreshold < 0.0f || !FMath::IsFinite(WeakPoint.Radius) || WeakPoint.Radius <= 0.0f)
		{
			OutDiagnostic = TEXT("Boss weak points must have valid IDs, multipliers, thresholds, and radii.");
			return false;
		}
	}
	if (!FMath::IsNearlyEqual(Phases[0].StartHealthPercent, 1.0f))
	{
		OutDiagnostic = TEXT("The first boss phase must begin at 100 percent health.");
		return false;
	}

	TSet<FName> PatternIds;
	for (const FPRBossAbilityPatternDefinition& Pattern : AbilityPatterns)
	{
		const bool bInvalidLineTelegraph = Pattern.Telegraph.Shape == EPRBossTelegraphShape::Line
			&& (!FMath::IsFinite(Pattern.Telegraph.Length) || Pattern.Telegraph.Length <= 0.0f || !FMath::IsFinite(Pattern.Telegraph.Width) || Pattern.Telegraph.Width <= 0.0f);
		const bool bInvalidSummonScaling = Pattern.SummonCount < 0 || Pattern.AdditionalSummonsPerPlayer < 0 || Pattern.MaxSummons < 0
			|| ((Pattern.SummonCount > 0 || Pattern.AdditionalSummonsPerPlayer > 0) && (Pattern.SummonDefinitionId.IsNone() || Pattern.MaxSummons <= 0));
		if (Pattern.PatternId.IsNone() || PatternIds.Contains(Pattern.PatternId) || !FMath::IsFinite(Pattern.Weight) || Pattern.Weight < 0.0f || !FMath::IsFinite(Pattern.CooldownSeconds) || Pattern.CooldownSeconds < 0.0f || !FMath::IsFinite(Pattern.ExecutionDurationSeconds) || Pattern.ExecutionDurationSeconds < 0.0f || !FMath::IsFinite(Pattern.PersistentDurationSeconds) || Pattern.PersistentDurationSeconds < 0.0f || !FMath::IsFinite(Pattern.BaseDamage) || Pattern.BaseDamage < 0.0f || !FMath::IsFinite(Pattern.EffectRadius) || Pattern.EffectRadius < 0.0f || !FMath::IsFinite(Pattern.Telegraph.DurationSeconds) || Pattern.Telegraph.DurationSeconds < 0.0f || !FMath::IsFinite(Pattern.Telegraph.Length) || !FMath::IsFinite(Pattern.Telegraph.Width) || (Pattern.BaseDamage > 0.0f && Pattern.Telegraph.Shape == EPRBossTelegraphShape::None) || bInvalidLineTelegraph || bInvalidSummonScaling)
		{
			OutDiagnostic = TEXT("Boss ability patterns contain invalid identity, weight, cooldown, telegraph, or summon values.");
			return false;
		}
		PatternIds.Add(Pattern.PatternId);
	}
	TSet<FName> ArenaEventIds;
	for (const FPRBossArenaEventDefinition& ArenaEvent : ArenaEvents)
	{
		if (ArenaEvent.ArenaEventId.IsNone() || ArenaEventIds.Contains(ArenaEvent.ArenaEventId) || !ArenaEvent.EventTag.IsValid() || !FMath::IsFinite(ArenaEvent.DurationSeconds) || ArenaEvent.DurationSeconds < 0.0f)
		{
			OutDiagnostic = TEXT("Boss arena events must have unique IDs, valid event tags, and non-negative durations.");
			return false;
		}
		ArenaEventIds.Add(ArenaEvent.ArenaEventId);
	}
	for (const FPRBossAbilityPatternDefinition& Pattern : AbilityPatterns)
	{
		if (!Pattern.ArenaEventId.IsNone() && !ArenaEventIds.Contains(Pattern.ArenaEventId))
		{
			OutDiagnostic = TEXT("Boss pattern references an unknown arena event.");
			return false;
		}
	}
	for (const FPRBossPhaseDefinition& Phase : Phases)
	{
		for (const FName PatternId : Phase.EnabledPatternIds)
		{
			if (!PatternIds.Contains(PatternId)) { OutDiagnostic = TEXT("Boss phase references an unknown pattern."); return false; }
		}
	}

	OutDiagnostic.Reset();
	return true;
}
