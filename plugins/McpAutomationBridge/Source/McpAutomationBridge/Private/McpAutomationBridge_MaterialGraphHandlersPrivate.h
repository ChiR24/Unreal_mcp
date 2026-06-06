#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UClass;
class UMaterial;
class UMaterialExpression;
class UMcpAutomationBridgeSubsystem;

#if WITH_EDITOR
namespace McpMaterialGraphHandlers
{
UMaterialExpression* FindExpressionByIdOrNameOrIndex(
    UMaterial& Material,
    const FString& IdOrName,
    int32 Index = -1);
UMaterialExpression* FindExpressionByPayload(
    UMaterial& Material,
    const TSharedPtr<FJsonObject>& Payload,
    const FString& IdField,
    const FString& IndexField);
UClass* ResolveGraphNodeClass(const FString& NodeType, bool bUseExtendedAliases);
UClass* ResolveBatchNodeClass(const FString& NodeType);
UClass* ResolveExpressionClass(const FString& ExpressionClassName);
void AddExpressionToMaterial(UMaterial& Material, UMaterialExpression& Expression);
bool SetMainMaterialInputExpression(
    UMaterial& Material,
    const FString& InputName,
    UMaterialExpression* Expression);

bool HandleAddNode(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UMaterial& Material);
bool HandleRemoveNode(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UMaterial& Material);
bool HandleConnectNodes(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UMaterial& Material);
bool HandleBreakConnections(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UMaterial& Material);
bool HandleGetNodeDetails(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UMaterial& Material);
}
#endif
