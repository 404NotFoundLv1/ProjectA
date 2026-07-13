#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/DataAsset.h"
#include "Engine/GameInstance.h"
#include "Core/PRAssetManager.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/PRProfileSave.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Persistence/PRSaveStorage.h"
#include "Player/PRPlayerController.h"
#include "Ship/PRShipRepairDataAsset.h"
#include "Ship/PRShipRepairTypes.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UnrealType.h"
#include "UI/PRShipRepairWidget.h"

namespace
{
UPRShipRepairDataAsset* MakeEngineRepairContract()
{
	UPRShipRepairDataAsset* Contract = NewObject<UPRShipRepairDataAsset>(GetTransientPackage());
	Contract->RepairProjectId = TEXT("Repair.Ship.Engine.Stage1");
	Contract->DisplayName = FText::FromString(TEXT("Engine Repair I"));
	Contract->Description = FText::FromString(TEXT("Restore the engine compartment."));
	Contract->ModuleId = TEXT("Ship.Module.Engine");
	Contract->TargetLevel = 1;
	Contract->ResourceCosts = { FPRShipRepairResourceCost{ TEXT("EnergyCrystal"), 10 } };
	Contract->RequiredCompletedStoryNodeIds = { TEXT("Story.Prologue.RiftTestHold") };
	Contract->UnlockedChapterIdsOnCompletion = { TEXT("Chapter.One") };
	Contract->NextChapterId = TEXT("Chapter.One");
	return Contract;
}

FString MakeShipRepairTestRoot(const TCHAR* TestName)
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation"),
		TEXT("ShipRepair"),
		FString::Printf(TEXT("%s-%s"), TestName, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
}

bool WriteShipRepairRawEnvelopedSave(USaveGame* SaveGame, const FString& Path)
{
	TArray<uint8> Payload;
	if (!UGameplayStatics::SaveGameToMemory(SaveGame, Payload))
	{
		return false;
	}
	uint32 Magic = 0x56535250;
	uint16 EnvelopeVersion = 1;
	uint32 PayloadSize = Payload.Num();
	uint32 PayloadCrc = FCrc::MemCrc32(Payload.GetData(), Payload.Num());
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes, true);
	Writer << Magic;
	Writer << EnvelopeVersion;
	Writer << PayloadSize;
	Writer << PayloadCrc;
	Writer.Serialize(Payload.GetData(), Payload.Num());
	return !Writer.IsError() && FFileHelper::SaveArrayToFile(Bytes, *Path);
}

