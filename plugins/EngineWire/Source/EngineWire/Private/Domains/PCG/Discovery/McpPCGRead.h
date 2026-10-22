#pragma once

#include "Domains/PCG/McpPCGPrivate.h"

#if MCP_HAS_PCG
class UEdGraph;
namespace McpPCGHandlers
{
UEdGraph* FindExistingPCGEditorGraph(UPCGGraph* Graph, bool& bOutAmbiguous);
TSharedPtr<FJsonObject> ReadPCGEditorGraphSnapshot(UPCGGraph* Graph);
bool IsPCGAuthoredObject(const UObject* Object, const UObject* Root);
const TArray<FName>& PCGRevisionExtraProperties();
TSharedPtr<FJsonObject> ReadPCGSettings(UObject* Settings);
TSharedPtr<FJsonObject> ReadPCGNode(UPCGGraph* Graph, UPCGNode* Node, bool bIncludeSettings);
TSharedPtr<FJsonObject> ReadPCGGraph(UPCGGraph* Graph, bool bIncludeSettings, int32 Offset = 0, int32 Limit = MAX_int32,
    int32 EdgeOffset = 0, int32 EdgeLimit = MAX_int32, const FString& NodeId = FString());
FString GetPCGGraphRevision(UPCGGraph* Graph);
bool CheckPCGGraphRevision(UPCGGraph* Graph, const TSharedPtr<FJsonObject>& Payload, FString& OutError);
bool HandlePCGDiscovery(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
}
#endif
