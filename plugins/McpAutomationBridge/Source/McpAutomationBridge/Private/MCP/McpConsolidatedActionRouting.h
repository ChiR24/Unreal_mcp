#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpConsolidatedActions
{
inline void AppendUniqueActions(TArray<FString>& Target, const TArray<FString>& Source)
{
	for (const FString& Action : Source)
	{
		Target.AddUnique(Action);
	}
}

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

inline bool ContainsAction(const TArray<FString>& Actions, const FString& Action)
{
	return Actions.Contains(Action);
}
}

#include "MCP/McpConsolidatedActionRoutingAI.h"
#include "MCP/McpConsolidatedActionRoutingAnimationSystem.h"
#include "MCP/McpConsolidatedActionRoutingAssets.h"
#include "MCP/McpConsolidatedActionRoutingBlueprints.h"
#include "MCP/McpConsolidatedActionRoutingEnvironment.h"
#include "MCP/McpConsolidatedActionRoutingNetworkingLevel.h"

namespace McpConsolidatedActions
{
inline bool IsMaterialAuthoringAction(const FString& Action) { return ContainsAction(MaterialAuthoring(), Action); }
inline bool IsTextureAction(const FString& Action) { return ContainsAction(Texture(), Action); }
inline bool IsWidgetAuthoringAction(const FString& Action) { return ContainsAction(WidgetAuthoring(), Action); }
inline bool IsAnimationAuthoringAction(const FString& Action) { return ContainsAction(AnimationAuthoring(), Action); }
inline bool IsAudioAuthoringAction(const FString& Action) { return ContainsAction(AudioAuthoring(), Action); }
inline bool IsLightingAction(const FString& Action) { return ContainsAction(Lighting(), Action); }
inline bool IsSplineAction(const FString& Action) { return ContainsAction(Splines(), Action); }
inline bool IsSkeletonAction(const FString& Action) { return ContainsAction(Skeleton(), Action); }
inline bool IsPerformanceAction(const FString& Action) { return ContainsAction(Performance(), Action); }
inline bool IsInputAction(const FString& Action) { return ContainsAction(Input(), Action); }
inline bool IsGameFrameworkAction(const FString& Action) { return ContainsAction(GameFramework(), Action); }
inline bool IsSessionAction(const FString& Action) { return ContainsAction(Sessions(), Action); }
inline bool IsVolumeAction(const FString& Action) { return ContainsAction(Volumes(), Action); }
inline bool IsBehaviorTreeAction(const FString& Action) { return ContainsAction(BehaviorTree(), Action); }
inline bool IsNavigationAction(const FString& Action) { return ContainsAction(Navigation(), Action); }
inline bool IsPCGAction(const FString& Action) { return ContainsAction(PCG(), Action); }
}
