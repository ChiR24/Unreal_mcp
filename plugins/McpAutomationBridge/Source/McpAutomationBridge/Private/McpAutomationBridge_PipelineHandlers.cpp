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

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
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

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandlePipelineAction(
    const FString& RequestId, 
    const FString& Action, 
    const TSharedPtr<FJsonObject>& Payload, 
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

        // Construct UBT executable path
        // Location: Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe
        const FString UBTPath = FPaths::ConvertRelativePathToFull(
            FPaths::EngineDir() / TEXT("Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe"));
        
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
        TArray<TSharedPtr<FJsonValue>> Categories;

        // Core Actor & Asset Tools
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_actor")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_asset")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_blueprint")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_level")));

        // Editor & System Tools
        Categories.Add(MakeShared<FJsonValueString>(TEXT("control_editor")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("system_control")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_pipeline")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("inspect")));

        // Visual & Effects Tools
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_lighting")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_effect")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_material_authoring")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_texture")));

        // Animation & Physics Tools
        Categories.Add(MakeShared<FJsonValueString>(TEXT("animation_physics")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_skeleton")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_sequence")));

        // Audio Tools
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_audio")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_audio_authoring")));

        // Gameplay Tools
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_character")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_combat")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_inventory")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_interaction")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_gas")));

        // AI Tools
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_ai")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_behavior_tree")));

        // World Building Tools
        Categories.Add(MakeShared<FJsonValueString>(TEXT("build_environment")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_geometry")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_level_structure")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_volumes")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_navigation")));

        // UI Tools
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_widget_authoring")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_input")));

        // Networking & Multiplayer Tools
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_networking")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_sessions")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_game_framework")));

        // Performance Tools
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_performance")));

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetArrayField(TEXT("categories"), Categories);
        Result->SetNumberField(TEXT("count"), Categories.Num());

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Listed %d automation categories"), Categories.Num()), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // get_status: Return automation bridge status information
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("get_status"))
    {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

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
        Result->SetNumberField(TEXT("totalActions"), 1069);
        Result->SetNumberField(TEXT("toolCategories"), 35);

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


    // =========================================================
    // GET UBT LOG  (v8)
    // subAction="get_ubt_log"
    // Returns the last N lines of the UnrealBuildTool Log.txt.
    // This is the primary source of compile errors from Live
    // Coding and full rebuilds. Optional param: "lines" (int,
    // default 100). Optional param: "filter" (string, filters
    // to lines containing this substring).
    // =========================================================
    if (SubAction == TEXT("get_ubt_log")) {
        int32 MaxLines = 100;
        FString Filter;
        if (Payload->HasField(TEXT("lines"))) {
            double LinesVal = 0.0;
            Payload->TryGetNumberField(TEXT("lines"), LinesVal);
            MaxLines = FMath::Clamp((int32)LinesVal, 1, 2000);
        }
        Payload->TryGetStringField(TEXT("filter"), Filter);

        // UBT log is always at %LOCALAPPDATA%\UnrealBuildTool\Log.txt
        FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
        FString UbtLogPath = FPaths::Combine(LocalAppData,
            TEXT("UnrealBuildTool"), TEXT("Log.txt"));

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("logPath"), UbtLogPath);

        if (!FPaths::FileExists(UbtLogPath)) {
            Result->SetStringField(TEXT("error"),
                FString::Printf(TEXT("UBT log not found at: %s"), *UbtLogPath));
            Result->SetArrayField(TEXT("lines"), TArray<TSharedPtr<FJsonValue>>());
            SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("UBT log not found."), Result);
            return true;
        }

        FString LogContent;
        FFileHelper::LoadFileToString(LogContent, *UbtLogPath);

        TArray<FString> AllLines;
        LogContent.ParseIntoArrayLines(AllLines);

        // Apply filter if provided
        TArray<FString> FilteredLines;
        for (const FString& Line : AllLines) {
            if (Filter.IsEmpty() || Line.Contains(Filter, ESearchCase::IgnoreCase)) {
                FilteredLines.Add(Line);
            }
        }

        // Take last MaxLines
        int32 StartIdx = FMath::Max(0, FilteredLines.Num() - MaxLines);
        TArray<TSharedPtr<FJsonValue>> LineArray;
        for (int32 i = StartIdx; i < FilteredLines.Num(); i++) {
            LineArray.Add(MakeShared<FJsonValueString>(FilteredLines[i]));
        }

        Result->SetArrayField(TEXT("lines"), LineArray);
        Result->SetNumberField(TEXT("totalLines"), FilteredLines.Num());
        Result->SetNumberField(TEXT("returnedLines"), LineArray.Num());
        Result->SetBoolField(TEXT("filtered"), !Filter.IsEmpty());

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("UBT log retrieved."), Result);
        return true;
    }

    // =========================================================
    // GET LIVE CODING LOG  (v8)
    // subAction="get_livecoding_log"
    // Returns the last N lines of the main editor log filtered
    // to LiveCoding entries only — this is where Live Coding
    // compile errors appear. Also returns UBT log tail for
    // RulesError / module-not-found errors.
    // Optional param: "lines" (int, default 80).
    // =========================================================
    if (SubAction == TEXT("get_livecoding_log")) {
        int32 MaxLines = 80;
        if (Payload->HasField(TEXT("lines"))) {
            double LinesVal = 0.0;
            Payload->TryGetNumberField(TEXT("lines"), LinesVal);
            MaxLines = FMath::Clamp((int32)LinesVal, 1, 2000);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

        // ---- Section 1: Editor log — LiveCoding entries ----
        FString ProjectDir = FPaths::ProjectDir();
        FString ProjectName = FApp::GetProjectName();
        FString EditorLogPath = FPaths::Combine(
            ProjectDir, TEXT("Saved"), TEXT("Logs"),
            ProjectName + TEXT(".log"));

        TArray<TSharedPtr<FJsonValue>> LcLines;
        if (FPaths::FileExists(EditorLogPath)) {
            FString LogContent;
            FFileHelper::LoadFileToString(LogContent, *EditorLogPath);
            TArray<FString> AllLines;
            LogContent.ParseIntoArrayLines(AllLines);

            // Collect lines mentioning LiveCoding or live compile events
            TArray<FString> LcFiltered;
            for (const FString& Line : AllLines) {
                if (Line.Contains(TEXT("LiveCoding"), ESearchCase::IgnoreCase) ||
                    Line.Contains(TEXT("Live coding"), ESearchCase::IgnoreCase) ||
                    Line.Contains(TEXT("Live Coding"), ESearchCase::IgnoreCase)) {
                    LcFiltered.Add(Line);
                }
            }
            int32 StartIdx = FMath::Max(0, LcFiltered.Num() - MaxLines);
            for (int32 i = StartIdx; i < LcFiltered.Num(); i++) {
                LcLines.Add(MakeShared<FJsonValueString>(LcFiltered[i]));
            }
        }
        Result->SetArrayField(TEXT("liveCodingLines"), LcLines);
        Result->SetStringField(TEXT("editorLogPath"), EditorLogPath);

        // ---- Section 2: UBT log — last MaxLines/2 lines ----
        // RulesErrors and module-not-found errors appear only here.
        FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
        FString UbtLogPath = FPaths::Combine(LocalAppData,
            TEXT("UnrealBuildTool"), TEXT("Log.txt"));
        TArray<TSharedPtr<FJsonValue>> UbtLines;
        if (FPaths::FileExists(UbtLogPath)) {
            FString UbtContent;
            FFileHelper::LoadFileToString(UbtContent, *UbtLogPath);
            TArray<FString> AllUbtLines;
            UbtContent.ParseIntoArrayLines(AllUbtLines);
            int32 UbtMax = FMath::Max(1, MaxLines / 2);
            int32 StartIdx = FMath::Max(0, AllUbtLines.Num() - UbtMax);
            for (int32 i = StartIdx; i < AllUbtLines.Num(); i++) {
                UbtLines.Add(MakeShared<FJsonValueString>(AllUbtLines[i]));
            }
        }
        Result->SetArrayField(TEXT("ubtLogLines"), UbtLines);
        Result->SetStringField(TEXT("ubtLogPath"), UbtLogPath);

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Live Coding log retrieved."), Result);
        return true;
    }

        // Unknown subaction
    SendAutomationError(RequestingSocket, RequestId, 
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
