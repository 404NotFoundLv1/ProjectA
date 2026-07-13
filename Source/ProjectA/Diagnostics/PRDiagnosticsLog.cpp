#include "Diagnostics/PRDiagnosticsLog.h"

#include "Diagnostics/PRDiagnosticsSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

namespace
{
FName NetModeName(const ENetMode NetMode)
{
	switch (NetMode)
	{
	case NM_Standalone: return TEXT("Standalone");
	case NM_DedicatedServer: return TEXT("DedicatedServer");
	case NM_ListenServer: return TEXT("ListenServer");
	case NM_Client: return TEXT("Client");
	default: return TEXT("Unknown");
	}
}
}

bool PRRecordDiagnosticEvent(
	const UObject* WorldContextObject,
	const EPRDiagnosticSeverity Severity,
	const FName Category,
	const FName EventCode,
	const FString& Message,
	const FPRDiagnosticContext& Context)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!WorldContextObject || !GEngine)
	{
		return false;
	}
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : Cast<UGameInstance>(const_cast<UObject*>(WorldContextObject));
	if (!GameInstance)
	{
		return false;
	}
	UPRDiagnosticsSubsystem* Diagnostics = GameInstance->GetSubsystem<UPRDiagnosticsSubsystem>();
	if (!Diagnostics)
	{
		return false;
	}

	FPRDiagnosticContext NormalizedContext = Context;
	if (World)
	{
		if (NormalizedContext.MapName.IsNone())
		{
			NormalizedContext.MapName = FName(*UWorld::RemovePIEPrefix(World->GetMapName()));
		}
		if (NormalizedContext.NetMode.IsNone())
		{
			NormalizedContext.NetMode = NetModeName(World->GetNetMode());
		}
		if (NormalizedContext.PlayerId == INDEX_NONE)
		{
			const APlayerController* Controller = World->GetFirstPlayerController();
			NormalizedContext.PlayerId = Controller && Controller->PlayerState
				? Controller->PlayerState->GetPlayerId()
				: INDEX_NONE;
		}
	}
	return Diagnostics->RecordEvent(Severity, Category, EventCode, Message, NormalizedContext);
#endif
}
