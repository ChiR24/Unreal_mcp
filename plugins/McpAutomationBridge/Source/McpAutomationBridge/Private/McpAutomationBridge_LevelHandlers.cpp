#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Editor.h"
#include "FileHelpers.h"
#include "LevelEditor.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "RenderingThread.h"

// Check for LevelEditorSubsystem
#if defined(__has_include)
#  if __has_include("Subsystems/LevelEditorSubsystem.h")
#    include "Subsystems/LevelEditorSubsystem.h"
#    define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#  elif __has_include("LevelEditorSubsystem.h")
#    include "LevelEditorSubsystem.h"
#    define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#  else
#    define MCP_HAS_LEVELEDITOR_SUBSYSTEM 0
#  endif
#else
#  define MCP_HAS_LEVELEDITOR_SUBSYSTEM 0
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleLevelAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    const bool bIsLevelAction = (
        Lower == TEXT("manage_level") ||
        Lower == TEXT("save_current_level") ||
        Lower == TEXT("create_new_level") ||
        Lower == TEXT("stream_level") ||
        Lower == TEXT("spawn_light") ||
        Lower == TEXT("build_lighting") ||
        Lower == TEXT("bake_lightmap")
    );
    if (!bIsLevelAction) return false;

    FString EffectiveAction = Lower;
    
    // Unpack manage_level
    if (Lower == TEXT("manage_level"))
    {
        if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("manage_level payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
        FString SubAction; Payload->TryGetStringField(TEXT("action"), SubAction);
        const FString LowerSub = SubAction.ToLower();
        
        if (LowerSub == TEXT("load") || LowerSub == TEXT("load_level"))
        {
            // Map to Open command
            FString LevelPath; Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
            if (LevelPath.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("levelPath required"), TEXT("INVALID_ARGUMENT")); return true; }
            const FString Cmd = FString::Printf(TEXT("Open %s"), *LevelPath);
            TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("command"), Cmd);
            return HandleExecuteEditorFunction(RequestId, TEXT("execute_console_command"), P, RequestingSocket);
        }
        else if (LowerSub == TEXT("save"))
        {
            EffectiveAction = TEXT("save_current_level");
        }
        else if (LowerSub == TEXT("save_as") || LowerSub == TEXT("save_level_as"))
        {
            EffectiveAction = TEXT("save_level_as");
        }
        else if (LowerSub == TEXT("create_level"))
        {
            EffectiveAction = TEXT("create_new_level");
        }
        else if (LowerSub == TEXT("stream"))
        {
            EffectiveAction = TEXT("stream_level");
        }
        else if (LowerSub == TEXT("create_light"))
        {
            EffectiveAction = TEXT("spawn_light");
        }
        else
        {
             SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown manage_level action: %s"), *SubAction), TEXT("UNKNOWN_ACTION"));
             return true;
        }
    }

#if WITH_EDITOR
    if (EffectiveAction == TEXT("save_current_level"))
    {
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("functionName"), TEXT("SAVE_CURRENT_LEVEL"));
        return HandleExecuteEditorFunction(RequestId, TEXT("execute_editor_function"), P, RequestingSocket);
    }
    if (EffectiveAction == TEXT("save_level_as"))
    {
        // Force cleanup to prevent potential deadlocks with HLODs/WorldPartition during save
        if (GEditor)
        {
            FlushRenderingCommands();
            GEditor->ForceGarbageCollection(true);
            FlushRenderingCommands();
        }

        FString SavePath;
        if (Payload.IsValid()) Payload->TryGetStringField(TEXT("savePath"), SavePath);
        if (SavePath.IsEmpty()) {
             SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("savePath required for save_level_as"), nullptr, TEXT("INVALID_ARGUMENT"));
             return true;
        }

#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
        if (ULevelEditorSubsystem* LevelEditorSS = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
        {
             bool bSaved = false;
#if __has_include("FileHelpers.h")
             if (UWorld* World = GEditor->GetEditorWorldContext().World())
             {
                 bSaved = FEditorFileUtils::SaveMap(World, SavePath);
             }
#endif
             if (bSaved)
             {
                 TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                 Resp->SetStringField(TEXT("levelPath"), SavePath);
                 SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Level saved as %s"), *SavePath), Resp, FString());
             }
             else
             {
                 SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to save level as"), nullptr, TEXT("SAVE_FAILED"));
             }
             return true;
        }
#endif
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("LevelEditorSubsystem not available"), nullptr, TEXT("SUBSYSTEM_MISSING"));
        return true;
    }
    if (EffectiveAction == TEXT("build_lighting") || EffectiveAction == TEXT("bake_lightmap"))
    {
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("functionName"), TEXT("BUILD_LIGHTING"));
        if (Payload.IsValid()) { 
            FString Q; 
            if (Payload->TryGetStringField(TEXT("quality"), Q) && !Q.IsEmpty()) 
                P->SetStringField(TEXT("quality"), Q); 
        }
        return HandleExecuteEditorFunction(RequestId, TEXT("execute_editor_function"), P, RequestingSocket);
    }
    if (EffectiveAction == TEXT("create_new_level"))
    {
        FString LevelName;
        if (Payload.IsValid()) Payload->TryGetStringField(TEXT("levelName"), LevelName);
        
        FString LevelPath; 
        if (Payload.IsValid()) Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
        
        // Construct valid package path
        FString SavePath = LevelPath;
        if (SavePath.IsEmpty() && !LevelName.IsEmpty())
        {
            if (LevelName.StartsWith(TEXT("/"))) 
                SavePath = LevelName;
            else
                SavePath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelName);
        }
        
        if (SavePath.IsEmpty())
        {
             SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("levelName or levelPath required for create_level"), nullptr, TEXT("INVALID_ARGUMENT"));
             return true;
        }

        // Check if map already exists
        if (FPackageName::DoesPackageExist(SavePath))
        {
             // If exists, just open it
             const FString Cmd = FString::Printf(TEXT("Open %s"), *SavePath);
             TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("command"), Cmd);
             return HandleExecuteEditorFunction(RequestId, TEXT("execute_console_command"), P, RequestingSocket);
        }

        // Create new map
