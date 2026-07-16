#pragma once

#include "CoreMinimal.h"
#include "PRRoleTypes.generated.h"

UENUM(BlueprintType)
enum class EPRRoleModuleSlot : uint8
{
	None UMETA(Hidden),
	Q,
	E,
	R
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRRoleModuleSlotEntry
{
	GENERATED_BODY()

	FPRRoleModuleSlotEntry() = default;
	FPRRoleModuleSlotEntry(const EPRRoleModuleSlot InSlot, const FName InModuleId)
		: Slot(InSlot)
		, ModuleId(InModuleId)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role")
	EPRRoleModuleSlot Slot = EPRRoleModuleSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role")
	FName ModuleId = NAME_None;

	bool operator==(const FPRRoleModuleSlotEntry& Other) const
	{
		return Slot == Other.Slot && ModuleId == Other.ModuleId;
	}
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRRoleLoadout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role")
	TArray<FPRRoleModuleSlotEntry> Entries;

	bool IsStructurallyValid(FString* OutDiagnostic = nullptr) const;
};

UENUM(BlueprintType)
enum class EPRRoleLoadoutApplyResult : uint8
{
	Applied,
	InvalidLoadout,
	RoleLocked,
	ModuleLocked,
	NotInLobby,
	ReadyLocked,
	InternalFailure
};
