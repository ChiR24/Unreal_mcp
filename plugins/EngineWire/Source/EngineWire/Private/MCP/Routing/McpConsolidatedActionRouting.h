#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpConsolidatedActions
{
inline FString GetPayloadSubAction(const TSharedPtr<FJsonObject>& Payload)
{
	FString SubAction;
	if (Payload.IsValid())
	{
		if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty())
		{
			Payload->TryGetStringField(TEXT("action"), SubAction);
		}
	}
	SubAction = SubAction.ToLower();
	SubAction.ReplaceInline(TEXT("-"), TEXT("_"));
	SubAction.ReplaceInline(TEXT(" "), TEXT("_"));
	return SubAction;
}

inline TSharedPtr<FJsonObject> WithPayloadSubAction(const TSharedPtr<FJsonObject>& Payload, const FString& SubAction)
{
	if (!Payload.IsValid() || SubAction.IsEmpty())
	{
		return Payload;
	}

	TSharedPtr<FJsonObject> RoutedPayload = MakeShared<FJsonObject>();
	RoutedPayload->Values = Payload->Values;
	RoutedPayload->SetStringField(TEXT("action"), SubAction);
	RoutedPayload->SetStringField(TEXT("subAction"), SubAction);
	return RoutedPayload;
}

}

#include "MCP/Routing/McpConsolidatedActionRoutingAI.h"
#include "MCP/Routing/McpConsolidatedActionRoutingAnimationSystem.h"
#include "MCP/Routing/McpConsolidatedActionRoutingAssets.h"
#include "MCP/Routing/McpConsolidatedActionRoutingBlueprints.h"
#include "MCP/Routing/McpConsolidatedActionRoutingEnvironment.h"
#include "MCP/Routing/McpConsolidatedActionRoutingNetworkingLevel.h"

// Sub-route predicates (taken by pointer in the handler-registration tables).
// FString == is case-insensitive, so "Set_Padding" matches the lowercase entries.
namespace McpConsolidatedActions
{
inline bool IsAnimationAuthoringAction(const FString& Action) { return AnimationAuthoring().Contains(Action); }
inline bool IsAudioAuthoringAction(const FString& Action) { return AudioAuthoring().Contains(Action); }
inline bool IsGameFrameworkAction(const FString& Action) { return GameFramework().Contains(Action); }
inline bool IsInputAction(const FString& Action) { return Input().Contains(Action); }
inline bool IsLightingAction(const FString& Action) { return Lighting().Contains(Action); }
inline bool IsPerformanceAction(const FString& Action) { return Performance().Contains(Action); }
inline bool IsRenderingAction(const FString& Action) { return Rendering().Contains(Action); }
inline bool IsSessionAction(const FString& Action) { return Sessions().Contains(Action); }
inline bool IsSkeletonAction(const FString& Action) { return Skeleton().Contains(Action); }
inline bool IsSplineAction(const FString& Action) { return Splines().Contains(Action); }
inline bool IsSystemUiAction(const FString& Action) { return SystemUi().Contains(Action); }
inline bool IsVolumeAction(const FString& Action) { return Volumes().Contains(Action); }
inline bool IsWidgetAuthoringAction(const FString& Action) { return WidgetAuthoring().Contains(Action); }
}
