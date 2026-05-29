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

    // Extract subaction. Native MCP clients send the public tool action in
    // "action"; TS bridge fallback sends the internal "subAction" field.
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SubAction = GetJsonStringField(Payload, TEXT("action"));
    }

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
        if (ExtraArgs.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("arguments"), ExtraArgs);
        }

        Target.TrimStartAndEndInline();
        Platform.TrimStartAndEndInline();
        Configuration.TrimStartAndEndInline();
        ExtraArgs.TrimStartAndEndInline();

        if (Target.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Target is required for run_ubt."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        if (Platform.IsEmpty())
        {
#if PLATFORM_WINDOWS
            Platform = TEXT("Win64");
#elif PLATFORM_MAC
            Platform = TEXT("Mac");
#else
            Platform = TEXT("Linux");
#endif
        }

        if (Configuration.IsEmpty())
        {
            Configuration = TEXT("Development");
        }

        auto ValidateBuildToken = [&](const FString& Value, const TCHAR* FieldName) -> bool {
            if (!McpIsSafeUbtPositionalToken(Value))
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Invalid %s for run_ubt: %s must be a positional token"), FieldName, *Value),
                    TEXT("INVALID_ARGUMENT"));
                return false;
            }
            return true;
        };

        if (!ValidateBuildToken(Target, TEXT("target")) ||
            !ValidateBuildToken(Platform, TEXT("platform")) ||
            !ValidateBuildToken(Configuration, TEXT("configuration")))
        {
            return true;
        }

        if (!McpIsAllowedUbtPlatform(Platform))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Platform is not allowed for run_ubt: %s"), *Platform),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        if (!McpIsAllowedUbtConfiguration(Configuration))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Configuration is not allowed for run_ubt: %s"), *Configuration),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        if (!McpIsSafeUbtArgumentList(ExtraArgs))
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("UBT arguments contain unsafe characters."),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Construct the platform build wrapper path. On Linux/macOS the UBT .exe
        // wrapper does not exist; Build.sh/Build.bat is the stable editor-owned
        // entry point across installed and source engine layouts.
#if PLATFORM_WINDOWS
        const FString UBTPath = FPaths::ConvertRelativePathToFull(
            FPaths::EngineDir() / TEXT("Build/BatchFiles/Build.bat"));
#elif PLATFORM_MAC
        const FString UBTPath = FPaths::ConvertRelativePathToFull(
            FPaths::EngineDir() / TEXT("Build/BatchFiles/Mac/Build.sh"));
#else
        const FString UBTPath = FPaths::ConvertRelativePathToFull(
            FPaths::EngineDir() / TEXT("Build/BatchFiles/Linux/Build.sh"));
#endif

        if (!FPaths::FileExists(UBTPath))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("UBT build wrapper not found at: %s"), *UBTPath),
                TEXT("UBT_NOT_FOUND"));
            return true;
        }

        FString ProjectPath = FPaths::GetProjectFilePath();
        FString ProjectArg;
        if (!ProjectPath.IsEmpty())
        {
            ProjectArg = FString::Printf(TEXT(" -Project=\"%s\""), *ProjectPath);
        }

        // Build command line
        const FString Params = FString::Printf(TEXT("%s %s %s%s %s"),
            *Target, *Platform, *Configuration, *ProjectArg, *ExtraArgs);

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
            Result->SetStringField(TEXT("action"), TEXT("manage_pipeline"));
            Result->SetStringField(TEXT("subAction"), TEXT("run_ubt"));
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

        // Canonical public MCP tools
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_tools")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_asset")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_blueprint")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("control_actor")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("control_editor")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_level")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("build_environment")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("animation_physics")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("system_control")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_sequence")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("inspect")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_audio")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_geometry")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_effect")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_gas")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_character")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_combat")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_ai")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_inventory")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_interaction")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_networking")));
        Categories.Add(MakeShared<FJsonValueString>(TEXT("manage_level_structure")));

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetArrayField(TEXT("categories"), Categories);
        Result->SetNumberField(TEXT("count"), Categories.Num());

        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Listed %d canonical MCP tools"), Categories.Num()), Result);
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
        Result->SetNumberField(TEXT("toolCategories"), 22);

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
