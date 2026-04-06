// =============================================================================
// McpAutomationBridge_PipelineHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Pipeline & Build Automation Handlers
//
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
//
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_pipeline
//   - run_ubt: Launch UnrealBuildTool process with target/platform/config
//   - list_categories: Return all available automation tool categories
//   - get_status: Return automation bridge status and version info
//
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: PlatformProcess, Paths, EngineVersion, App
//
// Notes:
//   - UBT spawns as detached process; results logged separately
//   - Status includes engine version, platform, PIE state, project name
// =============================================================================

#include "McpVersionCompatibility.h" // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeToolCatalog.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "Misc/App.h"
#include "Kismet/GameplayStatics.h"
#include "Editor.h"

namespace
{
    /** @brief Contract: serializes one public subaction row from the canonical tool catalog. */
    TSharedPtr<FJsonObject> MakeSubActionJson(const FMcpAutomationBridgeToolSubActionCatalogEntry &Entry)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Entry.Name);
        Json->SetStringField(TEXT("summary"), Entry.Summary);
        Json->SetBoolField(TEXT("editorOnly"), Entry.bEditorOnly);
        Json->SetBoolField(TEXT("requiresLiveEditor"), Entry.bRequiresLiveEditor);
        Json->SetBoolField(TEXT("requiresAssetEditor"), Entry.bRequiresAssetEditor);
        Json->SetStringField(TEXT("interactionModel"), Entry.InteractionModel);
        if (!Entry.LimitationNote.IsEmpty())
        {
            Json->SetStringField(TEXT("limitationNote"), Entry.LimitationNote);
        }
        return Json;
    }

    /** @brief Contract: serializes one public tool row from the canonical tool catalog. */
    TSharedPtr<FJsonObject> MakeToolJson(const FMcpAutomationBridgeToolCatalogEntry &Entry)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("toolName"), Entry.ToolName);
        Json->SetStringField(TEXT("category"), Entry.Category);
        Json->SetStringField(TEXT("summary"), Entry.Summary);
        Json->SetBoolField(TEXT("public"), Entry.bPublic);

        TArray<TSharedPtr<FJsonValue>> SubActions;
        for (const FMcpAutomationBridgeToolSubActionCatalogEntry &SubAction : Entry.SubActions)
        {
            SubActions.Add(MakeShared<FJsonValueObject>(MakeSubActionJson(SubAction)));
        }
        Json->SetArrayField(TEXT("subActions"), SubActions);
        Json->SetNumberField(TEXT("subActionCount"), Entry.SubActions.Num());
        return Json;
    }

    /** @brief Contract: builds backward-compatible tool-name, structured-tool, and category-group-name metadata from the public catalog. */
    void BuildCatalogResponse(
        TArray<TSharedPtr<FJsonValue>> &OutToolNames,
        TArray<TSharedPtr<FJsonValue>> &OutTools,
        TArray<TSharedPtr<FJsonValue>> &OutCategoryGroupNames,
        int32 &OutActionCount)
    {
        TSet<FString> SeenGroups;
        OutActionCount = 0;

        for (const FMcpAutomationBridgeToolCatalogEntry &Entry : GetPublicMcpAutomationBridgeToolCatalog())
        {
            OutToolNames.Add(MakeShared<FJsonValueString>(Entry.ToolName));
            OutTools.Add(MakeShared<FJsonValueObject>(MakeToolJson(Entry)));

            if (!SeenGroups.Contains(Entry.Category))
            {
                SeenGroups.Add(Entry.Category);
                OutCategoryGroupNames.Add(MakeShared<FJsonValueString>(Entry.Category));
            }

            OutActionCount += Entry.SubActions.Num() > 0 ? Entry.SubActions.Num() : 1;
        }
    }
}

// =============================================================================
// Handler Implementation
// =============================================================================

/** @brief Contract: serves the catalog-backed manage_pipeline surface while preserving the existing response keys used by the local MCP wrapper.
 * categoryGroupNames is the canonical array field, groupCount is the canonical numeric count field,
 * and categoryGroups remains a legacy alias whose shape differs by subaction for backward compatibility.
 */
