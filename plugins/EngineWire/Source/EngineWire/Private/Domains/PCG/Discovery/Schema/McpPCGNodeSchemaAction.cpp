#include "Domains/PCG/Discovery/Schema/McpPCGNodeSchemaAction.h"

#if MCP_HAS_PCG
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
#include "Domains/PCG/Data/McpPCGDataActions.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Domains/PCG/Discovery/Schema/McpPCGPropSchema.h"
#include "Domains/PCG/Discovery/Schema/McpPCGSchemaScope.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace McpPCGHandlers
{
TSharedPtr<FJsonObject> ReadPCGNodeSchema(UPCGGraph* Graph, UPCGNode* Node, const FString& ScopePath,
    int32 Offset, int32 Limit, FString& OutError)
{
    OutError.Reset();
    FPCGReadBudget Budget;
    FPCGSchemaScope Scope;
    if (!ResolvePCGSchemaScope(Graph, Node, ScopePath, Scope, OutError)) return nullptr;
    const FString Revision = GetPCGGraphRevision(Graph);
    if (Revision.IsEmpty())
    {
        OutError = TEXT("The complete authored graph revision is unavailable within its existing read bounds.");
        return nullptr;
    }
    TSharedPtr<FJsonObject> Schema;
    if (!DescribePCGPropertySchema(Scope.Type, Offset, Limit, Schema, OutError)) return nullptr;
    const FString SettingsClass = Scope.Settings->GetClass()->GetPathName();
    const FString ScopeType = Scope.Type->GetPathName();
    if (SettingsClass.Len() > 1024 || ScopeType.Len() > 1024 || !Budget.IsWithinTime())
    {
        OutError = TEXT("The instance schema exceeds its identity-string or cooperative time bounds.");
        return nullptr;
    }
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"), Node->GetName());
    Result->SetStringField(TEXT("revision"), Revision);
    Result->SetStringField(TEXT("settingsClass"), SettingsClass);
    Result->SetStringField(TEXT("scopePath"), Scope.Path);
    Result->SetStringField(TEXT("scopeType"), ScopeType);
    const TCHAR* ScopeKind = Scope.Path.IsEmpty() ? TEXT("settings") :
        Cast<UClass>(Scope.Type) ? TEXT("owned_object") : TEXT("struct");
    Result->SetStringField(TEXT("scopeKind"), ScopeKind);
    Result->SetStringField(TEXT("schemaSource"), TEXT("existing_node_instance_metadata_only"));
    const bool bBlueprint = Scope.Settings->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
    Result->SetBoolField(TEXT("isBlueprintSettings"), bBlueprint);
    Result->SetStringField(TEXT("editPolicy"), bBlueprint ? TEXT("blueprint_callbacks_not_sandboxed") : TEXT("existing_native_staged_preflight_required"));
    Result->SetBoolField(TEXT("metadataOnly"), true);
    Result->SetBoolField(TEXT("valuesIncluded"), false);
    Result->SetBoolField(TEXT("loadedObjectsOnly"), true);
    Result->SetStringField(TEXT("consistency"), TEXT("per_call_instance_schema_not_value_snapshot"));
    Result->SetObjectField(TEXT("schema"), Schema);
    if (GetPCGGraphRevision(Graph) != Revision || !Budget.IsWithinTime())
    {
        OutError = TEXT("The authored graph changed during schema reading or its cooperative budget expired.");
        return nullptr;
    }
    return Result;
}

bool HandlePCGNodeSchema(UEngineWireSubsystem* Bridge, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FPCGReadBudget Budget;
    FString RawGraph, NodeId, ScopePath, Error;
    int32 Offset = 0, Limit = 16;
    if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("graphPath"), RawGraph) || RawGraph.IsEmpty() || RawGraph.Len() > 2048 ||
        !Payload->TryGetStringField(TEXT("nodeId"), NodeId) || NodeId.IsEmpty() || NodeId.Len() >= NAME_SIZE ||
        (Payload->HasField(TEXT("scopePath")) && !Payload->TryGetStringField(TEXT("scopePath"), ScopePath)) || ScopePath.Len() > 512 ||
        !ReadPCGDataInt(Payload, TEXT("offset"), 0, 0, MAX_int32, Offset, Error) ||
        !ReadPCGDataInt(Payload, TEXT("limit"), 16, 1, 32, Limit, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error.IsEmpty() ? TEXT("An exact graph/node and a bounded relative schema scope/page are required.") : Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FNormalizedAssetPath Normalized = NormalizeAssetPath(RawGraph);
    if (!Normalized.bIsValid)
    {
        Bridge->SendAutomationError(Socket, RequestId, Normalized.ErrorMessage, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // Only this already-loaded, exact object is eligible. Existing explicit load workflows remain separate.
    UPCGGraph* Graph = FindObject<UPCGGraph>(nullptr, *ToObjectPath(Normalized.Path));
    if (!IsValid(Graph) || Graph->HasAnyFlags(RF_ClassDefaultObject) || Graph->GetNodes().Num() > FPCGReadBudget::MaxNodes)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("The exact bounded PCG graph must already be loaded; no asset or class load was attempted."), TEXT("PCG_SCHEMA_UNAVAILABLE"));
        return true;
    }
    // Do not call the legacy title/index resolver: native title evaluation is unnecessary for an exact identity read.
    UPCGNode* Node = nullptr;
    for (UPCGNode* Candidate : {Graph->GetInputNode(), Graph->GetOutputNode()})
        if (IsValid(Candidate) && Candidate->GetName() == NodeId) Node = Candidate;
    if (!Node)
        for (UPCGNode* Candidate : Graph->GetNodes())
            if (IsValid(Candidate) && Candidate->GetName() == NodeId) { Node = Candidate; break; }
    if (!Node)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Use the exact backend nodeId returned by graph discovery, not a title, index or boundary alias."), TEXT("PCG_SCHEMA_UNAVAILABLE"));
        return true;
    }
    auto Result = ReadPCGNodeSchema(Graph, Node, ScopePath, Offset, Limit, Error);
    if (Result.IsValid()) Result->SetStringField(TEXT("graphPath"), Normalized.Path);
    FString Encoded;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded);
    if (!Result.IsValid() || !FJsonSerializer::Serialize(Result.ToSharedRef(), Writer) || Encoded.Len() > 80000 || !Budget.IsWithinTime())
    {
        Bridge->SendAutomationError(Socket, RequestId, Error.IsEmpty() ? TEXT("The complete instance schema exceeds its serialized-result or cooperative time budget.") : Error, TEXT("PCG_SCHEMA_UNAVAILABLE"));
        return true;
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("Existing PCG node-instance schema metadata read; values and edit support are not inferred."), Result);
    return true;
}
}
#else
namespace McpPCGHandlers
{
bool HandlePCGNodeSchema(UEngineWireSubsystem* Bridge, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    Bridge->SendAutomationError(Socket, RequestId, TEXT("PCG node-instance schema reads require the supported UE 5.7+ public APIs."), TEXT("NOT_SUPPORTED"));
    return true;
}
}
#endif
#endif