#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM) && __has_include("FileHelpers.h")
        if (GEditor->IsPlaySessionInProgress())
        {
             GEditor->RequestEndPlayMap();
             SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Cannot create level while Play In Editor is active."), nullptr, TEXT("PIE_ACTIVE"));
             return true;
        }

        // Force cleanup of previous world/resources to prevent RenderCore/Driver crashes (monza/D3D12)
        // especially when tests run back-to-back triggering thumbnail generation or world partition shutdown.
        if (GEditor)
        {
            FlushRenderingCommands();
            GEditor->ForceGarbageCollection(true);
            FlushRenderingCommands();
        }

        if (UWorld* NewWorld = GEditor->NewMap(true)) // true = force new map (creates untitled)
        {
             GEditor->GetEditorWorldContext().SetCurrentWorld(NewWorld);
             
             // Save it to valid path
             if (FEditorFileUtils::SaveMap(NewWorld, SavePath))
             {
                 TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); 
                 Resp->SetStringField(TEXT("levelPath"), SavePath);
                 SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Level created: %s"), *SavePath), Resp, FString());
             }
             else
             {
                 SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to save new level"), nullptr, TEXT("SAVE_FAILED"));
             }
        }
        else
        {
             SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create new map"), nullptr, TEXT("CREATION_FAILED"));
        }
        return true;
#else
        // Fallback for missing headers (shouldn't happen given build.cs)
        const FString Cmd = FString::Printf(TEXT("Open %s"), *SavePath);
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("command"), Cmd);
        return HandleExecuteEditorFunction(RequestId, TEXT("execute_console_command"), P, RequestingSocket);
#endif
    }
    if (EffectiveAction == TEXT("stream_level"))
    {
        FString LevelName; bool bLoad = true; bool bVis = true;
        if (Payload.IsValid()) { 
            Payload->TryGetStringField(TEXT("levelName"), LevelName); 
            Payload->TryGetBoolField(TEXT("shouldBeLoaded"), bLoad); 
            Payload->TryGetBoolField(TEXT("shouldBeVisible"), bVis); 
            if (LevelName.IsEmpty()) 
                Payload->TryGetStringField(TEXT("levelPath"), LevelName); 
        }
        if (LevelName.TrimStartAndEnd().IsEmpty()) { 
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("stream_level requires levelName or levelPath"), nullptr, TEXT("INVALID_ARGUMENT")); 
            return true; 
        }
        const FString Cmd = FString::Printf(TEXT("StreamLevel %s %s %s"), *LevelName, bLoad ? TEXT("Load") : TEXT("Unload"), bVis ? TEXT("Show") : TEXT("Hide"));
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("command"), Cmd);
        return HandleExecuteEditorFunction(RequestId, TEXT("execute_console_command"), P, RequestingSocket);
    }
    if (EffectiveAction == TEXT("spawn_light"))
    {
        FString LightType = TEXT("Point"); 
        if (Payload.IsValid()) 
            Payload->TryGetStringField(TEXT("lightType"), LightType);
        const FString LT = LightType.ToLower();
        FString ClassName;
        if (LT == TEXT("directional")) ClassName = TEXT("DirectionalLight");
        else if (LT == TEXT("spot")) ClassName = TEXT("SpotLight");
        else if (LT == TEXT("rect")) ClassName = TEXT("RectLight");
        else ClassName = TEXT("PointLight");
        TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
        if (Payload.IsValid())
        {
            const TSharedPtr<FJsonObject>* L = nullptr;
            if (Payload->TryGetObjectField(TEXT("location"), L) && L && (*L).IsValid()) Params->SetObjectField(TEXT("location"), *L);
            const TSharedPtr<FJsonObject>* R = nullptr;
            if (Payload->TryGetObjectField(TEXT("rotation"), R) && R && (*R).IsValid()) Params->SetObjectField(TEXT("rotation"), *R);
        }
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("functionName"), TEXT("SPAWN_ACTOR_AT_LOCATION"));
        P->SetStringField(TEXT("class_path"), ClassName);
        P->SetObjectField(TEXT("params"), Params.ToSharedRef());
        return HandleExecuteEditorFunction(RequestId, TEXT("execute_editor_function"), P, RequestingSocket);
    }
    return false;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Level actions require editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
