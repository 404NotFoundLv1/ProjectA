#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PRStatusGameplayEffect.generated.h"

/** Base class for replace-and-refresh negative status effects. */
UCLASS(Abstract)
class PROJECTA_API UPRStatusGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRStatusGameplayEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual FGameplayTag GetStatusTag() const PURE_VIRTUAL(UPRStatusGameplayEffect::GetStatusTag, return FGameplayTag(););
};

/** Five-second periodic pollution damage effect by default. */
UCLASS()
class PROJECTA_API UPRPollutionStatusGameplayEffect : public UPRStatusGameplayEffect
{
	GENERATED_BODY()

public:
	UPRPollutionStatusGameplayEffect(const FObjectInitializer& ObjectInitializer);
	virtual FGameplayTag GetStatusTag() const override;
};

/** Multiplicative MoveSpeed status effect. */
UCLASS()
class PROJECTA_API UPRSlowStatusGameplayEffect : public UPRStatusGameplayEffect
{
	GENERATED_BODY()

public:
	UPRSlowStatusGameplayEffect(const FObjectInitializer& ObjectInitializer);
	virtual FGameplayTag GetStatusTag() const override;
};

/** State.Stunned carrier; movement and ability reactions are owned by combatants. */
UCLASS()
class PROJECTA_API UPRStunStatusGameplayEffect : public UPRStatusGameplayEffect
{
	GENERATED_BODY()

public:
	UPRStunStatusGameplayEffect(const FObjectInitializer& ObjectInitializer);
	virtual FGameplayTag GetStatusTag() const override;
};

/** Enemy-side status replicated by Medic recon scans. */
UCLASS()
class PROJECTA_API UPRReconRevealGameplayEffect : public UPRStatusGameplayEffect
{
	GENERATED_BODY()

public:
	UPRReconRevealGameplayEffect(const FObjectInitializer& ObjectInitializer);
	virtual FGameplayTag GetStatusTag() const override;
};

/** Three seconds of energy drain and a modest mobility penalty from a Parasite. */
UCLASS()
class PROJECTA_API UPRParasitizedStatusGameplayEffect : public UPRStatusGameplayEffect
{
	GENERATED_BODY()
public:
	UPRParasitizedStatusGameplayEffect(const FObjectInitializer& ObjectInitializer);
	virtual FGameplayTag GetStatusTag() const override;
};

/** Blocks only role modules; weapon fire and dodge stay available. */
UCLASS()
class PROJECTA_API UPRAbilityDisruptedStatusGameplayEffect : public UPRStatusGameplayEffect
{
	GENERATED_BODY()
public:
	UPRAbilityDisruptedStatusGameplayEffect(const FObjectInitializer& ObjectInitializer);
	virtual FGameplayTag GetStatusTag() const override;
};

/** Eight-second anti-repeat grace carrier for Ability Disruptor hits. */
UCLASS()
class PROJECTA_API UPRDisruptionGraceGameplayEffect : public UPRStatusGameplayEffect
{
	GENERATED_BODY()
public:
	UPRDisruptionGraceGameplayEffect(const FObjectInitializer& ObjectInitializer);
	virtual FGameplayTag GetStatusTag() const override;
};
