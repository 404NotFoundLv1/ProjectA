#include "Items/PRWarehouseComponent.h"

UPRWarehouseComponent::UPRWarehouseComponent()
{
	SetMaximumSlots(200);
}

void UPRWarehouseComponent::BeginPlay()
{
	Super::BeginPlay();
	OnInventoryChanged.AddDynamic(this, &UPRWarehouseComponent::HandleInventoryChanged);
}

void UPRWarehouseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnInventoryChanged.RemoveDynamic(this, &UPRWarehouseComponent::HandleInventoryChanged);
	Super::EndPlay(EndPlayReason);
}

void UPRWarehouseComponent::HandleInventoryChanged()
{
	OnWarehouseChanged.Broadcast();
}
