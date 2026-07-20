#include "Items/PRAffixDefinitionDataAsset.h"

#include "GameplayEffect.h"

const FPrimaryAssetType UPRAffixDefinitionDataAsset::AffixPrimaryAssetType(TEXT("ProjectRiftAffix"));

FPrimaryAssetId UPRAffixDefinitionDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AffixPrimaryAssetType, AffixId.IsNone() ? GetFName() : AffixId);
}

bool UPRAffixDefinitionDataAsset::ValidateDefinition(FString& OutDiagnostic) const
{
	OutDiagnostic.Reset();
	if (AffixId.IsNone() || DisplayName.IsEmpty() || Description.IsEmpty())
	{
		OutDiagnostic = TEXT("Affix requires an id, localized display name, and description.");
		return false;
	}
	if (AllowedSlots.IsEmpty() || AllowedSlots.Contains(EPREquipmentSlot::None))
	{
		OutDiagnostic = TEXT("Affix requires one or more concrete equipment slots.");
		return false;
	}
	if (!FMath::IsFinite(MinMagnitude) || !FMath::IsFinite(MaxMagnitude) || !FMath::IsFinite(Weight)
		|| MinMagnitude <= 0.0f || MaxMagnitude < MinMagnitude || Weight <= 0.0f)
	{
		OutDiagnostic = TEXT("Affix magnitude range and weight must be finite positive values.");
		return false;
	}
	if (!ModifierEffectClass)
	{
		OutDiagnostic = TEXT("Affix requires an explicit native modifier GameplayEffect class.");
		return false;
	}
	return true;
}

