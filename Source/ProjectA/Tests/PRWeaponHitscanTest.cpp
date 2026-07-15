#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRCombatFeedbackTypes.h"
#include "Abilities/PRAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Characters/PRCharacter.h"
#include "Combat/PRCombatFeedbackComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/PRGameplayTags.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Enemies/PREnemyCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRWeaponDataAsset.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "Weapons/PRWeaponComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRWeaponHitscanTest,
	"ProjectRift.Weapons.Hitscan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
struct FHitscanPlayer
{
	APRPlayerState* PlayerState = nullptr;
	APRCharacter* Character = nullptr;
	APlayerController* Controller = nullptr;
};

FHitscanPlayer SpawnHitscanPlayer(UWorld* World, const FVector& Location)
{
	FHitscanPlayer Result;
	if (!World)
	{
		return Result;
	}
	Result.PlayerState = World->SpawnActor<APRPlayerState>();
	Result.Character = World->SpawnActor<APRCharacter>(Location, FRotator::ZeroRotator);
	Result.Controller = World->SpawnActor<APlayerController>();
	if (Result.PlayerState && Result.Character && Result.Controller)
	{
		Result.Controller->SetPlayerState(Result.PlayerState);
		Result.Controller->Possess(Result.Character);
	}
	return Result;
}

void AimCameraAt(APRCharacter* Character, const FVector& CameraLocation, const FVector& AimPoint)
{
	if (UCameraComponent* Camera = Character ? Character->GetFollowCamera() : nullptr)
	{
		const FRotator AimRotation = (AimPoint - CameraLocation).Rotation();
		Camera->SetWorldLocationAndRotation(CameraLocation, AimRotation);
		if (AController* Controller = Character->GetController())
		{
			Controller->SetControlRotation(AimRotation);
		}
	}
}

void TickHitscanWorld(UWorld* World, const float DurationSeconds)
{
	const float StepSeconds = 0.02f;
	for (float Elapsed = 0.0f; World && Elapsed < DurationSeconds; Elapsed += StepSeconds)
	{
		++GFrameCounter;
		World->Tick(
			ELevelTick::LEVELTICK_All,
			FMath::Min(StepSeconds, DurationSeconds - Elapsed));
	}
}
}

