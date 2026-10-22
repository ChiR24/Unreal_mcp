#include "Domains/PCG/Editing/McpPCGEditState.h"
#include "Domains/PCG/Editing/McpPCGConnect.h"
#include "MCP/Gateway/McpNativeGatewayCanonicalJson.h"

#if MCP_HAS_PCG
#include "PCGInputOutputSettings.h"

namespace McpPCGHandlers
{
bool HasOwnedPCGNodeSettings(UPCGGraph* Graph, UPCGNode* Node)
{
    if (!Graph || !Node || Node->GetOuter() != Graph) return false;
    const bool bBoundary = Node == Graph->GetInputNode() || Node == Graph->GetOutputNode();
    if (!bBoundary && !Graph->GetNodes().Contains(Node)) return false;
    UPCGSettings* Settings = Node->GetSettings();
    if (!Settings || Settings->HasAnyFlags(RF_ClassDefaultObject)) return false;
    if (!bBoundary) return Settings->GetOuter() == Node;
    const auto* BoundarySettings = Cast<UPCGGraphInputOutputSettings>(Settings);
    UPCGNode* OtherBoundary = Node == Graph->GetInputNode() ? Graph->GetOutputNode() : Graph->GetInputNode();
    return BoundarySettings && BoundarySettings->IsInput() == (Node == Graph->GetInputNode()) &&
        (Settings->GetOuter() == Node || Settings->GetOuter() == Graph) &&
        (!OtherBoundary || OtherBoundary->GetSettings() != Settings);
}

bool ValidatePCGNodeSettings(UPCGGraph* Graph, UPCGNode* Node, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
    TStrongObjectPtr<UPCGGraph> Staged(DuplicateObject<UPCGGraph>(Graph, GetTransientPackage()));
    if (!Staged.IsValid())
    {
        OutError = TEXT("Could not stage the PCG graph.");
        return false;
    }
    Staged->SetFlags(RF_Transient);
    UPCGNode* StagedNode = FindPCGNode(Staged.Get(), Node->GetName());
    UPCGSettings* Settings = StagedNode ? StagedNode->GetSettings() : nullptr;
    if (!Settings || Settings->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
    {
        OutError = TEXT("These settings cannot be safely prevalidated without executing custom Blueprint callbacks.");
        return false;
    }
    auto Before = MakeShared<FJsonObject>();
    Before->SetArrayField(TEXT("edges"), ReadPCGGraph(Staged.Get(), false)->GetArrayField(TEXT("edges")));
    int32 Count = 0;
    if (!ApplyPCGSettingsPayload(Settings, SubAction, Payload, OutError, Count)) return false;
    StagedNode->UpdateAfterSettingsChangeDuringCreation();
    Settings->PostEditChange();
    auto After = MakeShared<FJsonObject>();
    After->SetArrayField(TEXT("edges"), ReadPCGGraph(Staged.Get(), false)->GetArrayField(TEXT("edges")));
    FString BeforeJson, AfterJson;
    if (!McpCanonicalJsonObject(Before, BeforeJson, true) || !McpCanonicalJsonObject(After, AfterJson, true) || BeforeJson != AfterJson)
    {
        OutError = TEXT("The settings change would remove or replace existing PCG connections; disconnect them explicitly first.");
        return false;
    }
    if (GetPCGGraphRevision(Staged.Get()).IsEmpty())
    {
        OutError = TEXT("The staged graph exceeds the authored read budget or has invalid dependencies.");
        return false;
    }
    TSet<FString> PreviousDiagnostics;
    for (const auto& Value : ValidatePCGEdges(Graph))
    {
        FString Json;
        McpCanonicalJsonObject(Value->AsObject(), Json, true);
        PreviousDiagnostics.Add(Json);
    }
    for (const auto& Value : ValidatePCGEdges(Staged.Get()))
    {
        FString Json;
        McpCanonicalJsonObject(Value->AsObject(), Json, true);
        if (!PreviousDiagnostics.Contains(Json))
        {
            OutError = TEXT("The settings change would leave an incompatible PCG connection.");
            return false;
        }
    }
    return true;
}
}
#endif
