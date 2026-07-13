#pragma once

#include "Components/CanvasPanelSlot.h"
#include "Math/Vector2D.h"

struct FPRDebugUILayout final
{
	static FAnchors ProfileAnchors() { return FAnchors(1.0f, 0.0f); }
	static FVector2D ProfileAlignment() { return FVector2D(1.0f, 0.0f); }
	static FVector2D ProfilePosition() { return FVector2D(-24.0f, 24.0f); }
	static FVector2D ProfileSize() { return FVector2D(520.0f, 432.0f); }

	static FAnchors GASAnchors() { return FAnchors(0.0f, 0.0f); }
	static FVector2D GASAlignment() { return FVector2D::ZeroVector; }
	static FVector2D GASPosition() { return FVector2D(24.0f, 24.0f); }
	static FVector2D GASSize() { return FVector2D(360.0f, 200.0f); }

	static FAnchors LobbyReadyAnchors() { return FAnchors(0.0f, 1.0f); }
	static FVector2D LobbyReadyAlignment() { return FVector2D(0.0f, 1.0f); }
	static FVector2D LobbyReadyPosition() { return FVector2D(24.0f, -24.0f); }
	static FVector2D LobbyReadySize() { return FVector2D(420.0f, 160.0f); }
};
