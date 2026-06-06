#pragma once

#include "McpAutomationBridge_GASRequestContext.h"

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASComponents(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASAttributes(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASAbilityBasics(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASAbilityTargeting(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASAbilityTasks(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASAbilityPolicies(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASEffectsMagnitude(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASEffectsExecutionCues(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASEffectsStackingTags(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASCueNotify(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASCueEffects(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASTagAssets(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASInfo(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASAbilitySets(const FGASRequestContext& Context, const FString& SubAction);
bool HandleGASAbilityGrantAndExecution(const FGASRequestContext& Context, const FString& SubAction);
}
#endif