UPRProfileSave* MakeShipRepairStoredProfile(const FGuid ProfileId, const FPRProfileSnapshot& Snapshot, const int32 SaveVersion)
{
	UPRProfileSave* Profile = NewObject<UPRProfileSave>(GetTransientPackage());
	Profile->ProfileId = ProfileId;
	Profile->DisplayName = TEXT("Ship Repair Client");
	Profile->SaveVersion = SaveVersion;
	Profile->CreatedUtc = FDateTime::UtcNow();
	Profile->LastSavedUtc = Profile->CreatedUtc;
	Profile->Snapshot = Snapshot;
	return Profile;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRShipRepairContractTest,
	"ProjectRift.ShipRepair.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRShipRepairContractTest::RunTest(const FString& Parameters)
{
	UClass* RepairClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRShipRepairDataAsset"));
	TestNotNull(TEXT("Ship repair contract class exists"), RepairClass);
	TestTrue(
		TEXT("Ship repair contract is a Primary DataAsset"),
		RepairClass && RepairClass->IsChildOf(UPrimaryDataAsset::StaticClass()));
	if (RepairClass)
	{
		TestNotNull(TEXT("Repair contract exposes RepairProjectId"), FindFProperty<FNameProperty>(RepairClass, TEXT("RepairProjectId")));
		TestNotNull(TEXT("Repair contract exposes DisplayName"), FindFProperty<FTextProperty>(RepairClass, TEXT("DisplayName")));
		TestNotNull(TEXT("Repair contract exposes Description"), FindFProperty<FTextProperty>(RepairClass, TEXT("Description")));
		TestNotNull(TEXT("Repair contract exposes ModuleId"), FindFProperty<FNameProperty>(RepairClass, TEXT("ModuleId")));
		TestNotNull(TEXT("Repair contract exposes TargetLevel"), FindFProperty<FIntProperty>(RepairClass, TEXT("TargetLevel")));
		TestNotNull(TEXT("Repair contract exposes ResourceCosts"), FindFProperty<FArrayProperty>(RepairClass, TEXT("ResourceCosts")));
		TestNotNull(TEXT("Repair contract exposes repair prerequisites"), FindFProperty<FArrayProperty>(RepairClass, TEXT("RequiredRepairProjectIds")));
		TestNotNull(TEXT("Repair contract exposes story prerequisites"), FindFProperty<FArrayProperty>(RepairClass, TEXT("RequiredCompletedStoryNodeIds")));
		TestNotNull(TEXT("Repair contract exposes unlocked chapters"), FindFProperty<FArrayProperty>(RepairClass, TEXT("UnlockedChapterIdsOnCompletion")));
		TestNotNull(TEXT("Repair contract exposes NextChapterId"), FindFProperty<FNameProperty>(RepairClass, TEXT("NextChapterId")));
		TestNotNull(TEXT("Repair contract exposes validation"), RepairClass->FindFunctionByName(TEXT("IsContractValid")));
	}

	UScriptStruct* CostStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRShipRepairResourceCost"));
	TestNotNull(TEXT("Ship repair resource cost is reflected"), CostStruct);
	if (CostStruct)
	{
		TestNotNull(TEXT("Repair cost exposes ResourceId"), FindFProperty<FNameProperty>(CostStruct, TEXT("ResourceId")));
		TestNotNull(TEXT("Repair cost exposes Count"), FindFProperty<FIntProperty>(CostStruct, TEXT("Count")));
	}

	UScriptStruct* EvaluationStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRShipRepairEvaluation"));
	TestNotNull(TEXT("Ship repair evaluation is reflected"), EvaluationStruct);
	if (EvaluationStruct)
	{
		TestNotNull(TEXT("Repair evaluation exposes availability"), FindFProperty<FEnumProperty>(EvaluationStruct, TEXT("Availability")));
		TestNotNull(TEXT("Repair evaluation exposes diagnostics"), FindFProperty<FStrProperty>(EvaluationStruct, TEXT("Diagnostic")));
	}

	UScriptStruct* ReceiptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRShipRepairReceipt"));
	TestNotNull(TEXT("Ship repair receipt is reflected"), ReceiptStruct);
	if (ReceiptStruct)
	{
		TestNotNull(TEXT("Repair receipt exposes ProfileId"), FindFProperty<FStructProperty>(ReceiptStruct, TEXT("ProfileId")));
		TestNotNull(TEXT("Repair receipt exposes RepairProjectId"), FindFProperty<FNameProperty>(ReceiptStruct, TEXT("RepairProjectId")));
		TestNotNull(TEXT("Repair receipt exposes TransactionId"), FindFProperty<FStructProperty>(ReceiptStruct, TEXT("TransactionId")));
		TestNotNull(TEXT("Repair receipt exposes settled wallet"), FindFProperty<FArrayProperty>(ReceiptStruct, TEXT("SettledResourceWallet")));
		TestNotNull(TEXT("Repair receipt exposes settled modules"), FindFProperty<FArrayProperty>(ReceiptStruct, TEXT("SettledShipModules")));
		TestNotNull(TEXT("Repair receipt exposes settled story"), FindFProperty<FStructProperty>(ReceiptStruct, TEXT("SettledStory")));
	}

	UClass* SaveSubsystemClass = UPRSaveSubsystem::StaticClass();
	TestNotNull(TEXT("Save subsystem exposes repair evaluation"), SaveSubsystemClass->FindFunctionByName(TEXT("EvaluateShipRepair")));
	TestNotNull(TEXT("Save subsystem exposes repair receipt application"), SaveSubsystemClass->FindFunctionByName(TEXT("ApplyShipRepairReceipt")));
	TestNotNull(TEXT("Save subsystem exposes pending repair query"), SaveSubsystemClass->FindFunctionByName(TEXT("HasPendingShipRepairReceipt")));
	TestNotNull(TEXT("Save subsystem exposes pending repair retry"), SaveSubsystemClass->FindFunctionByName(TEXT("RetryPendingShipRepairReceipt")));
	TestNotNull(TEXT("Save subsystem exposes development acceptance preparation"), SaveSubsystemClass->FindFunctionByName(TEXT("PrepareShipRepairAcceptanceForDevelopment")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRShipRepairPolicyTest,
	"ProjectRift.ShipRepair.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRShipRepairPolicyTest::RunTest(const FString& Parameters)
{
	UPRShipRepairDataAsset* EngineContract = MakeEngineRepairContract();
	TArray<UPRShipRepairDataAsset*> Catalog{ EngineContract };
	FString Diagnostic;
	TestTrue(TEXT("Production-shaped engine repair contract is valid"), EngineContract->ValidateContract(&Diagnostic));
	TestTrue(TEXT("Single repair project catalog is valid"), UPRShipRepairDataAsset::ValidateCatalog(Catalog, &Diagnostic));

	UPRShipRepairDataAsset* DuplicateCostContract = DuplicateObject<UPRShipRepairDataAsset>(EngineContract, GetTransientPackage());
	DuplicateCostContract->RepairProjectId = TEXT("Repair.Ship.Engine.DuplicateCost");
	DuplicateCostContract->ResourceCosts.Add(FPRShipRepairResourceCost{ TEXT("EnergyCrystal"), 1 });
	TestFalse(TEXT("Duplicate resource costs are rejected"), DuplicateCostContract->ValidateContract(&Diagnostic));

	UPRShipRepairDataAsset* SelfDependentContract = DuplicateObject<UPRShipRepairDataAsset>(EngineContract, GetTransientPackage());
	SelfDependentContract->RepairProjectId = TEXT("Repair.Ship.Engine.SelfDependent");
	SelfDependentContract->RequiredRepairProjectIds = { SelfDependentContract->RepairProjectId };
	TestFalse(TEXT("A repair project cannot require itself"), SelfDependentContract->ValidateContract(&Diagnostic));

	UPRShipRepairDataAsset* MissingPrerequisiteContract = DuplicateObject<UPRShipRepairDataAsset>(EngineContract, GetTransientPackage());
	MissingPrerequisiteContract->RepairProjectId = TEXT("Repair.Ship.Engine.MissingPrerequisite");
	MissingPrerequisiteContract->RequiredRepairProjectIds = { TEXT("Repair.Ship.Unknown") };
	TestFalse(
		TEXT("Catalog rejects a missing repair prerequisite"),
		UPRShipRepairDataAsset::ValidateCatalog({ MissingPrerequisiteContract }, &Diagnostic));

	UPRShipRepairDataAsset* CycleA = DuplicateObject<UPRShipRepairDataAsset>(EngineContract, GetTransientPackage());
	CycleA->RepairProjectId = TEXT("Repair.Ship.Cycle.A");
	CycleA->ModuleId = TEXT("Ship.Module.CycleA");
	CycleA->RequiredRepairProjectIds = { TEXT("Repair.Ship.Cycle.B") };
	UPRShipRepairDataAsset* CycleB = DuplicateObject<UPRShipRepairDataAsset>(EngineContract, GetTransientPackage());
	CycleB->RepairProjectId = TEXT("Repair.Ship.Cycle.B");
	CycleB->ModuleId = TEXT("Ship.Module.CycleB");
	CycleB->RequiredRepairProjectIds = { TEXT("Repair.Ship.Cycle.A") };
	TestFalse(TEXT("Catalog rejects cyclic repair prerequisites"), UPRShipRepairDataAsset::ValidateCatalog({ CycleA, CycleB }, &Diagnostic));

	FPRProfileSnapshot Snapshot;
	Snapshot.ResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 10 } };
	FPRShipRepairEvaluation Evaluation = EngineContract->EvaluateRepair(Snapshot, Catalog);
	TestEqual(TEXT("Missing story prerequisite blocks repair"), Evaluation.Availability, EPRShipRepairAvailability::MissingStoryProgress);

	Snapshot.Story.CompletedStoryNodeIds.Add(TEXT("Story.Prologue.RiftTestHold"));
	Snapshot.ResourceWallet[0].Count = 9;
	Evaluation = EngineContract->EvaluateRepair(Snapshot, Catalog);
	TestEqual(TEXT("Insufficient resources block repair"), Evaluation.Availability, EPRShipRepairAvailability::InsufficientResources);

	Snapshot.ResourceWallet[0].Count = 10;
	Evaluation = EngineContract->EvaluateRepair(Snapshot, Catalog);
	TestTrue(TEXT("Exact resources satisfy the repair contract"), Evaluation.IsAvailable());
	TestTrue(TEXT("Available repair applies to a snapshot"), EngineContract->ApplyRepairToSnapshot(Snapshot, Catalog, &Diagnostic));
	TestEqual(TEXT("Exact repair cost removes the zero balance row"), Snapshot.GetResourceCount(TEXT("EnergyCrystal")), 0);
	const FPRProfileShipModuleState* EngineModule = Snapshot.ShipModules.FindByPredicate([](const FPRProfileShipModuleState& Module)
	{
		return Module.ModuleId == FName(TEXT("Ship.Module.Engine"));
	});
	TestNotNull(TEXT("Repair creates the engine module state"), EngineModule);
	TestEqual(TEXT("Repair raises the engine to level one"), EngineModule ? EngineModule->Level : INDEX_NONE, 1);
	TestTrue(TEXT("Repair unlocks the engine module"), EngineModule && EngineModule->bUnlocked);
	TestTrue(TEXT("Repair unlocks Chapter One"), Snapshot.Story.UnlockedChapterIds.Contains(TEXT("Chapter.One")));
	TestEqual(TEXT("Repair advances the current chapter"), Snapshot.Story.CurrentChapterId, FName(TEXT("Chapter.One")));
	Evaluation = EngineContract->EvaluateRepair(Snapshot, Catalog);
	TestEqual(TEXT("Completed repair is idempotently recognized"), Evaluation.Availability, EPRShipRepairAvailability::AlreadyCompleted);
	TestFalse(TEXT("Completed repair cannot deduct resources again"), EngineContract->ApplyRepairToSnapshot(Snapshot, Catalog, &Diagnostic));

	FPRShipRepairReceipt Receipt;
	Receipt.ProfileId = FGuid::NewGuid();
	Receipt.RepairProjectId = EngineContract->RepairProjectId;
	Receipt.TransactionId = FGuid::NewGuid();
	Receipt.SettledResourceWallet = Snapshot.ResourceWallet;
	Receipt.SettledShipModules = Snapshot.ShipModules;
	Receipt.SettledStory = Snapshot.Story;
	TestTrue(TEXT("Server-shaped repair receipt is valid"), Receipt.IsValid(&Diagnostic));
	Receipt.TransactionId.Invalidate();
	TestFalse(TEXT("Repair receipt rejects an invalid transaction id"), Receipt.IsValid(&Diagnostic));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRShipRepairPersistenceTest,
	"ProjectRift.ShipRepair.Persistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRShipRepairPersistenceTest::RunTest(const FString& Parameters)
{
	const FString TestRoot = MakeShipRepairTestRoot(TEXT("Persistence"));
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRSaveSubsystem* SaveSubsystem = NewObject<UPRSaveSubsystem>(GameInstance);
	SaveSubsystem->InitializeForAutomation(TestRoot);
	const FPRProfileOperationResult CreateResult = SaveSubsystem->CreateProfile(TEXT("Ship Repair Client"));
	TestTrue(TEXT("Ship repair profile is created"), CreateResult.IsSuccess());

	FPRProfileSnapshot InitialSnapshot;
	InitialSnapshot.ResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 10 } };
	InitialSnapshot.Story.CompletedStoryNodeIds = { TEXT("Story.Prologue.RiftTestHold") };
	TestTrue(TEXT("Initial ship repair snapshot is installed"), SaveSubsystem->ReplaceActiveProfileSnapshot(InitialSnapshot).IsSuccess());
	TestTrue(TEXT("Initial ship repair snapshot is saved"), SaveSubsystem->SaveActiveProfile().IsSuccess());
	TestTrue(TEXT("Ship repair profile is session bound"), SaveSubsystem->BindActiveProfileToSession().IsSuccess());
	TestTrue(TEXT("Save subsystem reports the repair as available"), SaveSubsystem->EvaluateShipRepair(TEXT("Repair.Ship.Engine.Stage1")).IsAvailable());

	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	UPRShipRepairDataAsset* Contract = AssetManager ? AssetManager->LoadShipRepairSync(TEXT("Repair.Ship.Engine.Stage1")) : nullptr;
	TArray<UPRShipRepairDataAsset*> Catalog;
	TestTrue(TEXT("Ship repair catalog loads for persistence test"), AssetManager && AssetManager->LoadShipRepairCatalog(Catalog));
	FPRProfileSnapshot ExpectedSnapshot = InitialSnapshot;
	FString Diagnostic;
	TestTrue(TEXT("Expected repair state can be calculated"), Contract && Contract->ApplyRepairToSnapshot(ExpectedSnapshot, Catalog, &Diagnostic));

	FPRShipRepairReceipt Receipt;
	Receipt.ProfileId = CreateResult.ProfileId;
	Receipt.RepairProjectId = TEXT("Repair.Ship.Engine.Stage1");
	Receipt.TransactionId = FGuid::NewGuid();
	Receipt.SettledResourceWallet = ExpectedSnapshot.ResourceWallet;
	Receipt.SettledShipModules = ExpectedSnapshot.ShipModules;
	Receipt.SettledStory = ExpectedSnapshot.Story;
	const FPRProfileOperationResult ApplyResult = SaveSubsystem->ApplyShipRepairReceipt(Receipt);
	TestTrue(TEXT("Valid ship repair receipt is saved transactionally"), ApplyResult.IsSuccess());
	TestFalse(TEXT("First repair apply is not marked duplicate"), ApplyResult.bAlreadyProcessedRepairTransaction);

	FPRProfileSnapshot AppliedSnapshot;
	SaveSubsystem->GetActiveProfileSnapshot(AppliedSnapshot);
	TestEqual(TEXT("Repair transaction deducts EnergyCrystal exactly once"), AppliedSnapshot.GetResourceCount(TEXT("EnergyCrystal")), 0);
	TestTrue(TEXT("Repair transaction records its durable id"), AppliedSnapshot.ProcessedRepairTransactionIds.Contains(Receipt.TransactionId));
	TestTrue(TEXT("Repair transaction unlocks Chapter One"), AppliedSnapshot.Story.UnlockedChapterIds.Contains(TEXT("Chapter.One")));

	const FPRProfileOperationResult DuplicateResult = SaveSubsystem->ApplyShipRepairReceipt(Receipt);
	TestTrue(TEXT("Duplicate repair receipt is an idempotent success"), DuplicateResult.IsSuccess());
	TestTrue(TEXT("Duplicate repair receipt is reported"), DuplicateResult.bAlreadyProcessedRepairTransaction);

	FPRShipRepairReceipt WrongProfileReceipt = Receipt;
	WrongProfileReceipt.ProfileId = FGuid::NewGuid();
	WrongProfileReceipt.TransactionId = FGuid::NewGuid();
	TestFalse(TEXT("Repair receipt for another profile is rejected"), SaveSubsystem->ApplyShipRepairReceipt(WrongProfileReceipt).IsSuccess());
	FPRShipRepairReceipt WrongProjectReceipt = Receipt;
	WrongProjectReceipt.RepairProjectId = TEXT("Repair.Ship.Unknown");
	WrongProjectReceipt.TransactionId = FGuid::NewGuid();
	TestFalse(TEXT("Repair receipt for an unknown project is rejected"), SaveSubsystem->ApplyShipRepairReceipt(WrongProjectReceipt).IsSuccess());
	FPRShipRepairReceipt TamperedReceipt = Receipt;
	TamperedReceipt.TransactionId = FGuid::NewGuid();
	TamperedReceipt.SettledResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 99 } };
	TestFalse(TEXT("Repair receipt with tampered absolute state is rejected"), SaveSubsystem->ApplyShipRepairReceipt(TamperedReceipt).IsSuccess());
	FPRProfileSnapshot AfterRejectedReceipts;
	SaveSubsystem->GetActiveProfileSnapshot(AfterRejectedReceipts);
	TestEqual(TEXT("Rejected receipts do not alter the saved wallet"), AfterRejectedReceipts.GetResourceCount(TEXT("EnergyCrystal")), 0);

	SaveSubsystem->ReleaseSessionProfileBinding();
	SaveSubsystem->Deinitialize();
	UGameInstance* ReloadGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRSaveSubsystem* ReloadSubsystem = NewObject<UPRSaveSubsystem>(ReloadGameInstance);
	ReloadSubsystem->InitializeForAutomation(TestRoot);
	TestTrue(TEXT("Reloaded ship repair profile activates"), ReloadSubsystem->ActivateAndLoadProfile(CreateResult.ProfileId).IsSuccess());
	TestTrue(TEXT("Reloaded ship repair profile binds"), ReloadSubsystem->BindActiveProfileToSession().IsSuccess());
	const FPRProfileOperationResult RestartDuplicate = ReloadSubsystem->ApplyShipRepairReceipt(Receipt);
	TestTrue(TEXT("Duplicate repair remains idempotent after restart"), RestartDuplicate.IsSuccess());
	TestTrue(TEXT("Restart duplicate is reported"), RestartDuplicate.bAlreadyProcessedRepairTransaction);
	FPRProfileSnapshot ReloadedSnapshot;
	ReloadSubsystem->GetActiveProfileSnapshot(ReloadedSnapshot);
	TestTrue(TEXT("Reload preserves the repair transaction id"), ReloadedSnapshot.ProcessedRepairTransactionIds.Contains(Receipt.TransactionId));
	ReloadSubsystem->Deinitialize();
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRShipRepairWriteFailureTest,
	"ProjectRift.ShipRepair.WriteFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRShipRepairWriteFailureTest::RunTest(const FString& Parameters)
{
	const FString TestRoot = MakeShipRepairTestRoot(TEXT("WriteFailure"));
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRSaveSubsystem* SaveSubsystem = NewObject<UPRSaveSubsystem>(GameInstance);
	SaveSubsystem->InitializeForAutomation(TestRoot);
	const FPRProfileOperationResult CreateResult = SaveSubsystem->CreateProfile(TEXT("Repair Write Failure"));
	FPRProfileSnapshot InitialSnapshot;
	InitialSnapshot.ResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 10 } };
	InitialSnapshot.Story.CompletedStoryNodeIds = { TEXT("Story.Prologue.RiftTestHold") };
	TestTrue(TEXT("Write-failure repair snapshot is installed"), SaveSubsystem->ReplaceActiveProfileSnapshot(InitialSnapshot).IsSuccess());
	TestTrue(TEXT("Write-failure repair snapshot is saved"), SaveSubsystem->SaveActiveProfile().IsSuccess());
	TestTrue(TEXT("Write-failure repair profile binds"), SaveSubsystem->BindActiveProfileToSession().IsSuccess());

	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	UPRShipRepairDataAsset* Contract = AssetManager ? AssetManager->LoadShipRepairSync(TEXT("Repair.Ship.Engine.Stage1")) : nullptr;
	TArray<UPRShipRepairDataAsset*> Catalog;
	TestTrue(TEXT("Write-failure repair catalog loads"), AssetManager && AssetManager->LoadShipRepairCatalog(Catalog));
	FPRProfileSnapshot ExpectedSnapshot = InitialSnapshot;
	FString Diagnostic;
	TestTrue(TEXT("Write-failure expected state calculates"), Contract && Contract->ApplyRepairToSnapshot(ExpectedSnapshot, Catalog, &Diagnostic));
	FPRShipRepairReceipt Receipt;
	Receipt.ProfileId = CreateResult.ProfileId;
	Receipt.RepairProjectId = TEXT("Repair.Ship.Engine.Stage1");
	Receipt.TransactionId = FGuid::NewGuid();
	Receipt.SettledResourceWallet = ExpectedSnapshot.ResourceWallet;
	Receipt.SettledShipModules = ExpectedSnapshot.ShipModules;
	Receipt.SettledStory = ExpectedSnapshot.Story;

	FPRSafeSaveStore Store(TestRoot);
	UPRProfileSave* FuturePrimary = MakeShipRepairStoredProfile(CreateResult.ProfileId, InitialSnapshot, UPRProfileSave::LatestSaveVersion + 1);
	TestTrue(TEXT("Future profile can be installed as a repair write barrier"), WriteShipRepairRawEnvelopedSave(FuturePrimary, Store.GetProfilePrimaryPath(CreateResult.ProfileId)));
	const FPRProfileOperationResult FailureResult = SaveSubsystem->ApplyShipRepairReceipt(Receipt);
	TestEqual(TEXT("Future primary rejects repair write"), FailureResult.Status, EPRProfileOperationStatus::UnsupportedFutureVersion);
	TestTrue(TEXT("Rejected repair remains pending"), SaveSubsystem->HasPendingShipRepairReceipt());
	FPRProfileSnapshot AfterFailure;
	SaveSubsystem->GetActiveProfileSnapshot(AfterFailure);
	TestEqual(TEXT("Rejected repair write preserves memory wallet"), AfterFailure.GetResourceCount(TEXT("EnergyCrystal")), 10);
	TestFalse(TEXT("Rejected repair write does not record transaction id"), AfterFailure.ProcessedRepairTransactionIds.Contains(Receipt.TransactionId));

	UPRProfileSave* RestoredPrimary = MakeShipRepairStoredProfile(CreateResult.ProfileId, InitialSnapshot, UPRProfileSave::LatestSaveVersion);
	TestTrue(TEXT("Valid primary can replace the deterministic write barrier"), WriteShipRepairRawEnvelopedSave(RestoredPrimary, Store.GetProfilePrimaryPath(CreateResult.ProfileId)));
	const FPRProfileOperationResult RetryResult = SaveSubsystem->RetryPendingShipRepairReceipt();
	TestTrue(TEXT("Pending repair succeeds after storage recovers"), RetryResult.IsSuccess());
	TestFalse(TEXT("Successful repair retry clears pending state"), SaveSubsystem->HasPendingShipRepairReceipt());
	FPRProfileSnapshot AfterRetry;
	SaveSubsystem->GetActiveProfileSnapshot(AfterRetry);
	TestEqual(TEXT("Successful repair retry applies the cost"), AfterRetry.GetResourceCount(TEXT("EnergyCrystal")), 0);
	TestTrue(TEXT("Successful repair retry records transaction id"), AfterRetry.ProcessedRepairTransactionIds.Contains(Receipt.TransactionId));

	SaveSubsystem->Deinitialize();
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRShipRepairAcceptancePreparationTest,
	"ProjectRift.ShipRepair.AcceptancePreparation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRShipRepairAcceptancePreparationTest::RunTest(const FString& Parameters)
{
	const FString TestRoot = MakeShipRepairTestRoot(TEXT("AcceptancePreparation"));
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRSaveSubsystem* SaveSubsystem = NewObject<UPRSaveSubsystem>(GameInstance);
	SaveSubsystem->InitializeForAutomation(TestRoot);
	const FPRProfileOperationResult CreateResult = SaveSubsystem->CreateProfile(TEXT("Repair Acceptance"));
	FPRProfileSnapshot InitialSnapshot;
	InitialSnapshot.ResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 3 } };
	TestTrue(TEXT("Acceptance profile starts below the repair cost"), SaveSubsystem->ReplaceActiveProfileSnapshot(InitialSnapshot).IsSuccess());
	TestTrue(TEXT("Development helper safely prepares the acceptance profile"), SaveSubsystem->PrepareShipRepairAcceptanceForDevelopment().IsSuccess());
	FPRProfileSnapshot PreparedSnapshot;
	SaveSubsystem->GetActiveProfileSnapshot(PreparedSnapshot);
	TestEqual(TEXT("Acceptance helper raises EnergyCrystal only to the minimum cost"), PreparedSnapshot.GetResourceCount(TEXT("EnergyCrystal")), 10);
	TestTrue(TEXT("Acceptance helper adds the required story node"), PreparedSnapshot.Story.CompletedStoryNodeIds.Contains(TEXT("Story.Prologue.RiftTestHold")));
	TestTrue(TEXT("Acceptance helper does not complete a ship module"), PreparedSnapshot.ShipModules.IsEmpty());
	TestFalse(TEXT("Acceptance helper does not unlock Chapter One"), PreparedSnapshot.Story.UnlockedChapterIds.Contains(TEXT("Chapter.One")));
	TestTrue(TEXT("Prepared profile is immediately repair eligible"), SaveSubsystem->EvaluateShipRepair(TEXT("Repair.Ship.Engine.Stage1")).IsAvailable());
	SaveSubsystem->Deinitialize();

	UGameInstance* ReloadGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRSaveSubsystem* ReloadSubsystem = NewObject<UPRSaveSubsystem>(ReloadGameInstance);
	ReloadSubsystem->InitializeForAutomation(TestRoot);
	TestTrue(TEXT("Prepared acceptance profile reloads from disk"), ReloadSubsystem->ActivateAndLoadProfile(CreateResult.ProfileId).IsSuccess());
	FPRProfileSnapshot ReloadedSnapshot;
	ReloadSubsystem->GetActiveProfileSnapshot(ReloadedSnapshot);
	TestEqual(TEXT("Prepared minimum resources persist after restart"), ReloadedSnapshot.GetResourceCount(TEXT("EnergyCrystal")), 10);
	TestTrue(TEXT("Prepared story prerequisite persists after restart"), ReloadedSnapshot.Story.CompletedStoryNodeIds.Contains(TEXT("Story.Prologue.RiftTestHold")));
	ReloadSubsystem->Deinitialize();
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRShipRepairUiContractTest,
	"ProjectRift.ShipRepair.UiContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRShipRepairUiContractTest::RunTest(const FString& Parameters)
{
	const UClass* WidgetClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRShipRepairWidget"));
	TestNotNull(TEXT("Formal native ship repair widget is registered"), WidgetClass);
	if (WidgetClass)
	{
		TestNotNull(TEXT("Repair widget exposes refresh"), WidgetClass->FindFunctionByName(TEXT("RefreshRepairDisplay")));
		TestNotNull(TEXT("Repair widget exposes repair request"), WidgetClass->FindFunctionByName(TEXT("RequestDisplayedRepair")));
		TestNotNull(TEXT("Repair widget exposes a close action"), WidgetClass->FindFunctionByName(TEXT("RequestClose")));
		TestNotNull(TEXT("Repair widget reports development helper visibility"), WidgetClass->FindFunctionByName(TEXT("IsAcceptancePreparationAvailable")));
	}
	UPRShipRepairWidget* NativeWidget = NewObject<UPRShipRepairWidget>(GetTransientPackage());
	TestTrue(TEXT("Development build exposes the clearly marked acceptance helper"), NativeWidget->IsAcceptancePreparationAvailable());
	TestEqual(TEXT("Formal repair panel targets the registered engine repair"), NativeWidget->GetDisplayedRepairProjectId(), FName(TEXT("Repair.Ship.Engine.Stage1")));
	TestTrue(TEXT("Formal repair panel accepts keyboard focus so Escape can be consumed by the widget"), NativeWidget->IsFocusable());

	const UClass* ControllerClass = APRPlayerController::StaticClass();
	TestNotNull(TEXT("Controller exposes repair panel toggle"), ControllerClass->FindFunctionByName(TEXT("ToggleShipRepairPanel")));
	TestNotNull(TEXT("Controller exposes repair panel show"), ControllerClass->FindFunctionByName(TEXT("ShowShipRepairPanel")));
	TestNotNull(TEXT("Controller exposes repair panel hide"), ControllerClass->FindFunctionByName(TEXT("HideShipRepairPanel")));
	TestNotNull(TEXT("Controller exposes repair panel visibility"), ControllerClass->FindFunctionByName(TEXT("IsShipRepairPanelVisible")));
	return true;
}

#endif