bool UMcpAutomationBridgeSubsystem::HandlePipelineAction(
    const FString &RequestId,
    const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Validate action
    if (Action != TEXT("manage_pipeline"))
    {
        return false;
    }

    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Extract subaction
    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    // -------------------------------------------------------------------------
    // run_ubt: Launch UnrealBuildTool process
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("run_ubt"))
    {
        FString Target;
        Payload->TryGetStringField(TEXT("target"), Target);

        FString Platform;
        Payload->TryGetStringField(TEXT("platform"), Platform);

        FString Configuration;
        Payload->TryGetStringField(TEXT("configuration"), Configuration);

        FString ExtraArgs;
        Payload->TryGetStringField(TEXT("extraArgs"), ExtraArgs);

        // Construct the platform-specific UBT executable path.
        const FString UBTExecutableName =
#if PLATFORM_WINDOWS
            TEXT("UnrealBuildTool.exe");
#else
            TEXT("UnrealBuildTool");
#endif
        const FString UBTPath = FPaths::ConvertRelativePathToFull(
            FPaths::EngineDir() / TEXT("Binaries/DotNET/UnrealBuildTool") / UBTExecutableName);

        // Build command line
        const FString Params = FString::Printf(TEXT("%s %s %s %s"),
                                               *Target, *Platform, *Configuration, *ExtraArgs);

        // Spawn UBT as detached process
        FProcHandle ProcHandle = FPlatformProcess::CreateProc(
            *UBTPath,
            *Params,
            true,    // bLaunchDetached
            false,   // bLaunchHidden
            false,   // bLaunchReallyHidden
            nullptr, // ProcessID
            0,       // PriorityModifier
            nullptr, // OptionalWorkingDirectory
            nullptr  // PipeWriteChild
        );

        if (ProcHandle.IsValid())
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("action"), TEXT("run_ubt"));
            Result->SetStringField(TEXT("target"), Target);
            Result->SetStringField(TEXT("platform"), Platform);
            Result->SetStringField(TEXT("configuration"), Configuration);
            Result->SetBoolField(TEXT("processStarted"), true);

            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("UBT process started."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Failed to launch UBT."), TEXT("LAUNCH_FAILED"));
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // list_categories: Return all available automation tool categories
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("list_categories"))
    {
        TArray<TSharedPtr<FJsonValue>> ToolNames;
        TArray<TSharedPtr<FJsonValue>> Tools;
        TArray<TSharedPtr<FJsonValue>> CategoryGroupNames;
        int32 ActionCount = 0;
        BuildCatalogResponse(ToolNames, Tools, CategoryGroupNames, ActionCount);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetArrayField(TEXT("categories"), ToolNames);
        Result->SetArrayField(TEXT("tools"), Tools);
        Result->SetArrayField(TEXT("categoryGroups"), CategoryGroupNames);
        Result->SetArrayField(TEXT("categoryGroupNames"), CategoryGroupNames);
        Result->SetNumberField(TEXT("count"), ToolNames.Num());
        Result->SetNumberField(TEXT("groupCount"), CategoryGroupNames.Num());
        Result->SetNumberField(TEXT("actionCount"), ActionCount);
        Result->SetStringField(TEXT("catalogSource"), TEXT("McpAutomationBridgeToolCatalog"));

        SendAutomationResponse(RequestingSocket, RequestId, true,
                               FString::Printf(TEXT("Listed %d public MCP tools from the bridge catalog"), ToolNames.Num()), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // get_status: Return automation bridge status information
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("get_status"))
    {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        TArray<TSharedPtr<FJsonValue>> ToolNames;
        TArray<TSharedPtr<FJsonValue>> Tools;
        TArray<TSharedPtr<FJsonValue>> CategoryGroupNames;
        int32 ActionCount = 0;
        BuildCatalogResponse(ToolNames, Tools, CategoryGroupNames, ActionCount);

        // Connection status
        Result->SetBoolField(TEXT("connected"), true);
        Result->SetStringField(TEXT("bridgeType"), TEXT("Native C++ WebSocket"));

        // Version info
        Result->SetStringField(TEXT("version"), TEXT("1.0.0"));
        Result->SetStringField(TEXT("engineVersion"), *FEngineVersion::Current().ToString());
        Result->SetNumberField(TEXT("engineMajor"), ENGINE_MAJOR_VERSION);
        Result->SetNumberField(TEXT("engineMinor"), ENGINE_MINOR_VERSION);

        // Capability flags
#if WITH_EDITOR
        Result->SetBoolField(TEXT("editorMode"), true);
#else
        Result->SetBoolField(TEXT("editorMode"), false);
#endif

        // Action statistics
        Result->SetNumberField(TEXT("totalActions"), ActionCount);
        Result->SetNumberField(TEXT("toolCategories"), ToolNames.Num());
        Result->SetNumberField(TEXT("categoryGroups"), CategoryGroupNames.Num());
        Result->SetNumberField(TEXT("groupCount"), CategoryGroupNames.Num());
        Result->SetStringField(TEXT("catalogSource"), TEXT("McpAutomationBridgeToolCatalog"));
        Result->SetArrayField(TEXT("categories"), ToolNames);
        Result->SetArrayField(TEXT("categoryGroupNames"), CategoryGroupNames);
        Result->SetArrayField(TEXT("tools"), Tools);

        // Runtime info
        Result->SetStringField(TEXT("platform"), *UGameplayStatics::GetPlatformName());
        Result->SetBoolField(TEXT("isPlayInEditor"),
                             GEditor ? GEditor->IsPlaySessionInProgress() : false);

        // Project info
        Result->SetStringField(TEXT("projectName"), FApp::GetProjectName());

        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Automation bridge status retrieved"), Result);
        return true;
    }

    // Unknown subaction
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
