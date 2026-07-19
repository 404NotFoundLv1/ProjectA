#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Player/PRPlayerState.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRItemTransactionContractTest,
	"ProjectRift.Items.AuthoritativeTransactions.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRItemTransactionContractTest::RunTest(const FString& Parameters)
{
	UClass* TransactionComponentClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRItemTransactionComponent"));
	TestNotNull(TEXT("Item transaction component has a reflected class"), TransactionComponentClass);

	UFunction* Accessor = APRPlayerState::StaticClass()->FindFunctionByName(TEXT("GetItemTransactionComponent"));
	TestNotNull(TEXT("PlayerState exposes the transaction component"), Accessor);
	if (Accessor && TransactionComponentClass)
	{
		const FObjectProperty* ReturnProperty = FindFProperty<FObjectProperty>(Accessor, TEXT("ReturnValue"));
		TestNotNull(TEXT("PlayerState transaction accessor returns an object"), ReturnProperty);
		if (ReturnProperty)
		{
			TestTrue(
				TEXT("PlayerState transaction accessor returns the transaction component type"),
				ReturnProperty->PropertyClass->IsChildOf(TransactionComponentClass));
		}
	}

	UScriptStruct* HeaderStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRItemTransactionHeader"));
	UScriptStruct* RequestStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRItemTransactionRequest"));
	UScriptStruct* ResultStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRItemTransactionResult"));
	TestNotNull(TEXT("Transaction header is reflected"), HeaderStruct);
	TestNotNull(TEXT("Transaction request is reflected"), RequestStruct);
	TestNotNull(TEXT("Transaction result is reflected"), ResultStruct);
	if (HeaderStruct)
	{
		TestNotNull(TEXT("Transaction header contains TransactionId"), FindFProperty<FStructProperty>(HeaderStruct, TEXT("TransactionId")));
		TestNotNull(TEXT("Transaction header contains ExpectedRevision"), FindFProperty<FIntProperty>(HeaderStruct, TEXT("ExpectedRevision")));
	}
	if (ResultStruct)
	{
		TestNotNull(TEXT("Transaction result contains current revision"), FindFProperty<FIntProperty>(ResultStruct, TEXT("CurrentRevision")));
		TestNotNull(TEXT("Transaction result reports replay state"), FindFProperty<FBoolProperty>(ResultStruct, TEXT("bWasReplay")));
	}

	return true;
}

#endif
