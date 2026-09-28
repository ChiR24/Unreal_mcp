// Copyright (c) 2024 MCP Automation Bridge Contributors
//
// maintain_content refresh_blueprints. Renaming a class leaves every node that names it as it was:
// a cast keeps its "AsBP Mario Save" pin, which compiles into the bytecode under that name, and
// cached node titles keep the old class. The editor's Refresh All Nodes fixes one Blueprint at a time.

#include "Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowBulkSelection.h"
#include "Domains/AssetWorkflow/Rename/McpAutomationBridge_AssetRenameGuard.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpAssetRename
{
bool HandleRefreshBlueprints(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TArray<FString> Paths;
    if (!McpCollectBulkAssetPaths(*Bridge, RequestId, Socket, Payload, Paths))
    {
        return true;
    }
    int32 Refreshed = 0;
    int32 Saved = 0;
    TArray<TSharedPtr<FJsonValue>> Broken;
    for (const FString& Path : Paths)
    {
        const FString Safe = SanitizeProjectRelativePath(Path);
        // Widget and Animation Blueprints are Blueprints too; every other asset has no nodes.
        UBlueprint* Blueprint = Safe.IsEmpty() ? nullptr : Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(Safe));
        if (!Blueprint)
        {
            continue;
        }
        FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
        FString FirstError;
        const bool bCompiled = McpCompileBlueprintWithDiagnostics(Blueprint, MakeShared<FJsonObject>(), FirstError);
        Saved += SaveLoadedAssetThrottled(Blueprint, true) ? 1 : 0;
        ++Refreshed;
        if (!bCompiled)
        {
            Broken.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s: %s"), *Blueprint->GetPathName(),
                                                                    FirstError.IsEmpty() ? TEXT("does not compile") : *FirstError)));
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("refreshedCount"), Refreshed);
    Result->SetNumberField(TEXT("savedCount"), Saved);
    Result->SetArrayField(TEXT("compileErrors"), Broken);
    if (Refreshed == 0)
    {
        Bridge->SendAutomationResponse(Socket, RequestId, false, TEXT("No Blueprints among the given assets"), Result,
                                       TEXT("NO_BLUEPRINTS"));
        return true;
    }
    const bool bClean = Broken.Num() == 0;
    Result->SetBoolField(TEXT("success"), bClean);
    Bridge->SendAutomationResponse(
        Socket, RequestId, bClean,
        bClean ? FString::Printf(TEXT("Refreshed, compiled and saved %d Blueprint(s)"), Refreshed)
               : FString::Printf(TEXT("Refreshed %d Blueprint(s); %d do not compile (listed under compileErrors, saved anyway)"),
                                 Refreshed, Broken.Num()),
        Result, bClean ? FString() : TEXT("COMPILE_FAILED"));
    return true;
}
} // namespace McpAssetRename
