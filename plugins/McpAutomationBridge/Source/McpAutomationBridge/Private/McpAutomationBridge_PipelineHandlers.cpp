#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

// Helper macros for JSON field access
#define GetStringFieldPipe GetJsonStringField
#define GetNumberFieldPipe GetJsonNumberField
#define GetBoolFieldPipe GetJsonBoolField

#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "Misc/App.h"
#include "Kismet/GameplayStatics.h"
#include "Editor.h"

bool UMcpAutomationBridgeSubsystem::HandlePipelineAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_pipeline"))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetStringFieldPipe(Payload, TEXT("subAction"));

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

        // Construct UBT command line
        // Path to UBT... usually in Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe
        FString UBTPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe"));
        
        FString Params = FString::Printf(TEXT("%s %s %s %s"), *Target, *Platform, *Configuration, *ExtraArgs);

        // Spawn process
        FProcHandle ProcHandle = FPlatformProcess::CreateProc(
            *UBTPath,
            *Params,
            true, // bLaunchDetached
            false, // bLaunchHidden
            false, // bLaunchReallyHidden
            nullptr, // ProcessID
            0, // PriorityModifier
            nullptr, // OptionalWorkingDirectory
            nullptr // PipeWriteChild
        );

        if (ProcHandle.IsValid())
        {
             // We can't easily get the PID on all platforms from the handle immediately without more code,
             // but we know it started.
             TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
             Result->SetStringField(TEXT("action"), TEXT("run_ubt"));
             Result->SetStringField(TEXT("target"), Target);
             Result->SetStringField(TEXT("platform"), Platform);
             Result->SetStringField(TEXT("configuration"), Configuration);
             Result->SetBoolField(TEXT("processStarted"), true);
             SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("UBT process started."), Result);
        }
        else
        {
             SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to launch UBT."), TEXT("LAUNCH_FAILED"));
        }
        return true;
    }

    // ==========================================================================
    // list_categories - Return all available automation tool categories
    // ==========================================================================
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
        
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetArrayField(TEXT("categories"), Categories);
        Result->SetNumberField(TEXT("count"), Categories.Num());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Listed %d automation categories"), Categories.Num()), Result);
        return true;
    }

    // ==========================================================================
    // get_status - Return automation bridge status information
    // ==========================================================================
    if (SubAction == TEXT("get_status"))
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        
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
        Result->SetBoolField(TEXT("isPlayInEditor"), GEditor ? GEditor->IsPlaySessionInProgress() : false);
        
        // Project info
        Result->SetStringField(TEXT("projectName"), FApp::GetProjectName());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Automation bridge status retrieved"), Result);
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}

#undef GetStringFieldPipe
#undef GetNumberFieldPipe
#undef GetBoolFieldPipe

