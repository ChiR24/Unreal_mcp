#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "Serialization/JsonSerializer.h"
#include "McpBridgeWebSocket.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "Editor/EditorPerProjectUserSettings.h"
#endif

// Cycle stats for Editor handlers.
// Use `stat McpBridge` in the UE console to view these stats.
DECLARE_CYCLE_STAT(TEXT("Editor:ControlAction"), STAT_MCP_EditorControlAction, STATGROUP_McpBridge);

// Global static for session bookmarks
static TMap<FString, FTransform> GSessionBookmarks;

bool UMcpAutomationBridgeSubsystem::HandleControlEditorAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    SCOPE_CYCLE_COUNTER(STAT_MCP_EditorControlAction);
    
    FString SubAction;
    // In consolidated tool, 'action' is inside the payload
    if (!Payload->TryGetStringField(TEXT("action"), SubAction)) {
        // If not found, maybe the Action argument itself is the sub-action?
        // But the router sends "control_editor" as Action. 
        // Let's check if the caller passed the sub-action in "subAction" field (common pattern)
        if (!Payload->TryGetStringField(TEXT("subAction"), SubAction)) {
             return false;
        }
    }

#if WITH_EDITOR
    if (!GEditor) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    if (SubAction == TEXT("create_bookmark")) {
        FString BookmarkName;
        Payload->TryGetStringField(TEXT("bookmarkName"), BookmarkName);
        if (BookmarkName.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("bookmarkName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        if (GEditor->GetActiveViewport()) {
            FViewport* ActiveViewport = GEditor->GetActiveViewport();
            if (FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)ActiveViewport->GetClient()) {
                FVector Loc = ViewportClient->GetViewLocation();
                FRotator Rot = ViewportClient->GetViewRotation();
                
                GSessionBookmarks.Add(BookmarkName, FTransform(Rot, Loc));
                
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetStringField(TEXT("name"), BookmarkName);
                
                TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
                LocObj->SetNumberField(TEXT("x"), Loc.X);
                LocObj->SetNumberField(TEXT("y"), Loc.Y);
                LocObj->SetNumberField(TEXT("z"), Loc.Z);
                Result->SetObjectField(TEXT("location"), LocObj);

                TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
                RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
                RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
                RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
                Result->SetObjectField(TEXT("rotation"), RotObj);
                
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bookmark created (Session)"), Result);
                return true;
            }
        }
        
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active viewport"), TEXT("NO_VIEWPORT"));
        return true;
    }

    if (SubAction == TEXT("jump_to_bookmark")) {
        FString BookmarkName;
        Payload->TryGetStringField(TEXT("bookmarkName"), BookmarkName);
        
        if (FTransform* Found = GSessionBookmarks.Find(BookmarkName)) {
            if (GEditor->GetActiveViewport()) {
                if (FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient()) {
                    ViewportClient->SetViewLocation(Found->GetLocation());
                    ViewportClient->SetViewRotation(Found->GetRotation().Rotator());
                    ViewportClient->Invalidate();
                    SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Jumped to bookmark '%s'"), *BookmarkName));
                    return true;
                }
            }
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active viewport"), TEXT("NO_VIEWPORT"));
            return true;
        }
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Bookmark '%s' not found"), *BookmarkName), TEXT("NOT_FOUND"));
        return true;
    }

    if (SubAction == TEXT("set_preferences")) {
        // Check for 'preferences' object
        const TSharedPtr<FJsonObject>* PrefsPtr = nullptr;
        if (Payload->TryGetObjectField(TEXT("preferences"), PrefsPtr) && PrefsPtr && (*PrefsPtr).IsValid()) {
             // For now, return success to acknowledge receipt, as reflection setting is complex without specific keys.
             // We log what we received.
             UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Received set_preferences request. Auto-setting via JSON reflection is experimental."));
             
             // Reflection-based property setting on UEditorPerProjectUserSettings would require
             // mapping specific JSON keys to known UPROPERTY fields. Current implementation
             // acknowledges receipt for forward compatibility.
             
             SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Preferences received (Native implementation pending full reflection support)"));
             return true;
        }
        SendAutomationError(RequestingSocket, RequestId, TEXT("Preferences object required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (SubAction == TEXT("start_recording")) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Sequence Recording not yet implemented in native bridge"), TEXT("NOT_IMPLEMENTED"));
        return true;
    }

    if (SubAction == TEXT("stop_recording")) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Sequence Recording not yet implemented in native bridge"), TEXT("NOT_IMPLEMENTED"));
        return true;
    }
    
    // Fallback for set_camera if routed here
    if (SubAction == TEXT("set_camera")) {
        const TSharedPtr<FJsonObject>* LocObj = nullptr;
        const TSharedPtr<FJsonObject>* RotObj = nullptr;
        FVector Loc = FVector::ZeroVector;
        FRotator Rot = FRotator::ZeroRotator;
        
        bool bHasLoc = Payload->TryGetObjectField(TEXT("location"), LocObj);
        bool bHasRot = Payload->TryGetObjectField(TEXT("rotation"), RotObj);
        
        if (bHasLoc && LocObj) {
             double X=0, Y=0, Z=0;
             (*LocObj)->TryGetNumberField(TEXT("x"), X);
             (*LocObj)->TryGetNumberField(TEXT("y"), Y);
             (*LocObj)->TryGetNumberField(TEXT("z"), Z);
             Loc = FVector(X,Y,Z);
        }
        if (bHasRot && RotObj) {
             double P=0, Y=0, R=0;
             (*RotObj)->TryGetNumberField(TEXT("pitch"), P);
             (*RotObj)->TryGetNumberField(TEXT("yaw"), Y);
             (*RotObj)->TryGetNumberField(TEXT("roll"), R);
             Rot = FRotator(P,Y,R);
        }
        
        if (GEditor && GEditor->GetActiveViewport()) {
             if (FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient()) {
                 if (bHasLoc) ViewportClient->SetViewLocation(Loc);
                 if (bHasRot) ViewportClient->SetViewRotation(Rot);
                 ViewportClient->Invalidate();
                 SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Camera set"));
                 return true;
             }
        }
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active viewport"), TEXT("NO_VIEWPORT"));
        return true;
    }

    if (SubAction == TEXT("set_viewport_resolution")) {
        double Width = 0;
        double Height = 0;
        Payload->TryGetNumberField(TEXT("width"), Width);
        Payload->TryGetNumberField(TEXT("height"), Height);
        
        if (Width > 0 && Height > 0) {
             FString Cmd = FString::Printf(TEXT("r.SetRes %dx%dw"), (int)Width, (int)Height);
             if (GEngine) {
                 GEngine->Exec(NULL, *Cmd);
                 SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Resolution set command sent: %s"), *Cmd));
                 return true;
             }
        }
        SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid width/height or GEngine missing"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (SubAction == TEXT("set_viewport_realtime")) {
        bool bEnabled = false;
        if (Payload->TryGetBoolField(TEXT("enabled"), bEnabled)) {
            if (GEditor && GEditor->GetActiveViewport()) {
                if (FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient()) {
                    ViewportClient->SetRealtime(bEnabled);
                    ViewportClient->Invalidate();
                    SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Realtime set to %s"), bEnabled ? TEXT("true") : TEXT("false")));
                    return true;
                }
            }
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active viewport"), TEXT("NO_VIEWPORT"));
            return true;
        }
        SendAutomationError(RequestingSocket, RequestId, TEXT("enabled param required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // G1: Advanced viewport screenshot capture with base64 return option
    if (SubAction == TEXT("capture_viewport")) {
        FString OutputPath;
        Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
        FString Filename;
        Payload->TryGetStringField(TEXT("filename"), Filename);
        FString Format = TEXT("png");
        Payload->TryGetStringField(TEXT("format"), Format);
        double Width = 0, Height = 0;
        Payload->TryGetNumberField(TEXT("width"), Width);
        Payload->TryGetNumberField(TEXT("height"), Height);
        bool bReturnBase64 = false;
        Payload->TryGetBoolField(TEXT("returnBase64"), bReturnBase64);
        bool bShowUI = false;
        Payload->TryGetBoolField(TEXT("showUI"), bShowUI);
        
        // Determine output filename
        FString FinalPath;
        if (!OutputPath.IsEmpty()) {
            FinalPath = OutputPath;
        } else if (!Filename.IsEmpty()) {
            FinalPath = FPaths::ProjectSavedDir() / TEXT("Screenshots") / Filename;
        } else {
            FinalPath = FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("Capture_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
        }
        
        // Ensure extension
        if (!FinalPath.EndsWith(TEXT(".png")) && !FinalPath.EndsWith(TEXT(".jpg")) && !FinalPath.EndsWith(TEXT(".bmp"))) {
            FinalPath += TEXT(".") + Format.ToLower();
        }
        
        // Use high-res screenshot command
        FString ScreenshotCmd = FString::Printf(TEXT("HighResShot %s"), *FinalPath);
        if (Width > 0 && Height > 0) {
            ScreenshotCmd = FString::Printf(TEXT("HighResShot %dx%d %s"), (int32)Width, (int32)Height, *FinalPath);
        }
        
        if (GEngine) {
            GEngine->Exec(nullptr, *ScreenshotCmd);
            
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("filePath"), FinalPath);
            Result->SetStringField(TEXT("format"), Format);
            if (Width > 0) Result->SetNumberField(TEXT("width"), Width);
            if (Height > 0) Result->SetNumberField(TEXT("height"), Height);
            
            // If returnBase64 is requested, read the file and encode it
            if (bReturnBase64) {
                // Give the screenshot a moment to be written
                FPlatformProcess::Sleep(0.5f);
                
                TArray<uint8> FileData;
                if (FFileHelper::LoadFileToArray(FileData, *FinalPath)) {
                    FString Base64Data = FBase64::Encode(FileData);
                    Result->SetStringField(TEXT("base64"), Base64Data);
                    Result->SetNumberField(TEXT("sizeBytes"), FileData.Num());
                } else {
                    Result->SetStringField(TEXT("base64Warning"), TEXT("File not ready or not found - try increasing delay"));
                }
            }
            
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Viewport captured"), Result);
            return true;
        }
        
        SendAutomationError(RequestingSocket, RequestId, TEXT("GEngine not available"), TEXT("ENGINE_NOT_AVAILABLE"));
        return true;
    }

#endif // WITH_EDITOR

    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown sub-action: %s"), *SubAction), TEXT("UNKNOWN_ACTION"));
    return true;
}