bool FPRWeaponHitscanTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Hitscan test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}
	TestTrue(TEXT("Hitscan test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	const FHitscanPlayer Shooter = SpawnHitscanPlayer(World, FVector::ZeroVector);
	TestNotNull(TEXT("Hitscan shooter PlayerState exists"), Shooter.PlayerState);
	TestNotNull(TEXT("Hitscan shooter Character exists"), Shooter.Character);
	TestTrue(TEXT("Hitscan shooter GAS is initialized"), Shooter.Character && Shooter.Character->IsAbilitySystemInitialized());
	if (!Shooter.PlayerState || !Shooter.Character || !Shooter.Controller)
	{
		return false;
	}

	TStrongObjectPtr<UPRWeaponDataAsset> Rifle{ NewObject<UPRWeaponDataAsset>(GetTransientPackage()) };
	TStrongObjectPtr<UPRItemDataAsset> AmmoData{ NewObject<UPRItemDataAsset>(GetTransientPackage()) };
	Rifle->ItemId = TEXT("HitscanTestRifle");
	Rifle->AmmoItemId = TEXT("HitscanTestAmmo");
	Rifle->MagazineCapacity = 12;
	Rifle->InitialReserveAmmo = 0;
	Rifle->BaseDamage = 12.0f;
	Rifle->DamageType = ProjectRiftGameplayTags::Damage_Type_Physical;
	Rifle->Range = 12000.0f;
	Rifle->MinFireInterval = 0.0f;
	Rifle->HipSpreadDegrees = 0.0f;
	Rifle->AimSpreadDegrees = 0.0f;
	Rifle->WeaponActorClass = nullptr;
	TestEqual(TEXT("Test rifle defaults to a Light hit reaction"), Rifle->HitReaction.Strength, EPRHitReactionStrength::Light);
	TestTrue(TEXT("Test rifle defaults to a 0.12 second hit reaction"), FMath::IsNearlyEqual(Rifle->HitReaction.DurationSeconds, 0.12f));
	AmmoData->ItemId = Rifle->AmmoItemId;
	AmmoData->ItemType = EPRItemType::Ammunition;
	AmmoData->MaxStackCount = 120;

	UPRInventoryComponent* Inventory = Shooter.PlayerState->GetInventoryComponent();
	UPRWeaponComponent* Weapon = Shooter.PlayerState->GetWeaponComponent();
	Inventory->SetItemDataAssets({ Rifle.Get(), AmmoData.Get() });
	FPRItemInstance RifleItem;
	RifleItem.ItemId = Rifle->ItemId;
	RifleItem.Count = 1;
	FPRItemInstance Ammo;
	Ammo.ItemId = Rifle->AmmoItemId;
	Ammo.Count = 12;
	TestTrue(TEXT("Hitscan rifle enters inventory"), Inventory->AddItem(RifleItem));
	TestTrue(TEXT("Hitscan ammo enters inventory"), Inventory->AddItem(Ammo));
	TestTrue(TEXT("Hitscan rifle equips"), Weapon->EquipWeaponFromInventory(Rifle->ItemId));

	APREnemyCharacter* Enemy = World->SpawnActor<APREnemyCharacter>(FVector(1000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Hitscan enemy exists"), Enemy);
	if (!Enemy)
	{
		return false;
	}
	UPRCombatFeedbackComponent* EnemyFeedback = Enemy->FindComponentByClass<UPRCombatFeedbackComponent>();
	UPRAbilitySystemComponent* EnemyASC = Enemy->GetProjectRiftAbilitySystemComponent();
	TestNotNull(TEXT("Hitscan enemy owns the shared feedback component"), EnemyFeedback);
	TestNotNull(TEXT("Hitscan enemy owns the shared ASC"), EnemyASC);
	if (!EnemyFeedback || !EnemyASC)
	{
		return false;
	}
	const FVector EnemyAimPoint = Enemy->GetActorLocation();
	AimCameraAt(Shooter.Character, FVector(-400.0f, 100.0f, 100.0f), EnemyAimPoint);

	AStaticMeshActor* Cover = World->SpawnActor<AStaticMeshActor>(FVector(220.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	TestNotNull(TEXT("Cover cube mesh loads"), Cube);
	if (Cover && Cube)
	{
		Cover->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Cover->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Cover->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Cover->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
		Cover->SetActorScale3D(FVector(0.25f));
	}
	const float EnemyHealthBeforeCover = Enemy->GetHealth();
	TestEqual(TEXT("Muzzle obstruction consumes the accepted shot"), Weapon->TryFire(), EPRWeaponFireResult::FiredMiss);
	TestTrue(TEXT("Muzzle obstruction prevents camera-over-wall damage"), FMath::IsNearlyEqual(Enemy->GetHealth(), EnemyHealthBeforeCover));
	TestEqual(TEXT("Obstructed accepted shot consumes one round"), Weapon->GetMagazineAmmo(), 11);

	if (Cover)
	{
		Cover->SetActorEnableCollision(false);
		Cover->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cover->Destroy();
	}
	const float EnemyHealthBeforeHit = Enemy->GetHealth();
	const int32 FeedbackCountBeforeHit = EnemyFeedback->FeedbackDispatchCount;
	TestEqual(TEXT("Clear dual trace hits the hostile enemy"), Weapon->TryFire(), EPRWeaponFireResult::FiredHit);
	const float ExpectedPhysicalDamage = 12.0f * 1.1f;
	TestTrue(TEXT("Hitscan delegates to unified physical Damage Execution"), FMath::IsNearlyEqual(Enemy->GetHealth(), EnemyHealthBeforeHit - ExpectedPhysicalDamage, 0.01f));
	TestEqual(TEXT("Hostile hit consumes one round"), Weapon->GetMagazineAmmo(), 10);
	TestEqual(
		TEXT("Accepted hostile rifle hit dispatches exactly one resolved target feedback event"),
		EnemyFeedback->FeedbackDispatchCount,
		FeedbackCountBeforeHit + 1);
	TestEqual(TEXT("Rifle hit uses its Light reaction definition"), EnemyFeedback->GetActiveStaggerStrength(), EPRHitReactionStrength::Light);
	TestTrue(TEXT("Rifle hit grants State.HitStaggered"), EnemyASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	const FHitResult* RecordedRifleHit = EnemyFeedback->LastDamageEffectContext.GetHitResult();
	TestNotNull(TEXT("Rifle structured damage stores the real muzzle hit in EffectContext"), RecordedRifleHit);
	if (RecordedRifleHit)
	{
		TestEqual(TEXT("Rifle EffectContext preserves the actual hit actor"), RecordedRifleHit->GetActor(), static_cast<AActor*>(Enemy));
		TestTrue(
			TEXT("Rifle EffectContext preserves an impact point on the hit enemy"),
			FVector::Dist(RecordedRifleHit->ImpactPoint, Enemy->GetActorLocation()) <= 100.0f);
		TestTrue(TEXT("Rifle EffectContext preserves a normalized impact normal"), RecordedRifleHit->ImpactNormal.IsNormalized());
		TestEqual(
			TEXT("Rifle EffectContext preserves the original player avatar instigator"),
			EnemyFeedback->LastDamageEffectContext.GetOriginalInstigator(),
			static_cast<AActor*>(Shooter.Character));
	}
	TickHitscanWorld(World, 0.14f);
	TestFalse(TEXT("Default 0.12 second rifle stagger expires"), EnemyASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));

	Enemy->SetActorLocation(FVector(1000.0f, 600.0f, 0.0f));
	const FHitscanPlayer Friendly = SpawnHitscanPlayer(World, FVector(1000.0f, 0.0f, 0.0f));
	TestNotNull(TEXT("Friendly target exists"), Friendly.Character);
	const float FriendlyHealthBefore = Friendly.PlayerState && Friendly.PlayerState->GetAttributeSet()
		? Friendly.PlayerState->GetAttributeSet()->GetHealth()
		: 0.0f;
	AimCameraAt(Shooter.Character, FVector(-400.0f, 100.0f, 100.0f), Friendly.Character ? Friendly.Character->GetActorLocation() : FVector(1000.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Friendly aim still consumes an accepted shot"), Weapon->TryFire(), EPRWeaponFireResult::FiredMiss);
	TestTrue(TEXT("Shared combat validation rejects player friendly fire"), Friendly.PlayerState && FMath::IsNearlyEqual(Friendly.PlayerState->GetAttributeSet()->GetHealth(), FriendlyHealthBefore));
	TestEqual(TEXT("Friendly shot consumes one round"), Weapon->GetMagazineAmmo(), 9);

	if (Friendly.Character)
	{
		Friendly.Character->SetActorLocation(FVector(0.0f, 1000.0f, 0.0f));
	}
	Enemy->SetActorLocation(FVector(13000.0f, 0.0f, 0.0f));
	AimCameraAt(Shooter.Character, FVector(-400.0f, 100.0f, 100.0f), Enemy->GetActorLocation());
	const float EnemyHealthBeforeRangeShot = Enemy->GetHealth();
	TestEqual(TEXT("Target beyond configured range is a miss"), Weapon->TryFire(), EPRWeaponFireResult::FiredMiss);
	TestTrue(TEXT("Out-of-range target takes no damage"), FMath::IsNearlyEqual(Enemy->GetHealth(), EnemyHealthBeforeRangeShot));
	TestEqual(TEXT("Out-of-range accepted shot consumes one round"), Weapon->GetMagazineAmmo(), 8);

	return true;
}

#endif
